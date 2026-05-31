
#include "ota_mower_updater.h"
#include "log.h"
#include <SPIFFS.h>

using namespace ArduMower::Modem::Ota;

void StatusMessage::marshal(const ArduinoJson::JsonObject &o) const
{
  o["progress"] = progress;
  
  // More detailed status based on progress ranges
  if (progress >= 100) {
    o["status"] = "success";
  } else if (progress >= 95) {
    o["status"] = "finishing";
  } else if (progress >= 10) {
    o["status"] = "flashing";
  } else if (progress >= 5) {
    o["status"] = "erasing";
  } else if (progress > 0) {
    o["status"] = "initializing";
  } else {
    o["status"] = "starting";
  }
}


#ifdef MOWER_TERMINAL
MowerUpdater::MowerUpdater(Terminal &terminal, HardwareSerial &mowerFirmwareSerial) : 
  _terminal(terminal), _serial(mowerFirmwareSerial), 
  firmwareWriter(FirmwareWriterSTM32(mowerFirmwareSerial))
{
}
#else
MowerUpdater::MowerUpdater(HardwareSerial &mowerFirmwareSerial) : 
  _serial(mowerFirmwareSerial), 
  firmwareWriter(FirmwareWriterSTM32(mowerFirmwareSerial))
{
}
#endif

void MowerUpdater::startUpdate(String filename, UpdateComplete updateComplete) 
{ 
    if (filename.isEmpty()) {
        Log(ERR, "MowerUpdater::startUpdate called with empty filename");
        updateComplete("error-invalid-filename");
        return;
    }
    
    if (!SPIFFS.exists(filename)) {
        Log(ERR, "MowerUpdater::startUpdate file does not exist: %s", filename.c_str());
        updateComplete("error-file-not-found");
        return;
    }
    
    _filename = filename;
    _updateComplete = updateComplete;
    _progress = 0; // Reset progress
    
    Log(INFO, "MowerUpdater::startUpdate initiating update for: %s", filename.c_str());
    
#ifdef MOWER_TERMINAL
  _terminal.suspend([this] {
    Log(INFO, "MowerUpdater::startUpdate terminal suspended successfully");
    this->setSerialPortReady(true);
  });
#else
  this->setSerialPortReady(true);
#endif
}

void MowerUpdater::addStatusHandler(StatusHandler handler)
{
  _statusHandler = handler;
}

void MowerUpdater::updateStatus(byte progress)
{
  // Ensure progress is within valid range
  if (progress > 100) progress = 100;
  
  // Always update on 0 and 100, or if progress increased by at least 5%
  if (((progress == 0 || progress == 100) && (progress != _progress)) || 
      (progress > _progress + 5)) 
  {
    _progress = progress;
    Log(INFO, "MowerUpdater progress: %d%%", progress);
    
    if (_statusHandler != NULL) {
      _statusHandler(_progress);
    }
  }
}

void MowerUpdater::loop()
{
  if (_serialPortReady && _filename != "") 
  {
    String result = handleFlash();
    _serialPortReady = false;
    _filename = "";
#ifdef MOWER_TERMINAL
  _terminal.resume();
#endif
    _updateComplete(result);
  }
}

void MowerUpdater::setSerialPortReady(bool serialPortReady) 
{ 
    Log(DBG, "MowerUpdater::setSerialPortReady");
    _serialPortReady = serialPortReady;
}

void MowerUpdater::printBootloaderInfo()
{
  char blversion = firmwareWriter.version();
  uint8_t majorVersion = (blversion >> 4) & 0x0F;
  uint8_t minorVersion = blversion & 0x0F;
  String mcuId = firmwareWriter.getId();
  
  Log(INFO, "Bootloader Info - Version: %d.%d, MCU: %s, Target File: %s", 
      majorVersion, minorVersion, mcuId.c_str(), _filename.c_str());
}

String MowerUpdater::handleFlash()
{
  Log(INFO, "MowerUpdater::handleFlash starting firmware update: %s", _filename.c_str());

  // Constants for better maintainability
  static const size_t BLOCK_SIZE = 256;
  static const byte PROGRESS_ERASE = 5;
  static const byte PROGRESS_FLASH_START = 10;
  static const byte PROGRESS_FLASH_END = 95;
  
  uint8_t binread[BLOCK_SIZE];
  fsUploadFile = SPIFFS.open(_filename, "r");
  
  if (!fsUploadFile) {
    Log(ERR, "MowerUpdater::handleFlash failed to open file: %s", _filename.c_str());
    return "error-file-open";
  }

  // RAII-style cleanup helper
  auto cleanup = [this]() {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    firmwareWriter.switchToRunMode();
  };

  Log(INFO, "MowerUpdater::handleFlash file opened, size: %u bytes", fsUploadFile.size());

  if (fsUploadFile.size() == 0) {
    cleanup();
    return "error-file-empty";
  }

  {
    auto bini = fsUploadFile.size() / BLOCK_SIZE;
    auto lastbuf = fsUploadFile.size() % BLOCK_SIZE;
    
    Log(INFO, "MowerUpdater::handleFlash processing %u full blocks + %u remaining bytes", 
        (unsigned)bini, (unsigned)lastbuf);

    updateStatus(0);

    if (!firmwareWriter.switchToFlashMode()) {
      cleanup();
      return "error-init";
    }

    // if (!firmwareWriter.checkFlashMode()) {
    //   cleanup();
    //   return "error-connection-check";
    // }

    Log(INFO, "MowerUpdater::handleFlash bootloader ready, erasing flash");
    updateStatus(PROGRESS_ERASE);
    
    if (!firmwareWriter.erase()) {
      cleanup();
      return "error-erase-flash";
    }

    Log(INFO, "MowerUpdater::handleFlash starting firmware flash");
    updateStatus(PROGRESS_FLASH_START);

    uint32_t currentAddress = STM32STADDR;
    uint32_t totalBytes = fsUploadFile.size();
    uint32_t bytesWritten = 0;

    // Helper function to write a block with retries
    auto writeBlock = [&](uint8_t* data, size_t size) -> String {
      static const int MAX_RETRIES = 3;
      for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (attempt > 0) {
          Log(WARN, "Retry block at 0x%x (attempt %d/%d)", currentAddress, attempt + 1, MAX_RETRIES);
          delay(50);
          yield();
        }

        if (!firmwareWriter.sendCommand(STM32WR, 2000)) {
          if (attempt < MAX_RETRIES - 1) continue;
          Log(ERR, "Failed to send write command at address 0x%x", currentAddress);
          return "error-write-command";
        }
        yield();

        if (!firmwareWriter.sendAddress(currentAddress, 1000)) {
          if (attempt < MAX_RETRIES - 1) continue;
          Log(ERR, "Failed to send address 0x%x", currentAddress);
          return "error-write-address";
        }
        yield();

        if (!firmwareWriter.sendDataBlock(data, size, 2000)) {
          if (attempt < MAX_RETRIES - 1) continue;
          Log(ERR, "Failed to send data block at address 0x%x", currentAddress);
          return "error-write-data";
        }

        // All steps succeeded
        break;
      }

      currentAddress += size;
      bytesWritten += size;
      
      // Calculate accurate progress
      byte progress = PROGRESS_FLASH_START + 
                     (bytesWritten * (PROGRESS_FLASH_END - PROGRESS_FLASH_START)) / totalBytes;
      updateStatus(progress);
      
      return "";
    };

    // Process full blocks
    for (uint32_t i = 0; i < bini; i++) {
        unsigned long iterationStartTime = millis();

        size_t bytesRead = fsUploadFile.read(binread, BLOCK_SIZE);
        if (bytesRead != BLOCK_SIZE) {
          Log(ERR, "Failed to read block %u, expected %u bytes, got %u", 
              (unsigned)i, (unsigned)BLOCK_SIZE, (unsigned)bytesRead);
          cleanup();
          return "error-file-read";
        }
        yield();

        String error = writeBlock(binread, BLOCK_SIZE);
        if (!error.isEmpty()) {
          cleanup();
          return error;
        }
        
        yield();
        Log(DBG, "Block %u written in %lu ms", (unsigned)i, millis() - iterationStartTime);
    }

    // Handle the last partial block
    if (lastbuf > 0) {
        size_t bytesRead = fsUploadFile.read(binread, lastbuf);
        if (bytesRead != lastbuf) {
          Log(ERR, "Failed to read last block, expected %u bytes, got %u", 
              (unsigned)lastbuf, (unsigned)bytesRead);
          cleanup();
          return "error-file-read-last";
        }

        String error = writeBlock(binread, lastbuf);
        if (!error.isEmpty()) {
          cleanup();
          return error;
        }
    }

    fsUploadFile.close();
    Log(INFO, "MowerUpdater::handleFlash firmware written successfully");
    updateStatus(100);
  } // End of file handling scope

  firmwareWriter.switchToRunMode();
  Log(INFO, "MowerUpdater::handleFlash update completed successfully");
  return "";
}

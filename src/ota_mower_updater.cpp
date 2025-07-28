
#include "ota_mower_updater.h"
#include "log.h"
#include <SPIFFS.h>

using namespace ArduMower::Modem::Ota;

void StatusMessage::marshal(const JsonObject &o) const
{
  JsonObject jsonLine = o.createNestedObject("status");
  jsonLine["progress"] = progress;
}

MowerUpdater::MowerUpdater(Terminal &terminal, HardwareSerial &mowerFirmwareSerial) : 
    _terminal(terminal), _serial(mowerFirmwareSerial), 
    firmwareWriter(FirmwareWriterSTM32(mowerFirmwareSerial))
{
}

void MowerUpdater::startUpdate(String filename, UpdateComplete updateComplete) 
{ 
    _filename = filename;
    _updateComplete = updateComplete;
    
    _terminal.suspend([this] {
        Log(INFO, "MowerUpdater::startUpdate terminal suspend done");
        this->setSerialPortReady(true);
    });
}

void MowerUpdater::addStatusHandler(StatusHandler handler)
{
  _statusHandler = handler;
}

void MowerUpdater::updateStatus(byte progress)
{
  if (((progress == 0 || progress == 100) && (progress != _progress)) || (progress > _progress + 10)) 
  {
    _progress = progress;
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
    _terminal.resume();
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
  String line = "Bootloader Ver: ";
  char blversion = firmwareWriter.version();
  line += String((blversion >> 4) & 0x0F) + "." + String(blversion & 0x0F) + " MCU: ";
  line += firmwareWriter.getId();
  line += " File:";
  
  Log(DBG, line.c_str());
}

String MowerUpdater::handleFlash()
{
  Log(DBG, "MowerUpdater::handleFlash %s", _filename.c_str());

  const size_t blockSize = 256;
  uint8_t binread[blockSize];
  fsUploadFile = SPIFFS.open(_filename, "r");
  
  Log(DBG, "MowerUpdater::handleFlash file open");

  if (fsUploadFile) {
    auto bini = fsUploadFile.size() / blockSize;
    auto lastbuf = fsUploadFile.size() % blockSize;
    
    Log(DBG, "MowerUpdater::handleFlash file is open size=%u", fsUploadFile.size());

    updateStatus(0);

    if (!firmwareWriter.switchToFlashMode()) {
      firmwareWriter.switchToRunMode();
      return "error-init";
    }

    // if (!firmwareWriter.checkFlashMode()) {
    //   return "error-connection check";
    // }

    Log(DBG, "MowerUpdater::handleFlash bootloader is ready, earse flash");
    if (!firmwareWriter.erase()) {
      firmwareWriter.switchToRunMode();
      return "error-earse-flash";
    }

    Log(DBG, "MowerUpdater::handleFlash flash firmware");
    updateStatus(10);

    uint32_t currentAddress = STM32STADDR; // Start address for flashing

    for (int i = 0; i < bini; i++) { // Loop for full blockSize-byte blocks
        unsigned long iterationStartTime = millis(); // Startzeit der Iteration

        fsUploadFile.read(binread, blockSize); // Read blockSize bytes
        yield();
        //Log(DBG, "Writing block %d at address 0x%x", i, currentAddress);

        // 1. Send 0x31 (Write Memory) command
        if (!firmwareWriter.sendCommand(STM32WR, 2000)) { // STM32WR is 0x31
            Log(ERR, "Failed to send 0x31 command for block %d", i);
            firmwareWriter.switchToRunMode();
            return "error-write-command-0x31";
        }
        yield();

        // 2. Send 4-byte address
        if (!firmwareWriter.sendAddress(currentAddress, 1000)) {
            Log(ERR, "Failed to send address 0x%x for block %d", currentAddress, i);
            firmwareWriter.switchToRunMode();
            return "error-write-address";
        }
        yield();

        // 3. Send data block (blockSize bytes)
        // The sendDataBlock function handles the N-1 byte, data, and checksum, then waits for the final ACK.
        if (!firmwareWriter.sendDataBlock(binread, blockSize, 2000)) { // blockSize bytes
            Log(ERR, "Failed to send data block %d at address 0x%x", i, currentAddress);
            firmwareWriter.switchToRunMode();
            return "error-write-data-block";
        }

        currentAddress += blockSize; // Increment address for the next block
        yield();

        Log(DBG, "Flash-Loop Block %d dauerte %lu ms", i, millis() - iterationStartTime);
        updateStatus(i * 80 / bini + 10);
    }

    // Handle the last partial block
    if (lastbuf > 0) {
        fsUploadFile.read(binread, lastbuf); // Read remaining bytes

        //Log(DBG, "Writing last partial block at address 0x%x (len=%d)", currentAddress, lastbuf);

        // 1. Send 0x31 (Write Memory) command
        if (!firmwareWriter.sendCommand(STM32WR, 2000)) {
            Log(ERR, "Failed to send 0x31 command for last block");
            firmwareWriter.switchToRunMode();
            return "error-write-command-0x31-last";
        }

        // 2. Send 4-byte address
        if (!firmwareWriter.sendAddress(currentAddress, 1000)) {
            Log(ERR, "Failed to send address 0x%x for last block", currentAddress);
            firmwareWriter.switchToRunMode();
            return "error-write-address-last";
        }

        // 3. Send data block (lastbuf bytes)
        if (!firmwareWriter.sendDataBlock(binread, lastbuf, 2000)) {
            Log(ERR, "Failed to send last partial data block");
            firmwareWriter.switchToRunMode();
            return "error-write-data-block-last";
        }
    }

    fsUploadFile.close();
    Log(DBG, "MowerUpdater::handleFlash enable firmware");
    updateStatus(100);
  }

  firmwareWriter.switchToRunMode();

  // Log(DBG, "MowerUpdater::handleFlash enable firmware");

  // if (firmwareWriter.run() == STM32ERR) {
  //   return "error-enable-firmware";
  // }

  Log(DBG, "MowerUpdater::handleFlash done");

  return "";
}

#include "ota_http_server.h"
#include "log.h"
#include <Update.h>
#include <AsyncJson.h>
#include <SPIFFS.h>

using namespace ArduMower::Modem::Ota;
using namespace std::placeholders;

const char *resultToString(Http::Result r);

HttpServer::HttpServer(Settings::Settings &settings, AsyncWebServer &server, Terminal &terminal, HardwareSerial &mowerFirmwareSerial)
    : ArduMower::Modem::Http::Common(settings), _server(server), _terminal(terminal), _mowerFirmwareSerial(mowerFirmwareSerial), 
    _active(false), _failed(false), _restart(false), _restartTime(0) {}

void HttpServer::begin()
{
  auto uploadRequestHandler = std::bind(&HttpServer::handleUploadRequest, this, _1);
  auto uploadHandler = std::bind(&HttpServer::handleUpload, this, _1, _2, _3, _4, _5, _6);

  _server.on("/api/modem/ota/upload", HTTP_POST, uploadRequestHandler, uploadHandler);

  auto postRequestHandler = std::bind(&HttpServer::handlePostRequest, this, _1);
  auto bodyHandler = std::bind(&HttpServer::handleBody, this, _1, _2, _3, _4, _5);

  _server.on("/api/modem/ota/post", HTTP_POST, postRequestHandler, uploadHandler, bodyHandler);
}

void HttpServer::loop()
{
  loopRestart();
}

void HttpServer::handleUploadRequest(AsyncWebServerRequest *request)
{
  Http::UploadSession *session = (Http::UploadSession*)request->_tempObject;
  if (session == NULL)
  {
    Log(ERR, "Http::UploadSession::handleRequest::session-null");
    reject(request, 400, "upload", "unknown-request");
    return;
  }

  session->respond(request);
}

FirmwareUploadType HttpServer::getUploadType(AsyncWebServerRequest *request) {
  FirmwareUploadType uploadType = FirmwareUploadType::modem;
  if (request->hasParam("type")) {
    if (request->getParam("type")->value() == "mower") {
      uploadType = FirmwareUploadType::mower;
    }
  }

  Log(INFO, "Http::UploadSession::getUploadType type=%d", uploadType);

  return uploadType;
}

void HttpServer::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (index == 0) {
    FirmwareUploadType uploadType = getUploadType(request);
    if (uploadType == FirmwareUploadType::modem) {
      beginModemUpdate(request, index, data, len, final);
    } else {
      beginMowerUpdate(request, filename, index, data, len, final);
    }
  } else {
    continueUpdate(request, index, data, len, final);
  }
}

void HttpServer::handlePostRequest(AsyncWebServerRequest *request)
{
  Http::UploadSession *session = (Http::UploadSession *)request->_tempObject;
  if (session == NULL)
  {
    Log(ERR, "Http::UploadSession::handlePostRequest::session-null");
    reject(request, 400, "upload", "unknown-request");
    return;
  }

  session->respond(request);
}

void HttpServer::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (index == 0)
    beginModemUpdate(request, index, data, len, len == total);
  else
    continueUpdate(request, index, data, len, (index + len) == total);
}

void HttpServer::beginModemUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!auth(request))
    return;

  auto session = new Http::ModemUploadSession(this);
  request->_tempObject = session;
  session->handle(index, data, len, final);
}

void HttpServer::beginMowerUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!auth(request))
    return;

  auto session = new Http::MowerUploadSession(this, filename, _mowerFirmwareSerial);
  request->_tempObject = session;
  session->handle(index, data, len, final);

  _terminal.suspend([session] {
    Log(INFO, "Http::UploadSession::beginMowerUpdate terminal suspend done");
    session->setSerialPortReady(true);
  });
}

void HttpServer::continueUpdate(AsyncWebServerRequest *request, size_t index, uint8_t *data, size_t len, bool final)
{
  Http::UploadSession *session = (Http::UploadSession *)request->_tempObject;
  if (session == NULL)
    return;

  session->handle(index, data, len, final);
}

void HttpServer::requestRestart()
{
  _restartTime = millis() + 1000;
  _restart = true;
}

void HttpServer::loopRestart()
{
  if (!_restart)
    return;

  if (millis() < _restartTime)
    return;

  ESP.restart();
}

// Http::ModemUploadSession

void Http::ModemUploadSession::respond(AsyncWebServerRequest *request)
{

  auto res = new AsyncJsonResponse();
  auto o = res->getRoot();

  auto success = result == Result::SUCCESS;

  // TODO unify with error handling from http_common.h
  o["success"] = success;
  o["result"] = resultToString(result);

  if (success)
    o["md5"] = Update.md5String();

  Log(INFO, "Ota::Http::ModemUploadSession::respond(%d / %s)", (int)result, resultToString(result));
  res->setLength();
  request->send(res);
  if (success)
    s->requestRestart();
  else
    Update.abort();
}

void Http::ModemUploadSession::handle(size_t index, uint8_t *data, size_t len, bool final)
{
  if (!(result == Result::PENDING || result == Result::STARTED))
    return;

  if (_index != index)
  {
    Log(ERR, "Ota::Http::ModemUploadSession::handle::index-mismatch(expect=%u is=%u)", _index, index);
    result = Result::INDEX_MISMATCH;
    return;
  }

  if (index == 0)
  {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Log(ERR, "Ota::Http::ModemUploadSession::handle::update-begin-error(%s)", Update.errorString());
      result = Result::UPDATE_BEGIN_FAILED;
      return;
    }

    if (!verifyHeader(data, len))
    {
      result = Result::VERIFY_HEADER_FAILED;
      return;
    }

    Log(INFO, "Ota::Http::ModemUploadSession::handle::update-begin");
    result = Result::STARTED;
  }

  auto n = Update.write(data, len);
  if (n != len)
  {
    Log(ERR, "Ota::Http::ModemUploadSession::handle::update-write-error(len=%d written=%d error=%s)", len, n, Update.errorString());
    result = Result::SHORT_WRITE_ERROR;
    return;
  }
  _index += len;

  if (!final)
    return;

  if (!Update.end(true))
  {
    Log(ERR, "Ota::Http::ModemUploadSession::handle::update-end-error(%s)", Update.errorString());
    result = Result::UPDATE_END_FAILED;
    return;
  }

  Log(INFO, "Ota::Http::ModemUploadSession::handle::update-end");
  result = Result::SUCCESS;
}

bool Http::ModemUploadSession::verifyHeader(uint8_t *data, size_t len)
{
  if (len < 1)
  // return false;
  {
    Log(INFO, "Ota::Http::ModemUploadSession::verifyHeader::error::length");
    return false;
  }
  if (data[0] != 0xe9)
  // return false;
  {
    Log(INFO, "Ota::Http::ModemUploadSession::verifyHeader::error::magic");
    return false;
  }
  // -00000000  e9 06 02 2f f0 4a 08 40  ee 00 00 00 00 00 00 00  |.../.J.@........|
  // +00000000  e9 06 02 2f 04 48 08 40  ee 00 00 00 00 00 00 00  |.../.H.@........|

  // -00000010  00 00 00 00 00 00 00 01  20 00 40 3f e0 59 05 00  |........ .@?.Y..|
  // +00000010  00 00 00 00 00 00 00 01  20 00 40 3f 3c 5f 05 00  |........ .@?<_..|
  // +00000010  00 00 00 00 00 00 00 01  20 00 40 3f f0 52 03 00  |........ .@?.R..|

  // if (len < )
  return true;
}

static const char *result_success = "success";
static const char *result_started = "started";
static const char *result_incomplete = "incomplete";
static const char *result_error = "error";
static const char *result_index_mismatch = "index_mismatch";
static const char *result_update_begin_failed = "update_begin_failed";
static const char *result_verify_header_failed = "verify_header_failed";
static const char *result_short_write_error = "short_write_error";
static const char *result_update_end_failed = "update_end_failed";
static const char *result_unknown = "unknown";

const char *resultToString(Http::Result r)
{
  switch (r)
  {
  case Http::Result::SUCCESS:
    return result_success;

  case Http::Result::STARTED:
    return result_started;

  case Http::Result::INCOMPLETE:
    return result_incomplete;

  case Http::Result::ERROR:
    return result_error;

  case Http::Result::INDEX_MISMATCH:
    return result_index_mismatch;

  case Http::Result::UPDATE_BEGIN_FAILED:
    return result_update_begin_failed;

  case Http::Result::VERIFY_HEADER_FAILED:
    return result_verify_header_failed;

  case Http::Result::SHORT_WRITE_ERROR:
    return result_short_write_error;

  case Http::Result::UPDATE_END_FAILED:
    return result_update_end_failed;

  default:
    return result_unknown;
  }
}

// Http::MowerUploadSession

Http::MowerUploadSession::MowerUploadSession(HttpServer *server, String filename, HardwareSerial &serial) : 
  _server(server), _filename(filename), result(Result::PENDING), _index(0), _serial(serial), firmwareWriter(FirmwareWriterSTM32(serial)) 
{
  if(!SPIFFS.begin(true)){
    Log(ERR, "Http::MowerUploadSession::MowerUploadSession can't init SPIFFS");
    return;
  }

  if (!_filename.startsWith("/")) _filename = "/" + _filename;
}

void Http::MowerUploadSession::respond(AsyncWebServerRequest *request)
{

  auto res = new AsyncJsonResponse();
  auto o = res->getRoot();

  auto success = result == Result::SUCCESS;

  // TODO unify with error handling from http_common.h
  o["success"] = success;
  o["result"] = resultToString(result);

  if (success)
    o["md5"] = Update.md5String();

  Log(INFO, "Ota::Http::MowerUploadSession::respond(%d / %s)", (int)result, resultToString(result));
  res->setLength();
  request->send(res);
  if (success)
    _server->requestRestart();
  else
    Update.abort();
}

void Http::MowerUploadSession::setSerialPortReady(bool serialPortReady) 
{ 
  _serialPortReady = serialPortReady;
  
  Log(DBG, "Http::MowerUploadSession::setSerialPortReady (fileUploaded=%u serialPortReady=%u)", _fileUploaded, _serialPortReady);

  if (_fileUploaded) {
    updateFirmware();
  }
}

void Http::MowerUploadSession::handle(size_t index, uint8_t *data, size_t len, bool final)
{
  if (!(result == Result::PENDING || result == Result::STARTED))
    return;
    
  if (_index != index)
  {
    Log(ERR, "Ota::Http::MowerUploadSession::handle::index-mismatch(expect=%u is=%u)", _index, index);
    result = Result::INDEX_MISMATCH;
    return;
  }

  if (index == 0)
  {
    handleListFiles();

    if (SPIFFS.exists(_filename)) {
      Log(INFO, "Ota::Http::MowerUploadSession::handle remove file");
      SPIFFS.remove(_filename);
    }

    fsUploadFile = SPIFFS.open(_filename, "w");
    
    if (!verifyHeader(data, len))
    {
      result = Result::VERIFY_HEADER_FAILED;
      return;
    }

    Log(INFO, "Ota::Http::MowerUploadSession::handle::upload-begin %s", _filename.c_str());
    result = Result::STARTED;
  }

  if (!fsUploadFile) 
  {
    Log(ERR, "Ota::Http::MowerUploadSession::handle::upload-write-error(no file handler)");
    result = Result::UPDATE_BEGIN_FAILED;
    return;
  }
  

  auto n = fsUploadFile.write(data, len);
  if (n != len)
  {
    Log(ERR, "Ota::Http::MowerUploadSession::handle::upload-write-error(len=%d written=%d)", len, n);
    result = Result::SHORT_WRITE_ERROR;
    return;
  }
  _index += len;

  if (!final)
    return;

  fsUploadFile.close();
  _fileUploaded = true;

  Log(DBG, "Ota::Http::MowerUploadSession::handle written %u (fileUploaded=%u serialPortReady=%u)", _index, _fileUploaded, _serialPortReady);
  
  if (_serialPortReady) {
    updateFirmware();    
  }
}

bool Http::MowerUploadSession::updateFirmware()
{
  String err = handleFlash();
  if (err != "")
  {
    Log(ERR, "Ota::Http::MowerUploadSession::handle::update-error(%s)", err.c_str());
    result = Result::UPDATE_END_FAILED;
    return false;
  }

  Log(INFO, "Ota::Http::MowerUploadSession::handle::update-end");
  result = Result::SUCCESS;

  _server->resumeTerminal();

  return true;
}

bool Http::MowerUploadSession::verifyHeader(uint8_t *data, size_t len)
{
  // TODO
  return true;
}

void Http::MowerUploadSession::handleListFiles()
{
  String fileList = "Bootloader Ver: ";
  String Listcode;
  char blversion = 0;
  File dir = SPIFFS.open("/");
  blversion = firmwareWriter.version();
  fileList += String((blversion >> 4) & 0x0F) + "." + String(blversion & 0x0F) + " MCU: ";
  fileList += firmwareWriter.getId();
  fileList += " File:";
  File file = dir.openNextFile();
  while (file)
  {
    String fileName = file.name();
    File f = SPIFFS.open(("/" + fileName).c_str());
    String fileSize = String(f.size());
    fileList +=  " " + fileName + "   Size: " + fileSize;
    file = dir.openNextFile();
  }
  Log(INFO, fileList.c_str());
}

String Http::MowerUploadSession::handleFlash()
{
  Log(DBG, "Http::MowerUploadSession::handleFlash %s", _filename.c_str());

  const size_t blockSize = 256;
  uint8_t binread[blockSize];
  fsUploadFile = SPIFFS.open(_filename, "r");
  Log(DBG, "Http::MowerUploadSession::handleFlash file open");

  if (fsUploadFile) {
    auto bini = fsUploadFile.size() / blockSize;
    auto lastbuf = fsUploadFile.size() % blockSize;
    
    Log(DBG, "Http::MowerUploadSession::handleFlash file is open size=%u", fsUploadFile.size());

    if (!firmwareWriter.switchToFlashMode()) {
      firmwareWriter.switchToRunMode();
      return "error-init";
    }

    // if (!firmwareWriter.checkFlashMode()) {
    //   return "error-connection check";
    // }

    Log(DBG, "Http::MowerUploadSession::handleFlash bootloader is ready, earse flash");
    if (!firmwareWriter.erase()) {
      firmwareWriter.switchToRunMode();
      return "error-earse-flash";
    }

    Log(DBG, "Http::MowerUploadSession::handleFlash flash firmware");

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
    Log(DBG, "Http::MowerUploadSession::handleFlash enable firmware");
  }

  firmwareWriter.switchToRunMode();

  Log(DBG, "Http::MowerUploadSession::handleFlash enable firmware");

  if (firmwareWriter.run() == STM32ERR) {
    return "error-enable-firmware";
  }

  Log(DBG, "Http::MowerUploadSession::handleFlash done");

  return "";
}

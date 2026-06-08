#include "ota_http_server.h"
#include "log.h"
#include <Update.h>
#include <AsyncJson.h>
#include <SPIFFS.h>

using namespace ArduMower::Modem::Ota;
using namespace std::placeholders;

const char *resultToString(Http::Result r);

HttpServer::HttpServer(Settings::Settings &settings, AsyncWebServer &server, MowerUpdater &mowerUpdater)
    : ArduMower::Modem::Http::Common(settings), _server(server), _mowerUpdater(mowerUpdater),
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

  size_t totalSize = request->contentLength();
  auto session = new Http::ModemUploadSession(this, totalSize);
  request->_tempObject = session;
  session->handle(index, data, len, final);
}

void HttpServer::beginMowerUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!auth(request))
    return;

  auto session = new Http::MowerUploadSession(this, filename, _mowerUpdater);
  request->_tempObject = session;
  session->handle(index, data, len, final);
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
  else {
    Update.abort();
    
  }
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
    if (!Update.begin(_totalSize > 0 ? _totalSize : UPDATE_SIZE_UNKNOWN))
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
static const char *result_flash_file = "flash_file";
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

  case Http::Result::FLASH_FILE:
    return result_flash_file;

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

Http::MowerUploadSession::MowerUploadSession(HttpServer *server, String filename, MowerUpdater &mowerUpdater) : 
  _server(server), _filename(filename), _mowerUpdater(mowerUpdater), result(Result::PENDING), _index(0) 
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

  bool success = (result == Result::SUCCESS || result == Result::FLASH_FILE);

  o["success"] = success;
  o["result"] = resultToString(result);

  Log(INFO, "Ota::Http::MowerUploadSession::respond(%d / %s)", (int)result, resultToString(result));
  res->setLength();
  request->send(res);
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
      Log(DBG, "Ota::Http::MowerUploadSession::handle remove file");
      SPIFFS.remove(_filename);
    }

    fsUploadFile = SPIFFS.open(_filename, "w");
    
    if (!verifyHeader(data, len))
    {
      result = Result::VERIFY_HEADER_FAILED;
      return;
    }

    Log(DBG, "Ota::Http::MowerUploadSession::handle::upload-begin %s", _filename.c_str());
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
  result = Result::FLASH_FILE;
  
  Log(DBG, "Ota::Http::MowerUploadSession::handle written %u", _index);
  
  _mowerUpdater.startUpdate(_filename, [this](String updateResult) {
    if (updateResult == "") {
      Log(INFO, "Ota::Http::MowerUploadSession::handle success");
    } else {
      Log(ERR, "Ota::Http::MowerUploadSession::handle faild with error %s", updateResult.c_str());
    }
    result = updateResult == "" ? Result::SUCCESS : Result::UPDATE_END_FAILED;
  });
}

bool Http::MowerUploadSession::verifyHeader(uint8_t *data, size_t len)
{
  if (len < 8)
  {
    Log(INFO, "Ota::Http::MowerUploadSession::verifyHeader::error::length (need at least 8 bytes, got %d)", len);
    return false;
  }
  
  // Check for STM32 firmware patterns:
  // 1. ARM Cortex-M vector table - first 4 bytes should be stack pointer (typically in RAM range)
  // 2. Second 4 bytes should be reset vector (typically in flash range)
  
  uint32_t stackPointer = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
  uint32_t resetVector = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
  
  // STM32 stack pointer should be in RAM range (typically 0x20000000 - 0x20040000 for most STM32)
  // Stack Pointer: 0x20000000–0x200FFFFF (STM32 RAM, bis 1MB)
  if (stackPointer < 0x20000000 || stackPointer > 0x200FFFFF) {
      Log(INFO, "Ota::Http::MowerUploadSession::verifyHeader::error::invalid-stack-pointer (0x%08x)", stackPointer);
      return false;
  }
  
  // Reset vector should be in flash range (typically 0x08000000+ for STM32) and should be odd (Thumb mode)
  // Reset Vector: 0x08000001–0x080FFFFF (STM32 Flash, Thumb Mode)
  if (resetVector < 0x08000001 || resetVector > 0x080FFFFF || (resetVector & 0x1) == 0) {
      Log(INFO, "Ota::Http::MowerUploadSession::verifyHeader::error::invalid-reset-vector (0x%08x)", resetVector);
      return false;
  }
  
  // Additional check: file should have reasonable size for STM32 firmware
  if (len > 0 && len < 1024)
  {
    Log(WARN, "Ota::Http::MowerUploadSession::verifyHeader::warning::small-file-size (%d bytes)", len);
    // Don't fail here, just warn - it might be the first chunk
  }
  
  Log(INFO, "Ota::Http::MowerUploadSession::verifyHeader::success (SP: 0x%08x, Reset: 0x%08x)", stackPointer, resetVector);
  return true;
}

void Http::MowerUploadSession::handleListFiles()
{
  String fileList = "File list: ";
  String Listcode;
  File dir = SPIFFS.open("/");
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


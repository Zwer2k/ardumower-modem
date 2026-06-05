#include "http_adapter.h"
#include "mower_adapter.h"
#include "prometheus.h"
#include <Arduino.h>
#include "stm32ota/stm32ota.h"

#define _LOG_ "HttpAdapter::"

using namespace ArduMower::Modem;

      
void respondWithCors(AsyncWebServerRequest *req, int status, String contentType, String responseBody);
      
HttpAdapter::HttpAdapter(Router &router, AsyncWebServer &server, MowerAdapter &mower)
    : _router(router), _server(server), _mower(mower), requestId(0)
{
}

HttpAdapter::~HttpAdapter()
{
  delete _metrics;
}

void HttpAdapter::begin()
{
  _metrics = new Http::Metrics();
  _lock = xSemaphoreCreateBinary();
  xSemaphoreGive(_lock);

  auto commandRequestHandler = new AsyncCallbackWebHandler();
  commandRequestHandler->setUri("/");
  commandRequestHandler->setMethod(HTTP_POST);
  commandRequestHandler->onRequest(std::bind(&HttpAdapter::handleCommandRequest, this, std::placeholders::_1));
  commandRequestHandler->onBody(std::bind(&HttpAdapter::handleCommandRequestBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
  _server.addHandler(commandRequestHandler);

  _server.on("/", HTTP_OPTIONS, std::bind(&HttpAdapter::handleCORSPreflightRequest, this, std::placeholders::_1));
  _server.on("/api/modem/reboot", HTTP_POST, std::bind(&HttpAdapter::apiReboot, this, std::placeholders::_1));
  _server.on("/api/mower/reboot", HTTP_POST, std::bind(&HttpAdapter::apiMowerReboot, this, std::placeholders::_1));
  _server.on("/api/mower/rebootGps", HTTP_POST, std::bind(&HttpAdapter::apiMowerRebootGps, this, std::placeholders::_1));
}

void HttpAdapter::loop()
{
  processQueue();
  if (_restartAt != 0 && millis() >= _restartAt) {
    _restartAt = 0;
    ESP.restart();
  }
}

void HttpAdapter::processQueue()
{
  std::list<Http::CommandRequest *> keep;

  xSemaphoreTake(_lock, portMAX_DELAY);
  uint32_t now = millis();
  size_t openRequests = _queue.size();
  for (auto req : _queue)
  {
    if (req->done(now))
    {
      delete req;
      continue;
    }
    processRequest(req);
    keep.push_back(req);
  }
  openRequests = keep.size();
  static size_t lastOpenRequests = 0;
  if (openRequests != lastOpenRequests) {
    if (openRequests >= 8) {
      Log(WARN, "%sprocessQueue: queue almost full (%u)", _LOG_, (unsigned)openRequests);
    } else {
      Log(DBG, "%sprocessQueue: open requests: %u", _LOG_, (unsigned)openRequests);
    }
    lastOpenRequests = openRequests;
  }
  _queue = keep;
  xSemaphoreGive(_lock);
}

size_t HttpAdapter::queueSize()
{
  xSemaphoreTake(_lock, portMAX_DELAY);
  const size_t result = _queue.size();
  xSemaphoreGive(_lock);

  return result;
}

void HttpAdapter::enqueueRequest(Http::CommandRequest *req)
{
  Log(DBG, "%senqueueRequest (before): open requests: %u", _LOG_, (unsigned)_queue.size());
  if (queueIsFull())
  {
    Log(DBG, "%senqueueRequest::reject::full", _LOG_);
    req->reject(500, "request queue full");
    delete req;
    return;
  }

  xSemaphoreTake(_lock, portMAX_DELAY);
  _queue.push_back(req);
  Log(DBG, "%senqueueRequest (after): open requests: %u", _LOG_, (unsigned)_queue.size());
  xSemaphoreGive(_lock);
}

bool HttpAdapter::queueIsFull()
{
  return queueSize() >= 10;
}



void HttpAdapter::handleCommandRequest(AsyncWebServerRequest *request)
{
  Log(DBG, "%shandleCommandRequest ID %d, open requests: %u", _LOG_, requestId, (unsigned)_queue.size());
  Http::CommandRequest *req = new Http::CommandRequest(requestId++, _metrics, request, millis());
  if (req->done(millis()))
  {
    Log(DBG, "%shandleCommandRequest::fast", _LOG_);
    delete req;
    return;
  }

  enqueueRequest(req);
}

void HttpAdapter::handleCommandRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  Log(DBG, "%shandleCommandRequestBody", _LOG_);
  if (total > 0 && request->_tempObject == NULL) {
    request->_tempObject = malloc(total);
  }
  if (request->_tempObject != NULL) {
    memcpy((uint8_t*)(request->_tempObject) + index, data, len);
  }
}

void HttpAdapter::handleCORSPreflightRequest(AsyncWebServerRequest *request)
{
  Log(DBG, "%shandleCORSPreflightRequest", _LOG_);
  respondWithCors(request, 204, "text/plain", "");
}

void HttpAdapter::processRequest(Http::CommandRequest *req)
{
  const uint32_t id = req->id;
  switch (req->state)
  {
  case 0:
    // command from http request body has not been sent to the router yet

    // body may have arrived after constructor (large requests)
    req->recoverRequestBody();

    // read-only Kommandos aus dem Cache bedienen (Modem schickt selbst AT+S alle 5s via loop())
    {
      String body = req->httpRequestBody;

      if (body.startsWith("AT+S3,")) {
        String cached = _mower.cachedRawSensorSummary();
        if (cached.length() > 0) {
          Log(DBG, "%sprocessRequest::cache-hit(AT+S3)", _LOG_);
          req->onRouterResponse(cached);
          return;
        }
      } else if (body.startsWith("AT+S4,")) {
        String cached = _mower.cachedRawGpsDetails();
        if (cached.length() > 0) {
          Log(DBG, "%sprocessRequest::cache-hit(AT+S4)", _LOG_);
          req->onRouterResponse(cached);
          return;
        }
      } else if (body.startsWith("AT+S,") && !body.startsWith("AT+S2,")) {
        String cached = _mower.cachedRawState();
        if (cached.length() > 0) {
          Log(DBG, "%sprocessRequest::cache-hit(AT+S)", _LOG_);
          req->onRouterResponse(cached);
          return;
        }
      } else if (body.startsWith("AT+T,")) {
        String cached = _mower.cachedRawStats();
        if (cached.length() > 0) {
          Log(DBG, "%sprocessRequest::cache-hit(AT+T)", _LOG_);
          req->onRouterResponse(cached);
          return;
        }
      }
    }

    // Cache miss or not a cacheable command → forward to modem
    {
      String body = req->httpRequestBody;
      if (body.startsWith("AT+S,") || body.startsWith("AT+T,") || body.startsWith("AT+S3,") || body.startsWith("AT+S4,")) {
        Log(DBG, "%sprocessRequest::cache-miss(%.8s) → router.send", _LOG_, body.c_str());
      }
      if (_router.send(body,
                       [=](String res, int err)
                       { handleRouterResponse(id, res); }))
        req->state = 1;
      else
        Log(DBG, "%sprocessRequest::router.send() failed (busy)", _LOG_);
    }
    break;

  case 1:
    // response from the router has not been received yet
    break;
  }
}

void HttpAdapter::handleRouterResponse(const uint32_t id, String res)
{
  Log(DBG, "%shandleRouterResponse", _LOG_);
  // *req might have been deleted already

  bool found = false;
  xSemaphoreTake(_lock, portMAX_DELAY);
  for (auto it : _queue)
  {
    if (it->id != id)
      continue;

    it->onRouterResponse(res);
    found = true;
    Log(DBG, "%shandleRouterResponse::success", _LOG_);
  }
  xSemaphoreGive(_lock);

  if (!found)
    Log(DBG, "%shandleRouterResponse::not-found(id=%d)", _LOG_, id);
}


void HttpAdapter::apiReboot(AsyncWebServerRequest *req)
{
  req->send(200, "text/plain", "rebooting");
  _restartAt = millis() + 500;
}

// Reboot STM32 Mower via GPIO (BOOT0 LOW, NRST pulse)
void HttpAdapter::apiMowerReboot(AsyncWebServerRequest *req)
{
  FirmwareWriterSTM32::rebootMcuStatic();
  req->send(200, "text/plain", "mower reboot triggered");
}

// Reboot GPS receiver via mower command AT+Y2
void HttpAdapter::apiMowerRebootGps(AsyncWebServerRequest *req)
{
  if (_mower.rebootGPS())
    req->send(200, "text/plain", "gps reboot triggered");
  else
    req->send(503, "text/plain", "router busy - try again");
}

void respondWithCors(AsyncWebServerRequest *req, int status, String contentType, String responseBody)
{
  Log(DBG, "%s::respondWithCors reqIsNull=%d code=%d contentType=%s responseBody=%s", _LOG_, req==NULL, status, contentType.c_str(), responseBody.c_str());

  if ((req != NULL) && req->client()->connected()) {
    AsyncWebServerResponse *res = req->beginResponse(status, contentType, responseBody);
    res->addHeader("Access-Control-Allow-Origin", "*");
    res->addHeader("Access-Control-Allow-Headers", "authorization");
    req->send(res);
  }
}

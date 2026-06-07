#include "http_adapter.h"

using namespace ArduMower::Modem;

extern void respondWithCors(AsyncWebServerRequest *req, int status, String contentType, String responseBody);

Http::CommandRequest::CommandRequest(
    const uint32_t _id,
    Metrics *metrics,
    AsyncWebServerRequest *_request,
    const uint32_t timeNow)
    : _metrics(metrics), id(_id), state(0), httpRequestBody(""), routerResponse(""), request(_request), _done(false), 
      timeReceiveHttpRequest(timeNow)
{
  serialRequest = request->pause();
  readHttpRequestBody();
  trimHttpRequestBody();
}

bool Http::CommandRequest::done(const uint32_t now)
{
  if (_done)
    return true;

  return timeout(now);
}

bool Http::CommandRequest::timeout(const uint32_t now)
{
  if (now - timeReceiveHttpRequest < 5000)
    return false;

  _done = true;
  reject(504, "timeout");

  return true;
}

void Http::CommandRequest::readHttpRequestBody()
{
  if (request->_tempObject)
  {
    httpRequestBody = (char*)request->_tempObject;
    free(request->_tempObject);
    request->_tempObject = nullptr;
    return;
  }

  httpRequestBody = "";
}

void Http::CommandRequest::trimHttpRequestBody()
{
  int idx = httpRequestBody.indexOf('\n');
  if (idx >= 0)
    httpRequestBody = httpRequestBody.substring(0, idx);
  while (httpRequestBody.endsWith("\r"))
    httpRequestBody = httpRequestBody.substring(0, httpRequestBody.length() - 1);
}

void Http::CommandRequest::recoverRequestBody()
{
  if (httpRequestBody != "") return;
  if (!request->_tempObject) return;
  httpRequestBody = (char*)request->_tempObject;
  free(request->_tempObject);
  request->_tempObject = nullptr;
  trimHttpRequestBody();
}

void Http::CommandRequest::validateHttpRequestBody()
{
  if (httpRequestBody == "")
  {
    Log(DBG, "Http::CommandRequest::validateHttpRequestBody::error::empty-body");
    reject(400, "empty body");
    return;
  }
}

void Http::CommandRequest::reject(int code, String text)
{
  Log(DBG, "Http::CommandRequest::reject code=%d %s", code, text.c_str());

  _done = true;
  auto request = serialRequest.lock();
  respondWithCors(request.get(), code, "text/plain", text);
  _metrics->countStatusCode(code);
}

void Http::CommandRequest::onRouterResponse(String response)
{
  Log(DBG, "Http::CommandRequest::onRouterResponse [%s]", response.c_str());

  _done = true;

  if (response.length() == 0)
  {
    Log(DBG, "Http::CommandRequest::onRouterResponse::empty-response");
    reject(504, "timeout");
    return;
  }

  // Short acknowledgment (like 'P', 'C', 'M', etc.) - just send OK
  if (response.length() <= 3) {
    Log(DBG, "Http::CommandRequest::onRouterResponse::ack-response(%s)", response.c_str());
    auto request = serialRequest.lock();
    respondWithCors(request.get(), 200, "text/html; charset=UTF-8", "OK\r\n");
    _metrics->countStatusCode(200);
    return;
  }

  response += "\r\n";
  auto request = serialRequest.lock();
  respondWithCors(request.get(), 200, "text/html; charset=UTF-8", response);
  _metrics->countStatusCode(200);
}

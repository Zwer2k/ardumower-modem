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
  parseHttpRequestBody();
}

bool Http::CommandRequest::done(const uint32_t now)
{
  if (_done)
    return true;

  return timeout(now);
}

bool Http::CommandRequest::timeout(const uint32_t now)
{
  if (now - timeReceiveHttpRequest < 1000)
    return false;

  _done = true;
  reject(504, "timeout");

  return true;
}

void Http::CommandRequest::parseHttpRequestBody()
{
  Log(DBG, "Http::CommandRequest::parseHttpRequestBody");
  readHttpRequestBody();
  trimHttpRequestBody();
  validateHttpRequestBody();
}

void Http::CommandRequest::readHttpRequestBody()
{
  if (request->_tempObject)
  {
    httpRequestBody = (char*)request->_tempObject;
    return;
  }

  if (request->params() > 0) {
    httpRequestBody = request->getParam(0)->value();
    return;
  }

  httpRequestBody = "";
}

void Http::CommandRequest::trimHttpRequestBody()
{
  while (httpRequestBody.endsWith("\n") || httpRequestBody.endsWith("\r"))
    httpRequestBody = httpRequestBody.substring(0, httpRequestBody.length() - 1);
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
  _done = true;
  respondWithCors(request, code, "text/plain", text);
  _metrics->countStatusCode(code);
}

void Http::CommandRequest::onRouterResponse(String response)
{
  Log(DBG, "Http::CommandRequest::onRouterResponse [%s]", response.c_str());

  response += "\r\n";
  _done = true;

  respondWithCors(request, 200, "text/html; charset=UTF-8", response);
  _metrics->countStatusCode(200);
}

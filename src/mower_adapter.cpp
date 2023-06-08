#include "mower_adapter.h"
#include "checksum.h"
#include "log.h"
#include "prometheus_util.h"
#include "settings.h"

#define _LOG_ "MowerAdapter::"
#define _LOG_CMD_ "MowerAdapter::command::"

using namespace ArduMower::Modem;

void processCSVResponse(String res, std::function<void(int, String)> fn);

MowerAdapter::MowerAdapter(Settings::Settings &_settings, Router &_router)
    : settings(_settings), router(_router), sendIsInitialized(false) {}

void MowerAdapter::begin()
{
  enc.setOn(settings.general.encryption);
  enc.setPassword(settings.general.password);
  router.sniffRx(this);
  router.sniffTx(this);
}

void MowerAdapter::drainRx(String line, bool &stop)
{
  parseArduMowerResponse(line);
}

void MowerAdapter::drainTx(String line, bool &stop)
{
  parseArduMowerCommand(line);
}

void MowerAdapter::parseArduMowerResponse(String line)
{
  //Log(DBG, "%sparseArduMowerResponse(%s)", _LOG_, line.c_str());
  Log(COMM, "<< %s", line.c_str());

  if (line.length() < 2 + 4)
  // return;
  {
    Log(DBG, "%sparseArduMowerResponse::guard::length(%d)", _LOG_, line.length());
    return;
  }

  if (line[1] != ',')
  // return;
  {
    Log(DBG, "%sparseArduMowerResponse::guard::second-char(%c)", _LOG_, line[1]);
    return;
  }

  char responseTypeCode = line[0];

  String checksumStr = line.substring(line.length() - 5);
  if (!checksumStr.startsWith(",0x"))
  // return;
  {
    Log(DBG, "%sparseArduMowerResponse::guard::checksum-prefix", _LOG_);
    return;
  }
  uint8_t checksum = (checksumStr[3] - '0') << 4 | (checksumStr[4] - '0');

  String payload = line.substring(0, line.length() - 5);

  Checksum chk;
  chk.update(payload);

  // TODO test
  // if (chk.value != checksum) return;

  if (payload.startsWith("S,"))
    parseStateResponse(payload);
  else if (payload.startsWith("V,"))
    parseVersionResponse(payload);
  else if (payload.startsWith("T,"))
    parseStatisticsResponse(payload);
  else
    Log(DBG, "%sparseArduMowerResponse::payload-unknown(%s)", _LOG_, payload.c_str());
}

void MowerAdapter::parseArduMowerCommand(String line)
{
  Log(DBG, "%sparseArduMowerCommand(%s)", _LOG_, line.c_str());
  if (!line.startsWith("AT+"))
  {
    char *buffer = strdup(line.c_str());
    enc.decrypt(buffer, line.length());
    line = buffer;
    free(buffer);
    //Log(DBG, "%sparseArduMowerCommand::decrypted(%s)", _LOG_, line.c_str());
    Log(COMM, ">> %s", line.c_str());
  }

  if (line.startsWith("AT+C"))
    parseATCCommand(line);
}

// start mowing
bool MowerAdapter::start()
{
  Log(DBG, "%sstart", _LOG_CMD_); 
  //    AT+C,   -1,                   -1,    -1,          -1,                 -1,      0.32,             -1,   -1
  // Command, Mow , start / stop / Dock , Speed, fix timeout, finish and restart, Waypoints, skip waypoint ,sonar
  return sendCommand("AT+C,-1,1,-1,-1,-1,-1,-1,-1");
}

// stop mowing
bool MowerAdapter::stop()
{
  Log(DBG, "%sstop", _LOG_CMD_);
  return sendCommand("AT+C,-1,0,-1,-1,-1,-1,-1,-1");
}

// dock mower to station
bool MowerAdapter::dock()
{
  Log(DBG, "%sstart", _LOG_CMD_);
  return sendCommand("AT+C,-1,4,-1,-1,-1,-1,-1,-1");
}

// skip one Waypoint
bool MowerAdapter::skipWaypoint()
{
  Log(DBG, "%sskipWaypoint", _LOG_CMD_);
  return sendCommand("AT+C,-1,-1,-1,-1,-1,-1,1,-1");
}

// set to a waypoint as percent of maximum waypoint
bool MowerAdapter::setWaypoint(float waypoint)
{
  Log(DBG, "%ssetWaypoint(%.2f)", _LOG_CMD_, waypoint);
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "AT+C,-1,-1,-1,-1,-1,%.2f,-1,-1", waypoint);
  String command(buffer);
  return sendCommand(command);
}

// change mower movement speed
bool MowerAdapter::changeSpeed(float speed)
{
  Log(DBG, "%schangeSpeed(%.2f)", _LOG_CMD_, speed);
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "AT+C,-1,-1,%.2f,-1,-1,-1,-1,-1", speed);
  String command(buffer);
  return sendCommand(command);
}

// set fix Timeout
bool MowerAdapter::setFixTimeout(int timeout)
{
  Log(DBG, "%ssetFixTimeout(%d)", _LOG_CMD_, timeout);
  String command = "AT+C,-1,-1,-1," + String(timeout) + ",-1,-1,-1,-1";
  return sendCommand(command);
}

// activate and deactivate mowMotor
bool MowerAdapter::mowerEnabled(bool enabled)
{
  Log(DBG, "%smowerEnabled(%d)", _LOG_CMD_, enabled);
  String command = "AT+C," + String(enabled ? "1" : "0") + ",-1,-1,-1,-1,-1,-1,-1";

  return sendCommand(command);
}

// activate and deactivate finish and restart
bool MowerAdapter::finishAndRestartEnabled(bool enabled)
{
  Log(DBG, "%sfinishAndRestartEnabled(%d)", _LOG_CMD_, enabled);
  String command = "AT+C,-1,-1,-1,-1," + String(enabled ? "1" : "0") + ",-1,-1,-1";

  return sendCommand(command);
}

// activate and deactivate sonar
bool MowerAdapter::sonarEnabled(bool enabled)
{
  Log(DBG, "%sfinishAndRestartEnabled(%d)", _LOG_CMD_, enabled);
  String command = "AT+C,-1,-1,-1,-1,-1,-1,-1," + String(enabled ? "1" : "0");

  return sendCommand(command);
}


bool MowerAdapter::requestVersion()
{
  Log(DBG, "%srequestVersion", _LOG_);
  return sendCommand("AT+V", false);
}

bool MowerAdapter::requestStatus()
{
  Log(DBG, "%srequestStatus", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+S", true);
}

bool MowerAdapter::requestStats()
{
  Log(DBG, "%srequestStats", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+T", true);
}

// linear: m/s
// angular: rad/s
bool MowerAdapter::manualDrive(float linear, float angular)
{
  Log(DBG, "%smanualDrive(%.2f, %.2f)", _LOG_, linear, angular);
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "AT+M,%.2f, %.2f", linear, angular);
  String command(buffer);
  return sendCommand(command);
}

// reboot the mower
bool MowerAdapter::reboot()
{
  Log(DBG, "%sreboot()", _LOG_);
  return sendCommand("AT+Y");
}

// reboot GPS
bool MowerAdapter::rebootGPS()
{
  Log(DBG, "%srebootGPS()", _LOG_);
  return sendCommand("AT+Y2");
}

// power off the mower
bool MowerAdapter::powerOff()
{
  Log(DBG, "%spowerOff()", _LOG_);
  return sendCommand("AT+Y3");
}

// custom command
bool MowerAdapter::customCmd(String cmd)
{
  if(cmd.indexOf("AT+")){
    Log(ERR, "%scustom command %s is invalid", _LOG_, cmd.c_str());
    return false;
  }
  else {
    Log(DBG, "%scustom command %s is valid and forwarded", _LOG_, cmd.c_str());
    return sendCommand(cmd);
  }
}

void MowerAdapter::parseStatisticsResponse(String line)
{
  Log(DBG, "%sparseStatisticsResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, String val)
                     {
                       switch (index)
                       {
                       case 1:
                         _stats.durations.idle = val.toInt();
                         break;
                       case 2:
                         _stats.durations.charge = val.toInt();
                         break;
                       case 3:
                         _stats.durations.mow = val.toInt();
                         break;
                       case 4:
                         _stats.durations.mowFloat = val.toInt();
                         break;
                       case 5:
                         _stats.durations.mowFix = val.toInt();
                         break;

                       case 6:
                         _stats.recoveries.mowFloatToFix = val.toInt();
                         break;

                       case 7:
                         _stats.mowDistanceTraveled = val.toFloat();
                         break;
                       case 8:
                         _stats.mowMaxDgpsAge = val.toFloat();
                         break;

                       case 9:
                         _stats.recoveries.imu = val.toInt();
                         break;

                       case 10:
                         _stats.tempMin = val.toFloat();
                         break;
                       case 11:
                         _stats.tempMax = val.toFloat();
                         break;

                       case 12:
                         _stats.gpsChecksumErrors = val.toInt();
                         break;
                       case 13:
                         _stats.dgpsChecksumErrors = val.toInt();
                         break;
                       case 14:
                         _stats.maxMotorControlCycleTime = val.toFloat();
                         break;
                       case 15:
                         _stats.serialBufferSize = val.toInt();
                         break;
                       case 16:
                         _stats.durations.mowInvalid = val.toInt();
                         break;
                       case 17:
                         _stats.recoveries.mowInvalid = val.toInt();
                         break;
                       case 18:
                         _stats.obstacles.count = val.toInt();
                         break;
                       case 19:
                         _stats.freeMemory = val.toInt();
                         break;
                       case 20:
                         _stats.resetCause = val.toInt();
                         break;
                       case 21:
                         _stats.gpsJumps = val.toInt();
                         break;
                       case 22:
                         _stats.obstacles.sonar = val.toInt();
                         break;
                       case 23:
                         _stats.obstacles.bumper = val.toInt();
                         break;
                       case 24:
                         _stats.obstacles.gpsMotionLow = val.toInt();
                         break;
                       }
                     });

  _stats.timestamp = now;
}

void MowerAdapter::parseVersionResponse(String line)
{
  Log(DBG, "%sparseVersionResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, String val)
                     {
                       // V,Ardumower Sunray,1.0.189,1,73,0x58\r
                       switch (index)
                       {
                       case 1:
                         _props.firmware = val;
                         break;
                       case 2:
                         _props.version = val;
                         break;
                       case 3:
                         enc.setOn(val.toInt() == 1);
                         break;
                       case 4:
                         enc.setChallenge(val.toInt());
                         break;
                       }
                     });

  _props.timestamp = now;
  sendIsInitialized = true;
}

void MowerAdapter::parseStateResponse(String line)
{
  Log(DBG, "%sparseStateResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, String val)
                     {
                       switch (index)
                       {
                       case 1:
                         _state.batteryVoltage = val.toFloat();
                         break;
                       case 2:
                         _state.position.x = val.toFloat();
                         break;
                       case 3:
                         _state.position.y = val.toFloat();
                         break;
                       case 4:
                         _state.position.delta = val.toFloat();
                         break;
                       case 5:
                         _state.position.solution = val.toInt();
                         break;
                       case 6:
                         _state.job = val.toInt();
                         break;
                       case 7:
                         _state.position.mowPointIndex = val.toInt();
                         break;
                       case 8:
                         _state.position.age = val.toFloat();
                         break;
                       case 9:
                         _state.sensor = val.toInt();
                         break;

                       case 10:
                         _state.target.x = val.toFloat();
                         break;
                       case 11:
                         _state.target.y = val.toFloat();
                         break;

                       case 12:
                         _state.position.accuracy = val.toFloat();
                         break;
                       case 13:
                         _state.position.visibleSatellites = val.toInt();
                         break;
                       case 14:
                         _state.amps = val.toFloat();
                         break;
                       case 15:
                         _state.position.visibleSatellitesDgps = val.toInt();
                         break;
                       case 16:
                         _state.mapCrc = val.toInt();
                         break;
                       }
                     });

  _state.timestamp = now;
}

void MowerAdapter::parseATCCommand(String line)
{
  Log(DBG, "%sparseATCCommand", _LOG_);
  const auto now = millis();

  processCSVResponse(
      line,
      [&](int index, String val)
      {
        if (val.toInt() == -1)
          return;

        switch (index)
        {
        case 1:
          _desiredState.mowerMotorEnabled = val.toInt() == 1;
          break;
        case 2:
          _desiredState.op = val.toInt();
          break;
        case 3:
          _desiredState.speed = val.toFloat();
          break;
        case 4:
          _desiredState.fixTimeout = val.toInt();
          break;
        case 5:
          _desiredState.finishAndRestart = val.toInt() == 1;
          break;
        case 6:
          // setMowingPointPercent
          break;
        case 7:
          // skipNextMowingPoint
          break;
        case 8:
          // sonarEnabled
          break;
        }
      });

  _desiredState.timestamp = now;
}

bool MowerAdapter::assertSendIsInitialized()
{
  if (sendIsInitialized)
    return true;

  const uint32_t now = millis();
  static uint32_t next_time = 0;
  if (now < next_time)
    return false;
  next_time = now + 1000;

  Log(DBG, "%sassertSendIsInitialized::request-version", _LOG_);

  if (!requestVersion()) {
    next_time = now + 5000;
    return false;
  }

  Log(DBG, "%sassertSendIsInitialized::version-requested", _LOG_);

  return false;
}

bool MowerAdapter::sendCommand(String command, bool encrypt)
{
  Checksum chk;
  chk.update(command);

  char *buffer;
  asprintf(&buffer, "%s,0x%02x", command.c_str(), chk.value());
  Log(COMM, "> %s", buffer);
  
  if (encrypt)
    enc.encrypt(buffer, strlen(buffer));

  auto result = router.sendWithoutResponse(buffer);  
  free(buffer);

  return result;
}

void processCSVResponse(String res, std::function<void(int, String)> fn)
{
  int index = -1;

  while (res.length() > 0)
  {
    index++;

    const auto delimiter = res.indexOf(",");
    String val = delimiter == -1 ? res : res.substring(0, delimiter);

    fn(index, val);

    if (delimiter != -1)
      res = res.substring(delimiter + 1);
    else
      res = "";
  }
}

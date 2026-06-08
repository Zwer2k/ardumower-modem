#include <sstream>
#include "mower_adapter.h"
#include "checksum.h"
#include "log.h"
#include "prometheus_util.h"
#include "settings.h"
#include "mower_map.h"

#define _LOG_ "MowerAdapter::"
#define _LOG_CMD_ "MowerAdapter::command::"

using namespace ArduMower::Modem;

void processCSVResponse(String res, std::function<void(int, String)> fn);

MowerAdapter::MowerAdapter(Settings::Settings &_settings, Router &_router)
    : settings(_settings), router(_router), sendIsInitialized(false) {}

void MowerAdapter::setMap(const ArduMower::Domain::Robot::MowerMap &map) {
  // Warte kurz, bis kein Lesevorgang auf _map läuft (max 100ms)
  int waitCount = 0;
  while (_map.isReading() && waitCount < 10) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    waitCount++;
  }
  if (_map.isReading()) {
    Log(WARN, "%ssetMap: Map-Lesevorgang läuft noch, ignoriere", _LOG_);
    return;
  }
  _map = map;
  _map.timestamp = millis();
  Log(INFO, "%ssetMap: perimeter=%d exclusions=%d dockpoints=%d waypoints=%d",
      _LOG_, _map.perimeter.size(), _map.exclusions.size(), _map.dockpoints.size(), _map.waypoints.size());
}

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

  if (line[1] != ',' && !(line[0] == 'S' && (line[1] == '3' || line[1] == '4')) && !(line[0] == 'U'))
  // return;
  {
    Log(DBG, "%sparseArduMowerResponse::guard::second-char(%c)", _LOG_, line[1]);
    return;
  }

  String checksumStr = line.substring(line.length() - 5);
  if (!checksumStr.startsWith(",0x"))
  // return;
  {
    Log(DBG, "%sparseArduMowerResponse::guard::checksum-prefix", _LOG_);
    return;
  }
  //uint8_t checksum = (checksumStr[3] - '0') << 4 | (checksumStr[4] - '0');

  String payload = line.substring(0, line.length() - 5);

  Checksum chk;
  chk.update(payload);

  // TODO test
  // if (chk.value != checksum) return;

  if (payload.startsWith("U,"))
    parseUbxResponse(payload);
  else if (payload.startsWith("S4,"))
    { _cachedRawGpsDetails = line; parseGpsDetailsResponse(payload); }
  else if (payload.startsWith("S3,"))
    { _cachedRawSensorSummary = line; parseSensorSummaryResponse(payload); }
  else if (payload.startsWith("S,"))
    { _cachedRawState = line; parseStateResponse(payload); }
  else if (payload.startsWith("V,"))
    parseVersionResponse(payload);
  else if (payload.startsWith("T,"))
    { _cachedRawStats = line; parseStatisticsResponse(payload); }
  else
    Log(DBG, "%sparseArduMowerResponse::payload-unknown(%s)", _LOG_, payload.c_str());
}

void MowerAdapter::parseArduMowerCommand(String line)
{
  //Log(DBG, "%sparseArduMowerCommand %s", _LOG_, line.substring(0, 4).c_str());
  if (!line.startsWith("AT+"))
  {
    char *buffer = strdup(line.c_str());
    enc.decrypt(buffer, line.length());
    line = buffer;
    free(buffer);
    //Log(DBG, "%sparseArduMowerCommand::decrypted(%s)", _LOG_, line.c_str());
  }

  if (line.startsWith("AT+")) {
    // Bereinige die Zeile von invaliden Bytes, bevor sie verarbeitet wird.
    // Das verhindert, dass invalide UTF-8-Sequenzen in Domain-Objekte
    // oder WebSocket-Nachrichten gelangen.
    for (int i = 0; i < line.length(); i++) {
      unsigned char c = (unsigned char)line[i];
      if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
        line[i] = '?';
      } else if (c >= 0x80) {
        line[i] = '?';
      }
    }

    int badChar = containsNonUTF8(line);
    if (badChar == -1) {
      Log(COMM, ">> %s", line.c_str());
    } else {
      String plainPart = line.substring(0, badChar);
      String hexPart = line.substring(badChar);
      String hexString = bytesToHexString(hexPart);
      Log(COMM, ">> %s(%s)", plainPart.c_str(),  hexString.c_str());
    }
  } else {
    String hexString = bytesToHexString(line);
    Log(COMM, ">> (%s)", hexString.c_str());
  }

  if (line.startsWith("AT+C"))
    parseATCCommand(line);

  if (line.startsWith("AT+W")) {
    parseATWCommand(line);
  }

  if (line.startsWith("AT+N")) {
    parseATNCommand(line);
  }
}

// AT-N Kommando: AT-N,#perimeter,#exclusions,#dockpoints,#waypoints,#free
void MowerAdapter::parseATNCommand(String line) {
  Log(DBG, "%sparseATNCommand (map end)", _LOG_);
  // Warte kurz, bis kein Lesevorgang auf _map läuft (max 100ms, um Loop-Blockade zu vermeiden)
  int waitCount = 0;
  while (_map.isReading() && waitCount < 10) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    waitCount++;
  }
  if (_map.isReading()) {
    Log(WARN, "%sparseATNCommand: Map-Lesevorgang läuft noch, ignoriere AT-N", _LOG_);
    return;
  }
  // Entferne "AT-N," falls vorhanden
  if (line.startsWith("AT+N,")) line = line.substring(5);
  std::vector<int> counts;
  int lastCommaIdx = -1;
  int len = line.length();
  for (int idx = 0; idx < len; idx++) {
    char ch = line[idx];
    if ((ch == ',') || (idx == len - 1)) {
      int valueEnd = (ch == ',') ? idx : idx + 1;
      String token = line.substring(lastCommaIdx + 1, valueEnd);
      counts.push_back(token.toInt());
      lastCommaIdx = idx;
    }
  }
  if (counts.size() < 5) {
    Log(ERR, "%sparseATNCommand: zu wenig Felder", _LOG_);
    return;
  }
  int nPerimeter = counts[0];
  int nExclusions = counts[1];
  int nDockpoints = counts[2];
  int nWaypoints = counts[3];
  //int nFree = counts[4];
  using namespace ArduMower::Domain::Robot;
  // Aufteilen der tempWaypointsBuffer in die richtigen Vektoren
  int idx = 0;
  _map.perimeter.clear();
  _map.exclusions.clear();
  _map.dockpoints.clear();
  _map.waypoints.clear();

  // Perimeter
  for (int i = 0; i < nPerimeter && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.perimeter.push_back(tempWaypointsBuffer[idx]);
  }
  // Exclusions (hier als ein Block, ggf. erweitern für mehrere Exclusions)
  for (int ex = 0; ex < nExclusions; ex++) {
    std::vector<MapPoint> excl;
    // Annahme: exclusions sind jeweils gleich groß, oder alle Punkte nacheinander
    // Hier als Dummy: alle Exclusion-Punkte in einen Vektor
    // (Anpassung nötig, falls mehrere Exclusions mit je eigener Punktzahl)
    // Beispiel: nExclusions = 1, dann alle Punkte in einen Vektor
    //           nExclusions > 1: Aufteilung nach bekannter Punktzahl
    // Hier: alle Exclusions zusammenfassen
    // TODO: Exclusion-Aufteilung nach realer Struktur
    // (Hier als Platzhalter: keine Exclusions)
    // excl.push_back(...)
    _map.exclusions.push_back(excl);
  }
  // Dockpoints
  for (int i = 0; i < nDockpoints && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.dockpoints.push_back(tempWaypointsBuffer[idx]);
  }

  // Waypoints
  for (int i = 0; i < nWaypoints && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.waypoints.push_back(tempWaypointsBuffer[idx]);
  }

  // Summen prüfen
  bool ok = true;
  if ((int)_map.perimeter.size() != nPerimeter) {
    Log(ERR, "%sparseATNCommand: perimeter count mismatch %d != %d", _LOG_, _map.perimeter.size(), nPerimeter);
    ok = false;
  }
  if ((int)_map.exclusions.size() != nExclusions) {
    Log(ERR, "%sparseATNCommand: exclusions count mismatch %d != %d", _LOG_, _map.exclusions.size(), nExclusions);
    ok = false;
  }
  if ((int)_map.dockpoints.size() != nDockpoints) {
    Log(ERR, "%sparseATNCommand: dockpoints count mismatch %d != %d", _LOG_, _map.dockpoints.size(), nDockpoints);
    ok = false;
  }
  if ((int)_map.waypoints.size() != nWaypoints) {
    Log(ERR, "%sparseATNCommand: waypoints count mismatch %d != %d", _LOG_, _map.waypoints.size(), nWaypoints);
    ok = false;
  }
  if (!ok) {
    Log(ERR, "%sparseATNCommand: Map-Übertragung fehlerhaft, Abbruch", _LOG_);
    return;
  }
  Log(DBG, "%sparseATNCommand: Map vollständig, sende an Client", _LOG_);
  _map.timestamp = millis();
  tempWaypointsBuffer.clear();
}

// start mowing
bool MowerAdapter::start()
{
  Log(DBG, "%sstart", _LOG_CMD_); 
  //    AT+C,   -1,                   -1,    -1,          -1,                 -1,      0.32,             -1,   -1
  // Command, Mow , start / stop / Dock , Speed, fix timeout, finish and restart, Waypoints, skip waypoint ,sonar
  return sendCommand("AT+C,-1,1,0.2,100,0,-1,-1,1");
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
  Log(DBG, "%sdock", _LOG_CMD_);
  return sendCommand("AT+C,-1,4,-1,-1,-1,-1,-1,1");
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
  uint32_t now = millis();
  if (_lastStateRequest != 0 && now - _lastStateRequest < 5000) return true;
  _lastStateRequest = now ? now : 1;
  Log(DBG, "%srequestStatus", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+S", true);
}

bool MowerAdapter::requestStats()
{
  uint32_t now = millis();
  if (_lastStatsRequest != 0 && now - _lastStatsRequest < 5000) return true;
  _lastStatsRequest = now ? now : 1;
  Log(DBG, "%srequestStats", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+T", true);
}

bool MowerAdapter::requestStatusNow()
{
  _lastStateRequest = 0;
  return requestStatus();
}

bool MowerAdapter::requestStatsNow()
{
  _lastStatsRequest = 0;
  return requestStats();
}

bool MowerAdapter::requestSensorSummary()
{
  //Log(DBG, "%srequestSensorSummary", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+S3", true);
}

bool MowerAdapter::requestGpsDetails()
{
  Log(DBG, "%srequestGpsDetails", _LOG_);
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+S4", true);
}

bool MowerAdapter::sendUbx(const String &hexCmd)
{
  Log(DBG, "%ssendUbx(%s)", _LOG_, hexCmd.c_str());
  if (!assertSendIsInitialized())
    return false;
  return sendCommand("AT+U," + hexCmd, true);
}

void MowerAdapter::parseUbxResponse(String line)
{
  Log(DBG, "%sparseUbxResponse", _LOG_);
  const auto now = millis();

  int idx = line.indexOf(',');
  if (idx >= 0) {
    _ubxResponse.hexData = line.substring(idx + 1);
  } else {
    _ubxResponse.hexData = "";
  }
  _ubxResponse.timestamp = now;
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

// navigate to point (autonomous, firmware handles navigation)
bool MowerAdapter::navigateTo(float x, float y)
{
  Log(DBG, "%snavigateTo(%.2f, %.2f)", _LOG_CMD_, x, y);
  if (_map.timestamp == 0 || _map.perimeter.size() == 0) {
    Log(WARN, "%snavigateTo rejected: no map loaded", _LOG_);
    return false;
  }
  char buffer[50];
  snprintf(buffer, sizeof(buffer), "AT+R,%.2f,%.2f", x, y);
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

void MowerAdapter::parseSensorSummaryResponse(String line)
{
  //Log(DBG, "%sparseSensorSummaryResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, String val)
                     {
                       // S3,<left>,<center>,<right>,<sonarObs>,<sonarNear>,<bumpL>,<bumpR>,<bumpObs>,<bumpNear>,<lidarObs>,<lidarNear>,<lift>,<rain>
                       switch (index)
                       {
                       case 1:
                         _sensorSummary.sonarLeft = val.toFloat();
                         break;
                       case 2:
                         _sensorSummary.sonarCenter = val.toFloat();
                         break;
                       case 3:
                         _sensorSummary.sonarRight = val.toFloat();
                         break;
                       case 4:
                         _sensorSummary.sonarObstacle = val.toInt() == 1;
                         break;
                       case 5:
                         _sensorSummary.sonarNearObstacle = val.toInt() == 1;
                         break;
                       case 6:
                         _sensorSummary.bumperLeft = val.toInt() == 1;
                         break;
                       case 7:
                         _sensorSummary.bumperRight = val.toInt() == 1;
                         break;
                       case 8:
                         _sensorSummary.bumperObstacle = val.toInt() == 1;
                         break;
                       case 9:
                         _sensorSummary.bumperNearObstacle = val.toInt() == 1;
                         break;
                       case 10:
                         _sensorSummary.lidarObstacle = val.toInt() == 1;
                         break;
                       case 11:
                         _sensorSummary.lidarNearObstacle = val.toInt() == 1;
                         break;
                       case 12:
                         _sensorSummary.liftTriggered = val.toInt() == 1;
                         break;
                       case 13:
                         _sensorSummary.rainTriggered = val.toInt() == 1;
                         break;
                       }
                     });

  _sensorSummary.timestamp = now;
}

void MowerAdapter::parseGpsDetailsResponse(String line)
{
  Log(DBG, "%sparseGpsDetailsResponse", _LOG_);
  const auto now = millis();

  _gpsDetails.timestamp = now;

  int satCount = 0;
  int fieldIdx = -1;

  // Erster Durchlauf: Header-Felder parsen und Gesamtzahl der Token ermitteln
  processCSVResponse(line, [&](int index, String val)
                     {
                       fieldIdx = index;
                       switch (index)
                       {
                       case 1:
                         _gpsDetails.numSV = val.toInt();
                         break;
                       case 2:
                         _gpsDetails.numSVdgps = val.toInt();
                         break;
                       case 3:
                         _gpsDetails.solution = val.toInt();
                         break;
                       case 4:
                         _gpsDetails.hAccuracy = val.toFloat();
                         break;
                       case 5:
                         _gpsDetails.vAccuracy = val.toFloat();
                         break;
                       case 6:
                         _gpsDetails.dgpsAge = val.toInt();
                         break;
                       case 7:
                         satCount = val.toInt();
                         break;
                       }
                     });

  // Format anhand der Gesamtfeldzahl bestimmen
  int totalFields = fieldIdx + 1;
  int dataFields = totalFields - 8;
  int fieldsPerSat = (satCount > 0) ? dataFields / satCount : 8;

  if (fieldsPerSat != 10 && fieldsPerSat != 8) {
    Log(WARN, "%sparseGpsDetailsResponse unexpected fieldsPerSat=%d sats=%d total=%d",
        _LOG_, fieldsPerSat, satCount, totalFields);
    fieldsPerSat = 8;
  }

  // Zweiter Durchlauf: Satellitenfelder on-the-fly parsen (kein Vector!)
  _gpsDetails.satellites.clear();
  processCSVResponse(line, [&](int index, String val)
                     {
                       if (index < 8) return;

                       int satField = index - 8;
                       int satIdx = satField / fieldsPerSat;
                       int col = satField % fieldsPerSat;

                       if (satIdx >= satCount || satIdx >= 40) return;

                       if ((size_t)satIdx >= _gpsDetails.satellites.size())
                         _gpsDetails.satellites.resize(satIdx + 1);

                       auto& sat = _gpsDetails.satellites[satIdx];
                       switch (col) {
                       case 0: sat.gnssId = val.toInt(); break;
                       case 1: sat.svId = val.toInt(); break;
                       case 2: sat.sigId = val.toInt(); break;
                       case 3: sat.cno = val.toInt(); break;
                       case 4: sat.qualityInd = val.toInt(); break;
                       case 5: sat.prUsed = val.toInt() == 1; break;
                       case 6: sat.crCorrUsed = val.toInt() == 1; break;
                       case 7: sat.prRes = val.toFloat(); break;
                       case 8: sat.elevation = (int8_t)val.toInt(); break;
                       case 9: sat.azimuth = (int8_t)val.toInt(); break;
                       }
                     });

  Log(DBG, "%sparseGpsDetailsResponse done sats=%d/%d fields=%d fps=%d",
      _LOG_, (int)_gpsDetails.satellites.size(), satCount, fieldIdx, fieldsPerSat);
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
                       
                       // incompatible to sunray upstream
                       case 20:
                         _state.chargingMah = val.toFloat();
                         break;
                       case 21:
                         _state.motorLeftMah = val.toFloat();
                         break;
                       case 22:
                         _state.motorRightMah = val.toFloat();
                         break;
                       case 23:
                         _state.motorMowMah = val.toFloat();
                         break;
                       case 24:
                         _state.temperature = val.toFloat();
                         break;
                       }
                     });

  _state.timestamp = now;
}

void MowerAdapter::parseATWCommand(String line)
{
  Log(DBG, "%sparseATWCommand (waypoint-list)", _LOG_);
  // Erwartetes Format: AT+W,<startIndex>,<x1>,<y1>,<x2>,<y2>,...
  if (line.startsWith("AT+W,")) line = line.substring(5);
  
  std::vector<float> values;
  int lastCommaIdx = -1;
  int len = line.length();
  for (int idx = 0; idx < len; idx++) {
    char ch = line[idx];
    if ((ch == ',') || (idx == len - 1)) {
      int valueEnd = (ch == ',') ? idx : idx + 1;
      String token = line.substring(lastCommaIdx + 1, valueEnd);
      values.push_back(token.toFloat());
      lastCommaIdx = idx;
    }
  }

  if (values.size() < 3) return; // Mindestens Index, x, y

  int widx = (int)values[0];
  size_t dataStart = 1;

  using namespace ArduMower::Domain::Robot;
  // Nur beim ersten Block (widx==0) die Waypoint-Liste leeren
  if (widx == 0) {
    tempWaypointsBuffer.clear();
  }
  // Schreibe die empfangenen Punkte ab Startindex (widx) in die Waypoint-Liste
  int writeIdx = widx;

  bool overwriteLogged = false;
  for (size_t i = dataStart; i + 1 < values.size(); i += 2, writeIdx++) {
    float x = values[i];
    float y = values[i + 1];
    if ((int)tempWaypointsBuffer.size() < writeIdx) {
      Log(ERR, "%sparseATWCommand: Indexsprung! writeIdx=%d, BufferSize=%d", _LOG_, writeIdx, (int)tempWaypointsBuffer.size());
    }
    if ((int)tempWaypointsBuffer.size() <= writeIdx) {
      tempWaypointsBuffer.resize(writeIdx + 1);
    } else {
      if (!overwriteLogged) {
        Log(DBG, "%sparseATWCommand: Überschreibe bestehenden Punkt an writeIdx=%d", _LOG_, writeIdx);
        overwriteLogged = true;
      }
    }
    tempWaypointsBuffer[writeIdx] = MapPoint{x, y};
  }
  
  Log(DBG, "%sparseATWCommand done widx=%d, waypoints.size()=%d", _LOG_, widx, tempWaypointsBuffer.size());
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
  if (sendIsInitialized) {
    uint32_t age = _state.timestamp > 0
        ? (millis() - _state.timestamp)
        : (millis() - _props.timestamp);
    if (age > 30000) {
      static uint32_t lastStaleWarn = 0;
      uint32_t now = millis();
      if (now - lastStaleWarn >= 10000) {
        lastStaleWarn = now;
        Log(WARN, "%sstate stale for 30s, STM32 may have rebooted - requesting version", _LOG_);
      }
      _state.timestamp = 0;
      sendIsInitialized = false;
    } else {
      return true;
    }
  }

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

int MowerAdapter::containsNonUTF8(const String& input) {
  const uint8_t* data = (const uint8_t*)input.c_str();
  size_t len = input.length();
  int i = 0;
  while (i < len) {
    if ((data[i] & 0x80) == 0x00) { // 0xxxxxxx (single byte)
      i++;
    } else if ((data[i] & 0xE0) == 0xC0) { // 110xxxxx (two bytes)
      if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80) return i;
      i += 2;
    } else if ((data[i] & 0xF0) == 0xE0) { // 1110xxxx (three bytes)
      if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) return i;
      i += 3;
    } else if ((data[i] & 0xF8) == 0xF0) { // 11110xxx (four bytes)
      if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) return i;
      i += 4;
    } else { // Invalid starting byte
      return i;
    }
  }
  return -1;
}

String MowerAdapter::bytesToHexString(const String& byteString) {
  String hexString = "";
  for (size_t i = 0; i < byteString.length(); ++i) {
    byte currentByte = byteString.charAt(i);
    char hexBuffer[3]; // Platz für zwei Hex-Ziffern und das Nullterminierungszeichen
    sprintf(hexBuffer, "%02X", currentByte); // %02X formatiert als zweistellige Hex-Zahl mit führender Null
    hexString += String(hexBuffer);
  }
  return hexString;
}

bool MowerAdapter::uploadMapToMower()
{
  int idx = 0;

  auto sendPoints = [&](const std::vector<ArduMower::Domain::Robot::MapPoint> &pts, int &startIdx) -> bool {
    for (size_t i = 0; i < pts.size(); i += 20) {
      String cmd = "AT+W," + String(startIdx + i);
      for (size_t j = i; j < pts.size() && j < i + 20; j++)
        cmd += "," + String(pts[j].X, 6) + "," + String(pts[j].Y, 6);
      String response;
      int retries = 3;
      bool ok = false;
      for (int r = 0; r < retries; r++) {
        delay(10);
        response = "";
        if (!sendCommandWithResponse(cmd, response, true, 5000)) {
          Log(WARN, "%s AT+W chunk %d send failed (try %d/%d)", _LOG_, (int)(i / 20), r + 1, retries);
          continue;
        }
        response.trim();
        if (response.startsWith("W")) { ok = true; break; }
        Log(WARN, "%s AT+W chunk %d unexpected: '%s' (try %d/%d)", _LOG_, (int)(i / 20), response.c_str(), r + 1, retries);
      }
      if (!ok) {
        Log(ERR, "%s AT+W chunk %d failed after %d retries", _LOG_, (int)(i / 20), retries);
        return false;
      }
    }
    startIdx += pts.size();
    return true;
  };

  if (!sendPoints(_map.perimeter, idx)) return false;
  for (const auto &excl : _map.exclusions)
    if (!sendPoints(excl, idx)) return false;
  if (!sendPoints(_map.dockpoints, idx)) return false;
  if (!sendPoints(_map.waypoints, idx)) return false;

  // Calculate total exclusion points across all polygons
  size_t totalExclPoints = 0;
  for (const auto &excl : _map.exclusions)
    totalExclPoints += excl.size();

  // Send AT+N with counts: perimeter, total-exclusion-points, dock, mow-waypoints, free-space
  // Must come before AT+X so exclusionPointsCount is set (prevents premature buffer free)
  {
    String cmd = "AT+N," + String(_map.perimeter.size()) + "," + String(totalExclPoints) + ","
                + String(_map.dockpoints.size()) + "," + String(_map.waypoints.size()) + ",0";
    String response;
    if (!sendCommandWithResponse(cmd, response, true, 5000)) {
      Log(ERR, "%s AT+N failed", _LOG_);
      return false;
    }
    response.trim();
    if (!response.startsWith("N")) {
      Log(WARN, "%s AT+N unexpected response: %s", _LOG_, response.c_str());
    }
  }

  // Send AT+X for exclusion polygon sizes (extracts exclusion points from flat buffer)
  for (size_t ex = 0; ex < _map.exclusions.size(); ex++) {
    String cmd = "AT+X," + String(ex) + "," + String(_map.exclusions[ex].size());
    String response;
    if (!sendCommandWithResponse(cmd, response, true, 5000)) {
      Log(ERR, "%s AT+X[%u] failed", _LOG_, ex);
      return false;
    }
    response.trim();
    if (!response.startsWith("X")) {
      Log(WARN, "%s AT+X[%u] unexpected response: %s", _LOG_, ex, response.c_str());
    }
  }

  // Wait a moment for Teensy to finalize
  delay(50);

  Log(INFO, "%s Map upload complete (%d perimeter, %d totalExclPoints, %d dockpoints, %d waypoints)",
      _LOG_, _map.perimeter.size(), totalExclPoints, _map.dockpoints.size(), _map.waypoints.size());
  return true;
}

void MowerAdapter::loop()
{
  uint32_t now = millis();
  if (_lastStateRequest == 0 || now - _lastStateRequest >= 5000) {
    _lastStateRequest = now ? now : 1;
    requestStatus();
  }
  if (_lastStatsRequest == 0 || now - _lastStatsRequest >= 5000) {
    _lastStatsRequest = now ? now : 1;
    requestStats();
  }
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

bool MowerAdapter::sendCommandWithResponse(String command, String &response, bool encrypt, int timeoutMs)
{
  Checksum chk;
  chk.update(command);

  char *buffer;
  asprintf(&buffer, "%s,0x%02x", command.c_str(), chk.value());
  Log(COMM, "> %s", buffer);

  if (encrypt)
    enc.encrypt(buffer, strlen(buffer));

  bool done = false;
  bool ok = router.send(buffer, [&](String resp, XferError error) {
    response = resp;
    done = true;
    ok = (error == XferError::SUCCESS);
  });
  free(buffer);

  if (!ok) return false;

  uint32_t start = millis();
  while (!done && (int)(millis() - start) < timeoutMs) {
    router.loop();
    delay(1);
    yield();
  }

  if (!done) {
    Log(WARN, "%ssendCommandWithResponse timeout for: %s", _LOG_, command.c_str());
    return false;
  }

  return ok;
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

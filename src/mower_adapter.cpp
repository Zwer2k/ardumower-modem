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
  Log(DBG, "%sparseArduMowerCommand %s", _LOG_, line.substring(0, 4).c_str());
  if (!line.startsWith("AT+"))
  {
    char *buffer = strdup(line.c_str());
    enc.decrypt(buffer, line.length());
    line = buffer;
    free(buffer);
    //Log(DBG, "%sparseArduMowerCommand::decrypted(%s)", _LOG_, line.c_str());
  }

  if (line.startsWith("AT+")) {
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

  // Logging: Alle Waypoints im Buffer vor dem Kopieren prüfen
  for (size_t i = 0; i < tempWaypointsBuffer.size(); i++) {
    const auto &pt = tempWaypointsBuffer[i];
    if ((pt.X == 0 && pt.Y == 0 && pt.delta == 0 && pt.timestamp.empty())) {
      Log(ERR, "%sparseATNCommand: tempWaypointsBuffer[%d] ist leer/uninitialisiert!", _LOG_, (int)i);
    }
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

  for (size_t i = dataStart; i + 1 < values.size(); i += 2, writeIdx++) {
    float x = values[i];
    float y = values[i + 1];
    if ((int)tempWaypointsBuffer.size() < writeIdx) {
      Log(ERR, "%sparseATWCommand: Indexsprung! writeIdx=%d, BufferSize=%d", _LOG_, writeIdx, (int)tempWaypointsBuffer.size());
    }
    if ((int)tempWaypointsBuffer.size() <= writeIdx) {
      tempWaypointsBuffer.resize(writeIdx + 1);
    } else {
      Log(DBG, "%sparseATWCommand: Überschreibe bestehenden Punkt an writeIdx=%d", _LOG_, writeIdx);
    }
    tempWaypointsBuffer[writeIdx] = MapPoint{x, y, 0, ""};
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

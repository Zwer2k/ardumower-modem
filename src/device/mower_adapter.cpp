#include <sstream>
#include "mower_adapter.h"
#include "checksum.h"
#include "log.h"
#include "prometheus_util.h"
#include "settings.h"
#include "mower_map.h"
#include <SPIFFS.h>

#define _LOG_ "MowerAdapter::"
#define _LOG_CMD_ "MowerAdapter::command::"

using namespace ArduMower::Modem;

void processCSVResponse(const char* res, std::function<void(int, const char*, size_t)> fn);
void processCSVResponse(const String& res, std::function<void(int, const char*, size_t)> fn);

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
  updateCurrentMapMeta();
  Log(INFO, "%ssetMap: perimeter=%d exclusions=%d dockpoints=%d waypoints=%d hash=%s area=%.1f",
      _LOG_, _map.perimeter.size(), _map.exclusions.size(), _map.dockpoints.size(), _map.waypoints.size(),
      _currentMapHash.c_str(), _currentMapArea);
}

void MowerAdapter::updateCurrentMapMeta() {
  _currentMapHash = _mapManager.computeHash(_map);
  _currentMapCrc = _mapManager.computeCrc(_map);
  _currentMapArea = _mapManager.computeArea(_map);
  int crcP, crcE, crcD, crcW;
  _map.computeMapCrcDetail(&crcP, &crcE, &crcD, &crcW);
  Log(INFO, "%s CRC %d aus Geometrie (p=%d e=%d d=%d w=%d, pts=%d/%d/%d/%d)",
      _LOG_, _currentMapCrc, crcP, crcE, crcD, crcW,
      _map.perimeter.size(), _map.exclusions.size(), _map.dockpoints.size(), _map.waypoints.size());
}

void MowerAdapter::setMowSettings(const ArduMower::Domain::Robot::MowSettings &s) {
  _mowSettings = s;
  _mowSettings.timestamp = millis();
  Log(INFO, "%ssetMowSettings: pattern=%d width=%.2f angle=%d distToBorder=%d laps=%d",
      _LOG_, s.pattern, s.width, s.angle, s.distanceToBorder, s.borderLaps);
}

std::vector<ArduMower::Domain::Robot::MapInfo> MowerAdapter::mapList() {
  std::vector<ArduMower::Domain::Robot::MapInfo> result;
  if (!_mapManager.begin()) return result;
  for (const auto &m : _mapManager.list()) {
    ArduMower::Domain::Robot::MapInfo info;
    info.id = m.id;
    info.name = m.name;
    info.area = m.area;
    info.hash = m.hash;
    info.crc = m.crc;
    info.rotation = m.rotation;
    info.timestamp = m.timestamp;
    result.push_back(info);
  }
  return result;
}

bool MowerAdapter::mapListDirty() {
  return _mapListDirty;
}

void MowerAdapter::clearMapListDirty() {
  _mapListDirty = false;
}

String MowerAdapter::saveMap(const String &name, double rotation) {
  if (_map.isReading()) {
    Log(WARN, "%ssaveMap: Map-Lesevorgang läuft, speichern abgelehnt", _LOG_);
    return "";
  }
  _map.rotation = rotation;
  String id = _mapManager.save(_map, name, rotation);
  if (id.length() > 0) {
    _mapListDirty = true;
    Log(INFO, "%ssaveMap: Karte gespeichert als %s", _LOG_, id.c_str());
  }
  return id;
}

bool MowerAdapter::loadMap(const String &id) {
  if (_map.isReading()) {
    Log(WARN, "%sloadMap: Map-Lesevorgang läuft, laden abgelehnt", _LOG_);
    return false;
  }
  ArduMower::Domain::Robot::MowerMap loaded;
  if (!_mapManager.load(id, loaded)) return false;
  if (!_mapManager.setActive(id)) return false;
  _map = loaded;
  _currentMapHash = _mapManager.computeHash(_map);
  _currentMapArea = _mapManager.computeArea(_map);
  _currentMapCrc = _mapManager.getCrc(id);
  Log(INFO, "%sloadMap: Karte %s geladen, CRC %d (aus SPIFFS)", _LOG_, id.c_str(), _currentMapCrc);
  _mapListDirty = true;
  return true;
}

bool MowerAdapter::renameMap(const String &id, const String &name) {
  if (!_mapManager.rename(id, name)) return false;
  _mapListDirty = true;
  Log(INFO, "%srenameMap: Karte %s umbenannt in '%s'", _LOG_, id.c_str(), name.c_str());
  return true;
}

bool MowerAdapter::deleteMap(const String &id) {
  if (!_mapManager.remove(id)) return false;
  _mapListDirty = true;
  Log(INFO, "%sdeleteMap: Karte %s gelöscht", _LOG_, id.c_str());
  return true;
}

String MowerAdapter::currentMapHash() {
  return _currentMapHash;
}

int MowerAdapter::currentMapCrc() {
  return _currentMapCrc;
}

double MowerAdapter::currentMapArea() {
  return _currentMapArea;
}

// ===== SPIFFS-Persistenz (vorbereitet, aber noch nicht aktiv) =====
// static const char *mapStorageFile = "/mower_map";
//
// void MowerAdapter::saveMap() { ... }
// void MowerAdapter::loadMap() { ... }

void MowerAdapter::begin()
{
  enc.setOn(settings.general.encryption);
  enc.setPassword(settings.general.password);
  router.sniffRx(this);
  router.sniffTx(this);

  // Persistierte Kartenverwaltung initialisieren und aktive Karte laden
  if (_mapManager.begin()) {
    ArduMower::Domain::Robot::MowerMap loaded;
    if (_mapManager.loadActive(loaded)) {
      _map = loaded;
      _currentMapHash = _mapManager.computeHash(_map);
      _currentMapArea = _mapManager.computeArea(_map);
      _currentMapCrc = _mapManager.getCrc(_mapManager.activeId());
      Log(INFO, "%s begin: aktive Karte '%s' aus SPIFFS geladen, CRC %d (%.1f m²)",
          _LOG_, _mapManager.activeId().c_str(), _currentMapCrc, _currentMapArea);
    } else if (_mapManager.activeId().length() > 0) {
      // Aktive ID verweist auf nicht mehr vorhandene Datei -> Index bereinigen
      _mapManager.setActive("");
    }
    _mapListDirty = true;
  }
}

void MowerAdapter::drainRx(const String& line, bool &stop)
{
  parseArduMowerResponse(line);
}

void MowerAdapter::drainTx(const String& line, bool &stop)
{
  parseArduMowerCommand(line);
}

void MowerAdapter::parseArduMowerResponse(const String& line)
{
  Log(COMM, "<< %s", line.c_str());

  int len = line.length();
  if (len < 2 + 4)
  {
    Log(DBG, "%sparseArduMowerResponse::guard::length(%d)", _LOG_, len);
    return;
  }

  if (line[1] != ',' && !(line[0] == 'S' && (line[1] == '3' || line[1] == '4')) && !(line[0] == 'U'))
  {
    Log(DBG, "%sparseArduMowerResponse::guard::second-char(%c)", _LOG_, line[1]);
    return;
  }

  // Checksum-Präfix ",0x" per Pointer-Check statt substring-Allokation
  const char* cstr = line.c_str();
  if (cstr[len-5] != ',' || cstr[len-4] != '0' || cstr[len-3] != 'x')
  {
    Log(DBG, "%sparseArduMowerResponse::guard::checksum-prefix", _LOG_);
    return;
  }

  const char* payload = cstr;

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
  if (strncmp(payload, "U,", 2) == 0)
    parseUbxResponse(payload);
  else if (strncmp(payload, "S4,", 3) == 0)
    { _cachedRawGpsDetails = line; parseGpsDetailsResponse(payload); }
  else if (strncmp(payload, "S3,", 3) == 0)
#else
  if (strncmp(payload, "S3,", 3) == 0)
#endif
    { _cachedRawSensorSummary = line; parseSensorSummaryResponse(payload); }
  else if (strncmp(payload, "S,", 2) == 0)
    { _cachedRawState = line; parseStateResponse(payload); }
  else if (strncmp(payload, "V,", 2) == 0)
    parseVersionResponse(payload);
  else if (strncmp(payload, "T,", 2) == 0)
    { _cachedRawStats = line; parseStatisticsResponse(payload); }
  else
    Log(DBG, "%sparseArduMowerResponse::payload-unknown(%s)", _LOG_, payload);
}

void MowerAdapter::parseArduMowerCommand(const String& line)
{
  String mutableLine;

  if (!line.startsWith("AT+"))
  {
    char *buffer = strdup(line.c_str());
    enc.decrypt(buffer, line.length());
    mutableLine = String(buffer, line.length());
    free(buffer);
  } else {
    mutableLine = line;
  }

  if (mutableLine.startsWith("AT+")) {
    for (int i = 0; i < mutableLine.length(); i++) {
      unsigned char c = (unsigned char)mutableLine[i];
      if (c < 0x20 && c != '\r' && c != '\n' && c != '\t')
        mutableLine[i] = '?';
      else if (c >= 0x80)
        mutableLine[i] = '?';
    }

    int badChar = containsNonUTF8(mutableLine);
    if (badChar == -1) {
      Log(COMM, ">> %s", mutableLine.c_str());
    } else {
      String plainPart = mutableLine.substring(0, badChar);
      String hexPart = mutableLine.substring(badChar);
      String hexString = bytesToHexString(hexPart);
      Log(COMM, ">> %s(%s)", plainPart.c_str(),  hexString.c_str());
    }
  } else {
    String hexString = bytesToHexString(mutableLine);
    Log(COMM, ">> (%s)", hexString.c_str());
  }

  if (mutableLine.startsWith("AT+C")) parseATCCommand(mutableLine);
  if (mutableLine.startsWith("AT+W")) parseATWCommand(mutableLine);
  if (mutableLine.startsWith("AT+N")) parseATNCommand(mutableLine);
  if (mutableLine.startsWith("AT+X")) parseATXCommand(mutableLine);
}

// AT-N Kommando: AT-N,#perimeter,#exclusionPoints,#dockpoints,#waypoints,#free
// Die Exclusion-Polygone werden über anschließende AT+X-Befehle mitgeteilt.
void MowerAdapter::parseATNCommand(const String& line) {
  Log(DBG, "%sparseATNCommand (map counts)", _LOG_);
  int waitCount = 0;
  while (_map.isReading() && waitCount < 10) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    waitCount++;
  }
  if (_map.isReading()) {
    Log(WARN, "%sparseATNCommand: Map-Lesevorgang läuft noch, ignoriere AT-N", _LOG_);
    return;
  }
  const char* p = line.c_str();
  if (line.startsWith("AT+N,")) p += 5;
  std::vector<int> counts;
  while (*p) {
    counts.push_back(atoi(p));
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
  }
  if (counts.size() < 5) {
    Log(ERR, "%sparseATNCommand: zu wenig Felder", _LOG_);
    return;
  }
  tempNPerimeter = counts[0];
  tempNExclusions = counts[1];
  tempNDockpoints = counts[2];
  tempNWaypoints = counts[3];
  tempMapCountsReceived = true;
  tempExclusionSizes.clear();

  if (tempNExclusions == 0) {
    finalizeInterceptedMap();
  }
}

// AT+X Kommando: AT+X,<startIdx>,<len1>,<len2>,...
void MowerAdapter::parseATXCommand(const String& line) {
  Log(DBG, "%sparseATXCommand (exclusion sizes)", _LOG_);
  int waitCount = 0;
  while (_map.isReading() && waitCount < 10) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    waitCount++;
  }
  if (_map.isReading()) {
    Log(WARN, "%sparseATXCommand: Map-Lesevorgang läuft noch, ignoriere AT-X", _LOG_);
    return;
  }
  const char* p = line.c_str();
  if (line.startsWith("AT+X,")) p += 5;
  std::vector<int> values;
  while (*p) {
    values.push_back(atoi(p));
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
  }
  if (values.size() < 2) {
    Log(ERR, "%sparseATXCommand: zu wenig Felder", _LOG_);
    return;
  }
  // values[0] = Startindex, danach folgen die Polygonlängen
  for (size_t i = 1; i < values.size(); i++) {
    tempExclusionSizes.push_back(values[i]);
  }

  if (tempMapCountsReceived) {
    int sum = 0;
    for (int s : tempExclusionSizes) sum += s;
    if (sum == tempNExclusions) {
      finalizeInterceptedMap();
    }
  }
}

void MowerAdapter::finalizeInterceptedMap() {
  using namespace ArduMower::Domain::Robot;
  Log(DBG, "%sfinalizeInterceptedMap: perimeter=%d exclusions=%d dock=%d waypoints=%d",
      _LOG_, tempNPerimeter, tempNExclusions, tempNDockpoints, tempNWaypoints);

  _map.perimeter.clear();
  _map.exclusions.clear();
  _map.dockpoints.clear();
  _map.waypoints.clear();

  int idx = 0;
  // Perimeter
  for (int i = 0; i < tempNPerimeter && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.perimeter.push_back(tempWaypointsBuffer[idx]);
  }
  // Exclusions anhand der AT+X-Längen aufteilen
  for (size_t ex = 0; ex < tempExclusionSizes.size(); ex++) {
    std::vector<MapPoint> excl;
    for (int j = 0; j < tempExclusionSizes[ex] && idx < (int)tempWaypointsBuffer.size(); j++, idx++) {
      excl.push_back(tempWaypointsBuffer[idx]);
    }
    _map.exclusions.push_back(excl);
  }
  // Dockpoints
  for (int i = 0; i < tempNDockpoints && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.dockpoints.push_back(tempWaypointsBuffer[idx]);
  }
  // Waypoints
  for (int i = 0; i < tempNWaypoints && idx < (int)tempWaypointsBuffer.size(); i++, idx++) {
    _map.waypoints.push_back(tempWaypointsBuffer[idx]);
  }

  // Summen prüfen
  bool ok = true;
  int totalExclPoints = 0;
  for (const auto &ex : _map.exclusions) totalExclPoints += ex.size();
  if ((int)_map.perimeter.size() != tempNPerimeter) {
    Log(ERR, "%sfinalizeInterceptedMap: perimeter count mismatch %d != %d", _LOG_, _map.perimeter.size(), tempNPerimeter);
    ok = false;
  }
  if (totalExclPoints != tempNExclusions) {
    Log(ERR, "%sfinalizeInterceptedMap: exclusion point count mismatch %d != %d", _LOG_, totalExclPoints, tempNExclusions);
    ok = false;
  }
  if ((int)_map.exclusions.size() != (int)tempExclusionSizes.size()) {
    Log(ERR, "%sfinalizeInterceptedMap: exclusion polygon count mismatch %d != %d", _LOG_, _map.exclusions.size(), tempExclusionSizes.size());
    ok = false;
  }
  if ((int)_map.dockpoints.size() != tempNDockpoints) {
    Log(ERR, "%sfinalizeInterceptedMap: dockpoints count mismatch %d != %d", _LOG_, _map.dockpoints.size(), tempNDockpoints);
    ok = false;
  }
  if ((int)_map.waypoints.size() != tempNWaypoints) {
    Log(ERR, "%sfinalizeInterceptedMap: waypoints count mismatch %d != %d", _LOG_, _map.waypoints.size(), tempNWaypoints);
    ok = false;
  }
  if (!ok) {
    Log(ERR, "%sfinalizeInterceptedMap: Map-Übertragung fehlerhaft, Abbruch", _LOG_);
    tempMapCountsReceived = false;
    tempExclusionSizes.clear();
    return;
  }
  Log(DBG, "%sfinalizeInterceptedMap: Map vollständig, sende an Client", _LOG_);
  _map.timestamp = millis();
  updateCurrentMapMeta();

  // Abgefangene Karte anhand des Hashes mit gespeicherten Karten abgleichen
  String existingId = _mapManager.findByHash(_currentMapHash);
  if (existingId.length() > 0) {
    _mapManager.setActive(existingId);
    Log(INFO, "%sfinalizeInterceptedMap: abgefangene Karte passt zu gespeicherter ID %s", _LOG_, existingId.c_str());
  } else {
    _mapManager.setActive("");
    Log(INFO, "%sfinalizeInterceptedMap: abgefangene Karte unbekannt (hash=%s), keine Auswahl", _LOG_, _currentMapHash.c_str());
  }
  _mapListDirty = true;
  tempWaypointsBuffer.clear();
  tempExclusionSizes.clear();
  tempMapCountsReceived = false;
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

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
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

void MowerAdapter::parseUbxResponse(const char* line)
{
  Log(DBG, "%sparseUbxResponse", _LOG_);
  const auto now = millis();

  const char* comma = strchr(line, ',');
  if (comma != NULL) {
    _ubxResponse.hexData = String(comma + 1);
  } else {
    _ubxResponse.hexData = "";
  }
  _ubxResponse.timestamp = now;
}
#endif

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

void MowerAdapter::parseStatisticsResponse(const char* line)
{
  Log(DBG, "%sparseStatisticsResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       (void)len;
                       switch (index)
                       {
                       case 1:
                         _stats.durations.idle = atoi(val);
                         break;
                       case 2:
                         _stats.durations.charge = atoi(val);
                         break;
                       case 3:
                         _stats.durations.mow = atoi(val);
                         break;
                       case 4:
                         _stats.durations.mowFloat = atoi(val);
                         break;
                       case 5:
                         _stats.durations.mowFix = atoi(val);
                         break;

                       case 6:
                         _stats.recoveries.mowFloatToFix = atoi(val);
                         break;

                        case 7:
                          _stats.mowDistanceTraveled = atof(val);
                          break;
                        case 8:
                          _stats.mowMaxDgpsAge = atof(val);
                          break;

                        case 9:
                          _stats.recoveries.imu = atoi(val);
                          break;

                        case 10:
                          _stats.tempMin = atof(val);
                          break;
                        case 11:
                          _stats.tempMax = atof(val);
                          break;

                        case 12:
                          _stats.gpsChecksumErrors = atoi(val);
                          break;
                        case 13:
                          _stats.dgpsChecksumErrors = atoi(val);
                          break;
                        case 14:
                          _stats.maxMotorControlCycleTime = atof(val);
                          break;
                        case 15:
                          _stats.serialBufferSize = atoi(val);
                          break;
                        case 16:
                          _stats.durations.mowInvalid = atoi(val);
                          break;
                        case 17:
                          _stats.recoveries.mowInvalid = atoi(val);
                          break;
                        case 18:
                          _stats.obstacles.count = atoi(val);
                          break;
                        case 19:
                          _stats.freeMemory = atoi(val);
                          break;
                        case 20:
                          _stats.resetCause = atoi(val);
                          break;
                        case 21:
                          _stats.gpsJumps = atoi(val);
                          break;
                        case 22:
                          _stats.obstacles.sonar = atoi(val);
                          break;
                        case 23:
                          _stats.obstacles.bumper = atoi(val);
                          break;
                        case 24:
                          _stats.obstacles.gpsMotionLow = atoi(val);
                          break;
                       }
                     });

  _stats.timestamp = now;
}

void MowerAdapter::parseSensorSummaryResponse(const char* line)
{
  //Log(DBG, "%sparseSensorSummaryResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       (void)len;
                       // S3,<left>,<center>,<right>,<sonarObs>,<sonarNear>,<bumpL>,<bumpR>,<bumpObs>,<bumpNear>,<lidarObs>,<lidarNear>,<lift>,<rain>
                       switch (index)
                       {
                       case 1:
                         _sensorSummary.sonarLeft = atof(val);
                         break;
                       case 2:
                         _sensorSummary.sonarCenter = atof(val);
                         break;
                       case 3:
                         _sensorSummary.sonarRight = atof(val);
                         break;
                       case 4:
                         _sensorSummary.sonarObstacle = atoi(val) == 1;
                         break;
                       case 5:
                         _sensorSummary.sonarNearObstacle = atoi(val) == 1;
                         break;
                       case 6:
                         _sensorSummary.bumperLeft = atoi(val) == 1;
                         break;
                       case 7:
                         _sensorSummary.bumperRight = atoi(val) == 1;
                         break;
                       case 8:
                         _sensorSummary.bumperObstacle = atoi(val) == 1;
                         break;
                       case 9:
                         _sensorSummary.bumperNearObstacle = atoi(val) == 1;
                         break;
                       case 10:
                         _sensorSummary.lidarObstacle = atoi(val) == 1;
                         break;
                       case 11:
                         _sensorSummary.lidarNearObstacle = atoi(val) == 1;
                         break;
                       case 12:
                         _sensorSummary.liftTriggered = atoi(val) == 1;
                         break;
                       case 13:
                         _sensorSummary.rainTriggered = atoi(val) == 1;
                         break;
                       }
                     });

  _sensorSummary.timestamp = now;
}

#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
void MowerAdapter::parseGpsDetailsResponse(const char* line)
{
  Log(DBG, "%sparseGpsDetailsResponse", _LOG_);
  const auto now = millis();

  _gpsDetails.timestamp = now;

  int satCount = 0;
  int fieldIdx = -1;

  // Erster Durchlauf: Header-Felder parsen und Gesamtzahl der Token ermitteln
  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       (void)len;
                       fieldIdx = index;
                       switch (index)
                       {
                       case 1:
                         _gpsDetails.numSV = atoi(val);
                         break;
                       case 2:
                         _gpsDetails.numSVdgps = atoi(val);
                         break;
                       case 3:
                         _gpsDetails.solution = atoi(val);
                         break;
                       case 4:
                         _gpsDetails.hAccuracy = atof(val);
                         break;
                       case 5:
                         _gpsDetails.vAccuracy = atof(val);
                         break;
                       case 6:
                         _gpsDetails.dgpsAge = atoi(val);
                         break;
                       case 7:
                         satCount = atoi(val);
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
  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       (void)len;
                       if (index < 8) return;

                       int satField = index - 8;
                       int satIdx = satField / fieldsPerSat;
                       int col = satField % fieldsPerSat;

                       if (satIdx >= satCount || satIdx >= 40) return;

                       if ((size_t)satIdx >= _gpsDetails.satellites.size())
                         _gpsDetails.satellites.resize(satIdx + 1);

                       auto& sat = _gpsDetails.satellites[satIdx];
                       switch (col) {
                       case 0: sat.gnssId = atoi(val); break;
                       case 1: sat.svId = atoi(val); break;
                       case 2: sat.sigId = atoi(val); break;
                       case 3: sat.cno = atoi(val); break;
                       case 4: sat.qualityInd = atoi(val); break;
                       case 5: sat.prUsed = atoi(val) == 1; break;
                       case 6: sat.crCorrUsed = atoi(val) == 1; break;
                       case 7: sat.prRes = atof(val); break;
                       case 8: sat.elevation = (int8_t)atoi(val); break;
                       case 9: sat.azimuth = (int8_t)atoi(val); break;
                       }
                     });

  Log(DBG, "%sparseGpsDetailsResponse done sats=%d/%d fields=%d fps=%d",
      _LOG_, (int)_gpsDetails.satellites.size(), satCount, fieldIdx, fieldsPerSat);
}
#endif

void MowerAdapter::parseVersionResponse(const char* line)
{
  Log(DBG, "%sparseVersionResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       // V,Ardumower Sunray,1.0.189,1,73,0x58\r
                       switch (index)
                       {
                       case 1:
                         _props.firmware = String(val, len);
                         break;
                       case 2:
                         _props.version = String(val, len);
                         break;
                       case 3:
                         enc.setOn(atoi(val) == 1);
                         break;
                       case 4:
                         enc.setChallenge(atoi(val));
                         break;
                       }
                     });

  _props.timestamp = now;
  sendIsInitialized = true;
}

void MowerAdapter::parseStateResponse(const char* line)
{
  Log(DBG, "%sparseStateResponse", _LOG_);
  const auto now = millis();

  processCSVResponse(line, [&](int index, const char* val, size_t len)
                     {
                       (void)len;
                       switch (index)
                       {
                       case 1:
                         _state.batteryVoltage = atof(val);
                         break;
                       case 2:
                         _state.position.x = atof(val);
                         break;
                       case 3:
                         _state.position.y = atof(val);
                         break;
                       case 4:
                         _state.position.delta = atof(val);
                         break;
                       case 5:
                         _state.position.solution = atoi(val);
                         break;
                       case 6:
                         _state.job = atoi(val);
                         break;
                       case 7:
                         _state.position.mowPointIndex = atoi(val);
                         break;
                       case 8:
                         _state.position.age = atof(val);
                         break;
                       case 9:
                         _state.sensor = atoi(val);
                         break;

                       case 10:
                         _state.target.x = atof(val);
                         break;
                       case 11:
                         _state.target.y = atof(val);
                         break;

                       case 12:
                         _state.position.accuracy = atof(val);
                         break;
                       case 13:
                         _state.position.visibleSatellites = atoi(val);
                         break;
                       case 14:
                         _state.amps = atof(val);
                         break;
                       case 15:
                         _state.position.visibleSatellitesDgps = atoi(val);
                         break;
                       case 16:
                         _state.mapCrc = atoi(val);
                         break;
                       
                       // incompatible to sunray upstream
                       case 20:
                         _state.chargingMah = atof(val);
                         break;
                       case 21:
                         _state.motorLeftMah = atof(val);
                         break;
                       case 22:
                         _state.motorRightMah = atof(val);
                         break;
                       case 23:
                         _state.motorMowMah = atof(val);
                         break;
                       case 24:
                         _state.temperature = atof(val);
                         break;
                       }
                     });

  _state.timestamp = now;
}

void MowerAdapter::parseATWCommand(const String& line)
{
  Log(DBG, "%sparseATWCommand (waypoint-list)", _LOG_);
  if (_map.isReading()) {
    Log(DBG, "%sparseATWCommand: Map-Lesevorgang läuft, ignoriere", _LOG_);
    return;
  }
  const char* p = line.c_str();
  if (line.startsWith("AT+W,")) p += 5;
  std::vector<float> values;
  while (*p) {
    values.push_back(atof(p));
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
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

void MowerAdapter::parseATCCommand(const String& line)
{
  Log(DBG, "%sparseATCCommand", _LOG_);
  const auto now = millis();

  processCSVResponse(
      line,
      [&](int index, const char* val, size_t len)
      {
        (void)len;
        if (atoi(val) == -1)
          return;

        switch (index)
        {
        case 1:
          _desiredState.mowerMotorEnabled = atoi(val) == 1;
          break;
        case 2:
          _desiredState.op = atoi(val);
          break;
        case 3:
          _desiredState.speed = atof(val);
          break;
        case 4:
          _desiredState.fixTimeout = atoi(val);
          break;
        case 5:
          _desiredState.finishAndRestart = atoi(val) == 1;
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
  if (_mapUploadState.active || _mapUploadPending) {
    Log(WARN, "%suploadMapToMower: upload already in progress", _LOG_);
    return false;
  }

  _mapUploadPending = true;
  Log(INFO, "%suploadMapToMower: queued", _LOG_);
  return true;
}

void MowerAdapter::startMapUploadFromLoop()
{
  if (!_mapUploadPending) return;
  _mapUploadPending = false;

  if (_mapUploadState.active) {
    Log(WARN, "%sstartMapUploadFromLoop: upload already active", _LOG_);
    return;
  }

  _map.beginRead();
  // Temporäre Intercept-Buffer für die Dauer des eigenen Uploads zurücksetzen,
  // damit keine alten/inkonsistenten Zustände eine spätere App-Übertragung stören.
  tempWaypointsBuffer.clear();
  tempExclusionSizes.clear();
  tempMapCountsReceived = false;
  _mapUploadState.snapshot = _map;
  _mapUploadState.active = true;
  _mapUploadState.phase = MapUploadState::start;
  _mapUploadState.polygonIdx = 0;
  _mapUploadState.pointIdx = 0;
  _mapUploadState.chunkRetry = 0;
  _mapUploadState.totalPointsSent = 0;
  _mapUploadState.lastResponse = "";
  _mapUploadState.lastOk = false;
  _mapUploadState.waitingForResponse = false;
  _mapUploadState.lastCommandQueued = false;
  // _map bleibt im reading-Zustand, um parseATNCommand/setMap während des Uploads zu blockieren

  auto &snap = _mapUploadState.snapshot;
  Log(INFO, "%sstartMapUploadFromLoop: started (perimeter=%d exclusions=%d dock=%d waypoints=%d)",
      _LOG_, snap.perimeter.size(), snap.exclusions.size(), snap.dockpoints.size(), snap.waypoints.size());
  if (!snap.waypoints.empty()) {
    Log(INFO, "%sstartMapUploadFromLoop: first waypoint %.2f,%.2f  last waypoint %.2f,%.2f",
        _LOG_, snap.waypoints.front().X, snap.waypoints.front().Y,
        snap.waypoints.back().X, snap.waypoints.back().Y);
  }
  if (!snap.perimeter.empty()) {
    Log(INFO, "%sstartMapUploadFromLoop: first perimeter %.2f,%.2f  last perimeter %.2f,%.2f",
        _LOG_, snap.perimeter.front().X, snap.perimeter.front().Y,
        snap.perimeter.back().X, snap.perimeter.back().Y);
  }
}

bool MowerAdapter::sendMapChunkAsync(const std::vector<ArduMower::Domain::Robot::MapPoint> &pts, int baseIdx)
{
  if (_mapUploadState.pointIdx >= pts.size())
    return false;

  // Kleinere Chunks, damit der Sunray-Serial-Empfangspuffer nicht überläuft.
  const size_t chunkSize = 10;
  size_t endIdx = _mapUploadState.pointIdx + chunkSize;
  if (endIdx > pts.size()) endIdx = pts.size();

  char buf[512];
  int n = snprintf(buf, sizeof(buf), "AT+W,%d", baseIdx + _mapUploadState.pointIdx);
  // Zwei Nachkommastellen reichen Sunray (cm-Auflösung) und halten die Pakete kurz.
  for (size_t j = _mapUploadState.pointIdx; j < endIdx; j++) {
    int written = snprintf(buf + n, sizeof(buf) - n, ",%.2f,%.2f", pts[j].X, pts[j].Y);
    if (written < 0 || (size_t)(n + written) >= sizeof(buf)) break;
    n += written;
  }
  String cmd(buf);

  _mapUploadState.lastBaseIdx = baseIdx;
  _mapUploadState.lastExpectedNextIdx = baseIdx + (int)endIdx;
  Log(INFO, "%ssendMapChunkAsync: %s (expect next W,%d)", _LOG_, cmd.c_str(), _mapUploadState.lastExpectedNextIdx);

  bool queued = sendCommandWithResponseAsync(cmd, [&](String response, bool ok) {
    _mapUploadState.lastResponse = response;
    _mapUploadState.lastOk = ok;
    _mapUploadState.waitingForResponse = false;
  }, true, 5000);

  if (!queued) {
    Log(WARN, "%ssendMapChunkAsync: router busy, will retry", _LOG_);
    _mapUploadState.lastCommandQueued = false;
    return true; // stay in current phase, retry next loop
  }

  _mapUploadState.lastCommandQueued = true;
  _mapUploadState.waitingForResponse = true;
  return true;
}

static bool responseOkForPhase(int phase, const String &response)
{
  if (response.length() == 0) return true;
  switch (phase) {
    case ArduMower::Modem::MapUploadState::perimeter:
    case ArduMower::Modem::MapUploadState::exclusions:
    case ArduMower::Modem::MapUploadState::dockpoints:
    case ArduMower::Modem::MapUploadState::waypoints:
      return response.startsWith("W");
    case ArduMower::Modem::MapUploadState::counts:
      return response.startsWith("N");
    case ArduMower::Modem::MapUploadState::exclusionSizes:
      return response.startsWith("X");
    default:
      return true;
  }
}

static bool isPointUploadPhase(int phase)
{
  switch (phase) {
    case ArduMower::Modem::MapUploadState::perimeter:
    case ArduMower::Modem::MapUploadState::exclusions:
    case ArduMower::Modem::MapUploadState::dockpoints:
    case ArduMower::Modem::MapUploadState::waypoints:
      return true;
    default:
      return false;
  }
}

static int parseResponseIndex(int phase, const String &response)
{
  if (!response.startsWith("W") && !response.startsWith("N") && !response.startsWith("X"))
    return -1;
  int comma = response.indexOf(',');
  if (comma < 0) return -1;
  int idx = comma + 1;
  while (idx < response.length() && response[idx] == ' ') idx++;
  if (idx >= response.length() || !isdigit((unsigned char)response[idx])) return -1;
  int value = 0;
  while (idx < response.length() && isdigit((unsigned char)response[idx])) {
    value = value * 10 + (response[idx] - '0');
    idx++;
  }
  return value;
}

void MowerAdapter::processMapUpload()
{
  if (!_mapUploadState.active)
    return;

  if (_mapUploadState.waitingForResponse) {
    processPendingCommand();
    return;
  }

  using namespace ArduMower::Domain::Robot;

  // Evaluate response of previous command (except for start phase)
  if (_mapUploadState.phase != MapUploadState::start && _mapUploadState.lastCommandQueued) {
    _mapUploadState.lastCommandQueued = false;
    _mapUploadState.lastResponse.trim();
    bool responseValid = _mapUploadState.lastOk && responseOkForPhase(_mapUploadState.phase, _mapUploadState.lastResponse);
    if (responseValid && isPointUploadPhase(_mapUploadState.phase)) {
      int respIdx = parseResponseIndex(_mapUploadState.phase, _mapUploadState.lastResponse);
      if (respIdx >= 0 && respIdx != _mapUploadState.lastExpectedNextIdx) {
        Log(WARN, "%sprocessMapUpload: response index mismatch in phase %d (expected %d, got %d)",
            _LOG_, _mapUploadState.phase, _mapUploadState.lastExpectedNextIdx, respIdx);
        responseValid = false;
      }
    }
    if (!responseValid) {
      _mapUploadState.chunkRetry++;
      Log(WARN, "%sprocessMapUpload: command failed in phase %d (retry %d/3)", _LOG_, _mapUploadState.phase, _mapUploadState.chunkRetry);
      if (_mapUploadState.chunkRetry >= 3) {
        Log(ERR, "%sprocessMapUpload: command failed in phase %d after 3 retries", _LOG_, _mapUploadState.phase);
        _map = _mapUploadState.snapshot;
        _map.timestamp = millis();
        _map.endRead();
        _mapUploadState.snapshot = ArduMower::Domain::Robot::MowerMap();
        _mapUploadState.phase = MapUploadState::error;
        _mapUploadState.active = false;
        return;
      }
      // Resend same chunk (pointIdx was not advanced)
      _mapUploadState.lastResponse = "";
    } else {
      // Success: advance pointIdx for chunk phases
      _mapUploadState.chunkRetry = 0;
      switch (_mapUploadState.phase) {
        case MapUploadState::perimeter:
        case MapUploadState::exclusions:
        case MapUploadState::dockpoints:
        case MapUploadState::waypoints:
          // Anhand der vom Mower bestätigten Indizes weiterschalten,
          // damit Chunk-Größe nicht aus der Sync gerät.
          _mapUploadState.pointIdx = _mapUploadState.lastExpectedNextIdx - _mapUploadState.lastBaseIdx;
          break;
        default:
          break;
      }
    }
  }

  switch (_mapUploadState.phase) {
    case MapUploadState::start:
      Log(INFO, "%sprocessMapUpload: uploading map...", _LOG_);
      _mapUploadState.phase = MapUploadState::perimeter;
      _mapUploadState.pointIdx = 0;
      _mapUploadState.chunkRetry = 0;
      _mapUploadState.totalPointsSent = 0;
      break;

    case MapUploadState::perimeter:
      if (_mapUploadState.pointIdx >= _mapUploadState.snapshot.perimeter.size()) {
        _mapUploadState.totalPointsSent += _mapUploadState.snapshot.perimeter.size();
        _mapUploadState.phase = MapUploadState::exclusions;
        _mapUploadState.polygonIdx = 0;
        _mapUploadState.pointIdx = 0;
        _mapUploadState.chunkRetry = 0;
        _mapUploadState.lastResponse = "";
        break;
      }
      sendMapChunkAsync(_mapUploadState.snapshot.perimeter, _mapUploadState.totalPointsSent);
      break;

    case MapUploadState::exclusions:
      if (_mapUploadState.polygonIdx >= _mapUploadState.snapshot.exclusions.size()) {
        _mapUploadState.phase = MapUploadState::dockpoints;
        _mapUploadState.pointIdx = 0;
        _mapUploadState.chunkRetry = 0;
        _mapUploadState.lastResponse = "";
        break;
      }
      if (_mapUploadState.pointIdx >= _mapUploadState.snapshot.exclusions[_mapUploadState.polygonIdx].size()) {
        _mapUploadState.totalPointsSent += _mapUploadState.snapshot.exclusions[_mapUploadState.polygonIdx].size();
        _mapUploadState.polygonIdx++;
        _mapUploadState.pointIdx = 0;
        _mapUploadState.chunkRetry = 0;
        _mapUploadState.lastResponse = "";
        break;
      }
      sendMapChunkAsync(_mapUploadState.snapshot.exclusions[_mapUploadState.polygonIdx], _mapUploadState.totalPointsSent);
      break;

    case MapUploadState::dockpoints:
      if (_mapUploadState.pointIdx >= _mapUploadState.snapshot.dockpoints.size()) {
        _mapUploadState.totalPointsSent += _mapUploadState.snapshot.dockpoints.size();
        _mapUploadState.phase = MapUploadState::waypoints;
        _mapUploadState.pointIdx = 0;
        _mapUploadState.chunkRetry = 0;
        _mapUploadState.lastResponse = "";
        break;
      }
      sendMapChunkAsync(_mapUploadState.snapshot.dockpoints, _mapUploadState.totalPointsSent);
      break;

    case MapUploadState::waypoints:
      if (_mapUploadState.pointIdx >= _mapUploadState.snapshot.waypoints.size()) {
        _mapUploadState.totalPointsSent += _mapUploadState.snapshot.waypoints.size();
        _mapUploadState.phase = MapUploadState::counts;
        _mapUploadState.lastResponse = "";
        break;
      }
      sendMapChunkAsync(_mapUploadState.snapshot.waypoints, _mapUploadState.totalPointsSent);
      break;

    case MapUploadState::counts: {
      size_t totalExclPoints = 0;
      for (const auto &excl : _mapUploadState.snapshot.exclusions)
        totalExclPoints += excl.size();
      String cmd = "AT+N," + String(_mapUploadState.snapshot.perimeter.size()) + "," + String(totalExclPoints) + ","
                  + String(_mapUploadState.snapshot.dockpoints.size()) + "," + String(_mapUploadState.snapshot.waypoints.size()) + ",0";
      bool queued = sendCommandWithResponseAsync(cmd, [&](String response, bool ok) {
        _mapUploadState.lastResponse = response;
        _mapUploadState.lastOk = ok;
        _mapUploadState.waitingForResponse = false;
      }, true, 5000);
      if (!queued) return;
      _mapUploadState.waitingForResponse = true;
      _mapUploadState.phase = MapUploadState::exclusionSizes;
      _mapUploadState.polygonIdx = 0;
      _mapUploadState.lastResponse = "";
      break;
    }

    case MapUploadState::exclusionSizes:
      if (_mapUploadState.polygonIdx >= _mapUploadState.snapshot.exclusions.size()) {
        _mapUploadState.phase = MapUploadState::finalizing;
        _mapUploadState.lastResponse = "";
        break;
      }
      {
        String cmd = "AT+X," + String(_mapUploadState.polygonIdx) + "," + String(_mapUploadState.snapshot.exclusions[_mapUploadState.polygonIdx].size());
        bool queued = sendCommandWithResponseAsync(cmd, [&](String response, bool ok) {
          _mapUploadState.lastResponse = response;
          _mapUploadState.lastOk = ok;
          _mapUploadState.waitingForResponse = false;
        }, true, 5000);
        if (!queued) return;
        _mapUploadState.waitingForResponse = true;
        _mapUploadState.polygonIdx++;
        _mapUploadState.lastResponse = "";
      }
      break;

    case MapUploadState::finalizing:
      _map = _mapUploadState.snapshot;
      _map.timestamp = millis();
      _map.endRead();
      updateCurrentMapMeta();
      _mapUploadState.snapshot = ArduMower::Domain::Robot::MowerMap();
      _mapUploadState.phase = MapUploadState::done;
      _mapUploadState.active = false;
      Log(INFO, "%sprocessMapUpload: Map upload complete (%d perimeter, %d exclusions, %d dockpoints, %d waypoints) CRC %d",
          _LOG_, _map.perimeter.size(), _map.exclusions.size(), _map.dockpoints.size(), _map.waypoints.size(), _currentMapCrc);
      Log(INFO, "%sprocessMapUpload: upload complete", _LOG_);
      break;

    case MapUploadState::done:
    case MapUploadState::error:
    case MapUploadState::idle:
    default:
      _mapUploadState.snapshot = ArduMower::Domain::Robot::MowerMap();
      _mapUploadState.active = false;
      break;
  }
}

void MowerAdapter::loop()
{
  startMapUploadFromLoop();
  processPendingCommand();
  processMapUpload();

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

bool MowerAdapter::sendCommand(const String& command, bool encrypt)
{
  Checksum chk;
  chk.update(command.c_str());

  char buf[256];
  snprintf(buf, sizeof(buf), "%s,0x%02x", command.c_str(), chk.value());
  Log(COMM, "> %s", buf);

  if (encrypt)
    enc.encrypt(buf, strlen(buf));

  return router.sendWithoutResponse(buf);
}

bool MowerAdapter::sendCommandWithResponseAsync(const String& command, std::function<void(String, bool)> callback, bool encrypt, int timeoutMs)
{
  if (_pendingCommand.active) {
    Log(WARN, "%ssendCommandWithResponseAsync: already busy", _LOG_);
    return false;
  }

  Checksum chk;
  chk.update(command.c_str());

  char buf[256];
  snprintf(buf, sizeof(buf), "%s,0x%02x", command.c_str(), chk.value());
  Log(COMM, "> %s", buf);

  if (encrypt)
    enc.encrypt(buf, strlen(buf));

  _pendingCommand.callback = callback;
  _pendingCommand.startTime = millis();
  _pendingCommand.timeoutMs = timeoutMs;
  _pendingCommand.response = "";
  _pendingCommand.ok = false;
  _pendingCommand.active = true;
  _pendingCommand.done = false;
  _pendingCommand.command = command;

  bool queued = router.send(buf, [&](String resp, XferError error) {
    _pendingCommand.response = resp;
    _pendingCommand.ok = (error == XferError::SUCCESS);
    _pendingCommand.done = true;
  });

  if (!queued) {
    _pendingCommand.active = false;
    return false;
  }

  return true;
}

void MowerAdapter::processPendingCommand()
{
  if (!_pendingCommand.active)
    return;

  router.loop();

  if (!_pendingCommand.done) {
    if ((int)(millis() - _pendingCommand.startTime) >= _pendingCommand.timeoutMs) {
      Log(WARN, "%sprocessPendingCommand timeout for: %s", _LOG_, _pendingCommand.command.c_str());
      _pendingCommand.ok = false;
      _pendingCommand.done = true;
    }
    return;
  }

  auto callback = _pendingCommand.callback;
  String response = _pendingCommand.response;
  bool ok = _pendingCommand.ok;
  _pendingCommand.active = false;
  _pendingCommand.callback = nullptr;

  if (callback)
    callback(response, ok);
}

bool MowerAdapter::sendCommandWithResponse(const String& command, String &response, bool encrypt, int timeoutMs)
{
  bool done = false;
  bool ok = sendCommandWithResponseAsync(command, [&](String resp, bool success) {
    response = resp;
    done = true;
    ok = success;
  }, encrypt, timeoutMs);

  if (!ok) return false;

  uint32_t start = millis();
  while (!done && (int)(millis() - start) < timeoutMs) {
    processPendingCommand();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  if (!done) {
    Log(WARN, "%ssendCommandWithResponse timeout for: %s", _LOG_, command.c_str());
    _pendingCommand.active = false;
    _pendingCommand.callback = nullptr;
    return false;
  }

  return ok;
}

void processCSVResponse(const char* res, std::function<void(int, const char*, size_t)> fn)
{
  int index = -1;
  const char* p = res;

  while (*p)
  {
    index++;
    const char* comma = strchr(p, ',');
    size_t len = comma ? (size_t)(comma - p) : strlen(p);
    fn(index, p, len);

    p += len;
    if (*p == ',') p++;
  }
}

void processCSVResponse(const String& res, std::function<void(int, const char*, size_t)> fn)
{
  processCSVResponse(res.c_str(), fn);
}

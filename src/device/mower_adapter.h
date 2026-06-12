#ifndef _MOWER_ADAPTER_H
#define _MOWER_ADAPTER_H

#include "domain.h"
#include "mower_map.h"
#include "router.h"
#include "encrypt.h"
#include "settings.h"

namespace ArduMower
{
  namespace Modem
  {
    class MowerAdapter : public RxDrain, public TxDrain, public ArduMower::Domain::Robot::StateSource, public ArduMower::Domain::Robot::CommandExecutor {
    private:
      Settings::Settings &settings;
      Router &router;
      bool sendIsInitialized;
      Encrypt enc;
      ArduMower::Domain::Robot::Properties _props;
      ArduMower::Domain::Robot::State::State _state;
      ArduMower::Domain::Robot::Stats::Stats _stats;
      ArduMower::Domain::Robot::DesiredState _desiredState;
      ArduMower::Domain::Robot::SensorSummary _sensorSummary;
      ArduMower::Domain::Robot::GpsDetails _gpsDetails;
      ArduMower::Domain::Robot::UbxResponse _ubxResponse;
      ArduMower::Domain::Robot::MowerMap _map;
      ArduMower::Domain::Robot::MowSettings _mowSettings;
      uint32_t _lastStateRequest = 0;
      uint32_t _lastStatsRequest = 0;
      // Cache für rohe Antwort-Strings (mit Checksumme) – für HTTP-Cache-Serving
      String _cachedRawState;
      String _cachedRawStats;
      String _cachedRawSensorSummary;
      String _cachedRawGpsDetails;
      // Temporärer Buffer für alle empfangenen Wegpunkte (alle Typen)
      std::vector<ArduMower::Domain::Robot::MapPoint> tempWaypointsBuffer;

      void parseArduMowerCommand(String line);
      void parseArduMowerResponse(String line);
      void parseVersionResponse(String line);
      void parseStateResponse(String line);
      void parseStatisticsResponse(String line);
      void parseSensorSummaryResponse(String line);
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
      void parseGpsDetailsResponse(String line);
      void parseUbxResponse(String line);
#endif
      void parseATCCommand(String line);
      void parseATWCommand(String line);
      void parseATNCommand(String line);

      bool sendCommand(String command, bool encrypt = true);
      bool sendCommandWithResponse(String command, String &response, bool encrypt = true, int timeoutMs = 3000);
      bool assertSendIsInitialized();
      int containsNonUTF8(const String& input);
      String bytesToHexString(const String& byteString);
    public:
      MowerAdapter(Settings::Settings &_settings, Router &_router);
      void begin();
      virtual ArduMower::Domain::Robot::Properties props() { return _props; }
      virtual ArduMower::Domain::Robot::State::State state() { return _state; };
      virtual ArduMower::Domain::Robot::Stats::Stats stats() { return _stats; };
      virtual ArduMower::Domain::Robot::DesiredState desiredState() { return _desiredState; }
      virtual ArduMower::Domain::Robot::SensorSummary sensorSummary() { return _sensorSummary; }
      virtual ArduMower::Domain::Robot::GpsDetails gpsDetails() { return _gpsDetails; }
      virtual ArduMower::Domain::Robot::UbxResponse ubxResponse() { return _ubxResponse; }
      virtual ArduMower::Domain::Robot::Properties *propsP() { return &_props; }
      virtual ArduMower::Domain::Robot::State::State *stateP() { return &_state; };
      virtual ArduMower::Domain::Robot::Stats::Stats *statsP() { return &_stats; }
      virtual ArduMower::Domain::Robot::DesiredState *desiredStateP() { return &_desiredState; }
      virtual ArduMower::Domain::Robot::SensorSummary *sensorSummaryP() { return &_sensorSummary; }
      virtual ArduMower::Domain::Robot::GpsDetails *gpsDetailsP() { return &_gpsDetails; }
      virtual ArduMower::Domain::Robot::UbxResponse *ubxResponseP() { return &_ubxResponse; }
      virtual ArduMower::Domain::Robot::MowerMap mowerMap() { return _map; }
      virtual ArduMower::Domain::Robot::MowSettings mowSettings() { return _mowSettings; }
      virtual ArduMower::Domain::Robot::MowSettings *mowSettingsP() { return &_mowSettings; }
      // Zugriff auf gecachte rohe Antworten (für HTTP-Cache)
      virtual String cachedRawState() { return _cachedRawState; }
      virtual String cachedRawStats() { return _cachedRawStats; }
      virtual String cachedRawSensorSummary() { return _cachedRawSensorSummary; }
      virtual String cachedRawGpsDetails() { return _cachedRawGpsDetails; }
      virtual bool changeSpeed(float speed);
      virtual bool dock();
      virtual bool finishAndRestartEnabled(bool enabled);
      virtual bool mowerEnabled(bool enabled);
      virtual bool setFixTimeout(int timeout);
      virtual bool setWaypoint(float waypoint);
      virtual bool skipWaypoint();
      virtual bool start();
      virtual bool stop();
      virtual bool sonarEnabled(bool enabled);
      virtual bool requestVersion();
      virtual bool requestStatus();
      virtual bool requestStatusNow();
      virtual bool requestStats();
      virtual bool requestStatsNow();
      virtual bool requestSensorSummary();
#if defined(ENABLE_LIVE_MAP) || defined(ENABLE_GPS_DASHBOARD)
      virtual bool requestGpsDetails();
      virtual bool sendUbx(const String &hexCmd);
#endif
      virtual bool manualDrive(float linear, float angular);
      virtual bool navigateTo(float x, float y);
      virtual bool reboot();
      virtual bool rebootGPS();
      virtual bool powerOff();
      virtual bool uploadMapToMower() override;
      virtual bool customCmd(String cmd);
      virtual void drainRx(String line, bool &stop) override;
      virtual void drainTx(String line, bool &stop) override;
      virtual void setMap(const ArduMower::Domain::Robot::MowerMap &map);
      virtual void setMowSettings(const ArduMower::Domain::Robot::MowSettings &s);
      virtual void loop();
    };
  }
}
#endif

#include <Arduino.h>

// used by the CMakeLists.txt file to create a different compile target for test execution
#ifdef ESP_MODEM_TEST
#define DEBUG_LEVEL DBG
#include "../test/test_main.h"
#else

#include "git_version.h"

#include "log.h"
#ifdef ESP_MODEM_SIM
#include "../test/helper/fake_ardumower.h"
#endif

#include "api.h"
#include "ble_adapter.h"
#include "console.h"
#include "esp_os.h"
#include "http_adapter.h"
#include "looptime_monitor.h"
#include "modem_cli.h"
#include "mower_adapter.h"
#include "mqtt_adapter.h"
#include "ota.h"
#include "ota_arduinoota.h"
#include "ota_http_server.h"
#include "ota_mower_updater.h"
#include "prometheus_adapter.h"
#include "terminal.h"
#include "router.h"
#include "settings.h"
#include "ui_adapter.h"
#include "ui_adapter_socket.h"
#include "web_server.h"
#include "wifi_adapter.h"

#ifdef ENABLE_PS4_CONTROLLER
#include "ps4_controller.h"
#endif

using namespace ArduMower::Modem;

// encapsulate ESP32 APIs like "reboot" to allow testing
auto espOs = ArduinoESP();
Api::Api api(espOs);

// user settings read/update with JSON file on SPIFFS as backend
Settings::Settings settings;
// client or access point based on user settings & connection errors
Wifi::Adapter wifiAdapter(settings);

#ifdef ESP_MODEM_SIM
// console is used during validation tests and enabled on simulator target only
Console con(Serial, api, settings);
#elif MOWER_TERMINAL
// establishes a terminal connection to the mover via web interface
Terminal terminal(Serial1);
#endif


WebServer webServer;

// firmware update via Arduino IDE with buggy WiFiUDP
Ota::ArduinoOta ota(settings);
#ifdef MOWER_TERMINAL
Ota::MowerUpdater mowerUpdater(terminal, Serial1);
#else
Ota::MowerUpdater mowerUpdater(Serial1);
#endif
Ota::HttpServer otaHttpServer(settings, webServer.server(), mowerUpdater);


// provides request/response communication with the robot
Router router(Serial2);
// Bluetooth LE endpoint for app
BleAdapter bleAdapter(settings, router);
// emulates the behavior of the previous generation Bluetooth UART modules
Cli modemCli(router);
// ArduMower protocol interpreter, keeps state
MowerAdapter mowerAdapter(settings, router);
// HTTP endpoint for app
HttpAdapter httpAdapter(router, webServer.server(), mowerAdapter);
// serves the web frontend
Http::UiAdapter ui(api, settings, webServer.server(), mowerAdapter, mowerAdapter);
// handle socket connection with web client
#ifdef MOWER_TERMINAL
Http::UiSocketHandler socketHandler(terminal, webServer.server(), mowerAdapter, mowerAdapter, mowerUpdater);
#else
Http::UiSocketHandler socketHandler(webServer.server(), mowerAdapter, mowerAdapter, mowerUpdater);
#endif
// publish state on MQTT, receive commands on MQTT
MqttAdapter mqttAdapter(settings, router, mowerAdapter, mowerAdapter);
// metrics available for use with Prometheus
Prometheus::Adapter prometheusAdapter(settings, webServer.server(), mowerAdapter, mowerAdapter);
Prometheus::LooptimeMonitor looptime;

#ifdef ENABLE_PS4_CONTROLLER
PS4controller::Adapter ps4ControllerAdapter(settings, mowerAdapter, mowerAdapter); 
#endif

void setup() {
  Serial.begin(115200, SERIAL_8N1);

  Serial2.setRxBufferSize(4096); // must be called before begin(), default 256 overflows with UBX responses

  #if defined(ROUTER_TX_PIN) && defined(ROUTER_RX_PIN)
    Serial2.begin(115200, SERIAL_8N1, ROUTER_RX_PIN, ROUTER_TX_PIN); // self defined pins
  #elif CONFIG_IDF_TARGET_ESP32S3
    Serial2.begin(115200, SERIAL_8N1); // ESP32-S3
  #else
    Serial2.begin(115200, SERIAL_8N1); // ESP32
  #endif

  api.begin(&bleAdapter);
  settings.begin();
#ifdef ESP_MODEM_SIM
  con.begin();
#elif MOWER_TERMINAL
  terminal.begin();
#endif
  wifiAdapter.begin();
  router.begin();
  modemCli.begin();
  ota.begin();
  otaHttpServer.begin();
  otaHttpServer.onFlashProgress = [&](size_t cur, size_t tot){ socketHandler.broadcastFlashProgress(cur, tot); };
  bleAdapter.begin();
  httpAdapter.begin();
  mowerAdapter.begin();
  mqttAdapter.begin();
  prometheusAdapter.begin();
  ui.begin();
  socketHandler.begin();
  webServer.begin();
#ifdef ENABLE_PS4_CONTROLLER
  ps4ControllerAdapter.begin();
#endif

#ifdef ESP_MODEM_SIM
  looptime.add("con", std::bind(&Console::loop, &con));
#elif MOWER_TERMINAL
  looptime.add("terminal", std::bind(&Terminal::loop, &terminal));
#endif
  looptime.add("wifi", [&](){wifiAdapter.loop();});
  looptime.add("http", [&](){ if (!Ota::MowerUpdater::isFlashing()) httpAdapter.loop(); });
  looptime.add("ota_http", std::bind(&Ota::HttpServer::loop, &otaHttpServer));
  looptime.add("ota_mower", std::bind(&Ota::MowerUpdater::loop, &mowerUpdater));
  mowerUpdater.addIdleCallback(std::bind(&Router::loop, &router));
  looptime.add("ota_arduino", std::bind(&Ota::ArduinoOta::loop, &ota));
  looptime.add("flash_progress", [&](){
    static uint32_t last = 0;
    if (Ota::otaFlashTotal == 0) return;
    uint32_t now = millis();
    bool force = Ota::otaFlashForceSend;
    if (!force && now - last < 500) return;
    last = now;
    Ota::otaFlashForceSend = false;
    Log(DBG, "flash_progress: %u/%u", (unsigned)Ota::otaFlashProgress, (unsigned)Ota::otaFlashTotal);
    socketHandler.broadcastFlashProgress(Ota::otaFlashProgress, Ota::otaFlashTotal);
  });
  looptime.add("router", std::bind(&Router::loop, &router));
  looptime.add("mower", [&](){ if (!Ota::MowerUpdater::isFlashing()) mowerAdapter.loop(); });
  looptime.add("ble", [&](){ if (!Ota::MowerUpdater::isFlashing()) bleAdapter.loop(); });
  looptime.add("ui", [&](){ if (!Ota::MowerUpdater::isFlashing()) ui.loop(); });
  looptime.add("socket_handler", [&](){ if (!Ota::MowerUpdater::isFlashing()) socketHandler.loop(); });
  looptime.add("mqtt", [&](){ if (!Ota::MowerUpdater::isFlashing()) mqttAdapter.loop(millis()); });
#ifdef ENABLE_PS4_CONTROLLER
  looptime.add("ps4_controller", [&](){ if (!Ota::MowerUpdater::isFlashing()) ps4ControllerAdapter.loop(); });
#endif
  
#ifdef ESP_MODEM_SIM
  #if CONFIG_IDF_TARGET_ESP32S3
    Serial1.begin(115200, SERIAL_8N1); // loop to Serial2
  #elif
    Serial1.begin(115200, SERIAL_8N1, 23, 22); // loop to Serial2
  #endif
  setup_fake_ardumower();
  FakeArduMower.active = true;
#elif MOWER_TERMINAL
  Serial1.begin(115200, SERIAL_8N1); // connection to mower serial port
#endif
}

void loop() {
  looptime.loop();
  vPortYield();
}

#endif

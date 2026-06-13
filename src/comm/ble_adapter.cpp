#include "ble_adapter.h"
#include "log.h"

using namespace ArduMower::Modem;

BleAdapter::BleAdapter(Settings::Settings &s, Router &r)
    : settings(s), router(r),
      count(0), countMetric(
                    new Prometheus::CallbackMeasurement(
                        "ardumower_modem_ble_request_count",
                        std::bind(
                            &BleAdapter::writeCount,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2))),
      chr(NULL), status(BLE_STATUS_IDLE), expectNotify(0),
      bleReader("\n"), receivedFromBle(""), sendToBle(""),
      sendToMowerQueue()
{
}

unsigned int BleAdapter::writeCount(char *buffer, unsigned int size)
{
  return snprintf(buffer, size, "%u", count);
}

void BleAdapter::begin()
{
  if (!settings.bluetooth.enabled)
    return;

  BLEDevice::init(settings.general.name.c_str());
  if (settings.bluetooth.pin_enabled)
  {
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(settings.bluetooth.pin);
  } else {
    NimBLEDevice::setSecurityAuth(false, false, false);
  }

  auto server = BLEDevice::createServer();
  server->setCallbacks(this);

  auto service = server->createService(SERVICE_UUID);

  uint32_t props;
  if (settings.bluetooth.pin_enabled)
    props = NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::READ_ENC |
            NIMBLE_PROPERTY::READ_AUTHEN |
            NIMBLE_PROPERTY::WRITE_ENC |
            NIMBLE_PROPERTY::WRITE_AUTHEN |
            NIMBLE_PROPERTY::WRITE_NR;
  else
    props = NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_NR;
  chr = service->createCharacteristic(CHARACTERISTIC_UUID, props);
  chr->addDescriptor(new NimBLE2904());
  chr->setCallbacks(this);

  server->getAdvertising()->start();
}

void BleAdapter::loop()
{
  if (!settings.bluetooth.enabled)
    return;

  static int last_status = -1;
  if (status != last_status)
  {
    Log(DBG, "BleAdapter::loop::transition(%d -> %d)", last_status, status);
    last_status = status;
  }

  switch (status)
  {
  case BLE_STATUS_IDLE:
    startAdvertising();
    break;

  case BLE_STATUS_PAIRING:
    break;

  case BLE_STATUS_CONNECTED:
    if (receivedFromBle.length() > 0)
      status = BLE_STATUS_RXBLE;
    break;

  case BLE_STATUS_RXBLE:
    loopBleRx();
    break;

  case BLE_STATUS_TXROUTER:
    loopRouterTx();
    break;

  case BLE_STATUS_RXROUTER:
    break;

  case BLE_STATUS_TXBLE:
    loopBleTx();
    break;

  case BLE_STATUS_BLENOTIFY:
    break;
  }
}

void BleAdapter::clearPairings() {
  Log(INFO, "Disconecting BLE clients");
  auto server = NimBLEDevice::getServer();
  if (server) {
    auto devices = server->getPeerDevices();
    for (auto connHandle : devices) {
      server->disconnect(connHandle);
    }
  }

  NimBLEDevice::deleteAllBonds();
}

void BleAdapter::loopBleRx()
{
  while (receivedFromBle.length() > 0)
  {
    char c = receivedFromBle[0];
    receivedFromBle = receivedFromBle.substring(1);

    String line = bleReader.update(c);
    if (line != "")
    {
      count++;
      sendToMowerQueue.push_back(line);
      status = BLE_STATUS_TXROUTER;
      return;
    }
  }
}

void BleAdapter::loopBleTx()
{
  while (true)
  {
    const size_t n = sendToBle.length();
    if (n == 0)
    {
      status = BLE_STATUS_CONNECTED;
      return;
    }

    const size_t len = n > BLE_MTU ? BLE_MTU : n;
    String chunk = sendToBle.substring(0, len);
    sendToBle = sendToBle.substring(len);

    if (!chr->notify((uint8_t*)chunk.c_str(), len))
    {
      Log(ERR, "BleAdapter::loopBleTx::notify failed");
      return;
    }
  }
}

void BleAdapter::loopRouterTx()
{
  if (sendToMowerQueue.empty())
  {
    status = BLE_STATUS_CONNECTED;
    return;
  }

  String cmd = sendToMowerQueue.front();
  if (!router.send(cmd, [&](String res, int err)
                   {
                     sendToBle = res + "\r\n";
                     status = BLE_STATUS_TXBLE;
                   }))
    return;

  sendToMowerQueue.pop_front();
  status = BLE_STATUS_RXROUTER;
}

void BleAdapter::startAdvertising()
{
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->enableScanResponse(true);

  advertising->setMinInterval(0x06);
  advertising->setMaxInterval(0x12);

  BLEDevice::startAdvertising();

  if (status != BLE_STATUS_IDLE)
    return;

  status = BLE_STATUS_ADVERTISING;
}

void BleAdapter::reset()
{
  receivedFromBle = "";
  sendToBle = "";
  sendToMowerQueue.clear();
  bleReader.reset();
  expectNotify = 0;
}

#define BLE_MIN_INTERVAL 2
#define BLE_MAX_INTERVAL 10
#define BLE_LATENCY 0
#define BLE_TIMEOUT 30
void BleAdapter::onConnect(NimBLEServer *server, NimBLEConnInfo& connInfo)
{
  reset();

  server->updateConnParams(connInfo.getConnHandle(), BLE_MIN_INTERVAL, BLE_MAX_INTERVAL, BLE_LATENCY, BLE_TIMEOUT);
  if (!settings.bluetooth.pin_enabled)
  {
    status = BLE_STATUS_CONNECTED;
    return;
  }

  NimBLEDevice::startSecurity(connInfo.getConnHandle());
  status = BLE_STATUS_PAIRING;
}

void BleAdapter::onDisconnect(NimBLEServer *server, NimBLEConnInfo& connInfo, int reason)
{
  reset();
  status = BLE_STATUS_IDLE;
}

uint32_t BleAdapter::onPassKeyDisplay()
{
  Log(DBG, "BleAdapter::onPassKeyDisplay");
  if (!settings.bluetooth.pin_enabled)
    return 0;

  return settings.bluetooth.pin;
}

void BleAdapter::onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin)
{
  Log(DBG, "BleAdapter::onConfirmPassKey");
}

void BleAdapter::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo)
{
  Log(DBG, "BleAdapter::onMTUChange(%u)", MTU);

}

void BleAdapter::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
  Log(
    DBG, 
    "BleAdapter::onAuthenticationComplete(encrypted=%d,authenticated=%d,bonded=%d)", 
    connInfo.isEncrypted(),
    connInfo.isAuthenticated(),
    connInfo.isBonded()
  );

  if (!settings.bluetooth.pin_enabled)
    status = BLE_STATUS_CONNECTED;
  else if (connInfo.isEncrypted() && connInfo.isAuthenticated() && connInfo.isBonded())
    status = BLE_STATUS_CONNECTED;
  else
    Log(DBG, "BleAdapter::onAuthenticationComplete - rejected");
}

void BleAdapter::onWrite(NimBLECharacteristic *c, NimBLEConnInfo& connInfo)
{
  String val = c->getValue().c_str();

  receivedFromBle += val;

  if (status != BLE_STATUS_CONNECTED)
    return;

  status = BLE_STATUS_RXBLE;
}

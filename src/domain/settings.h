#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

static const char *defaultSettingsFilename = "/settings";

namespace ArduMower
{
  namespace Modem
  {
    namespace Settings
    {
      class Group
      {
      public:
        virtual void marshal(JsonObject o) const = 0;
        virtual bool unmarshal(JsonObject o) = 0;
        virtual void stripSecrets(const JsonObject &o) const = 0;

      protected:
        void stripSecret(const JsonObject &o, const char *key, const char *hasKey) const;
      };

      class General : public Group
      {
      public:
        General();

        String name;
        bool encryption;
        unsigned int password;

        bool valid(String & invalid) const;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

      class Web : public Group
      {
      public:
        bool use_password;
        String username;
        String password;

        Web() : use_password(false) {};

        bool valid(String & invalid) const;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

      class WiFi : public Group
      {
      public:
        constexpr static const char *default_ap_ssid = "ArduMower Modem";
        constexpr static const char *default_ap_psk = "ArduMower Modem";

        WiFi() : mode(0), ap_ssid(default_ap_ssid), ap_psk(default_ap_psk), sta_ip_mode(0) {}

        int mode; // 0=off 1=sta 2=ap
        String sta_ssid;
        String sta_psk;
        String ap_ssid;
        String ap_psk;
        int sta_ip_mode; // 0=dhcp 1=static
        String sta_ip;
        String sta_gateway;
        String sta_subnet;
        String sta_dns;

        bool valid(String & invalid) const;

        bool staSettingsValid() const
        {
          if (sta_ssid.length() == 0)
            return false;
          if (sta_psk.length() == 0)
            return false;

          return true;
        }

        bool apSettingsValid() const
        {
          if (ap_ssid.length() == 0)
            return false;
          if (ap_psk.length() == 0)
            return false;

          return true;
        }

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

      class Bluetooth : public Group
      {
      public:
        Bluetooth() : enabled(true), pin_enabled(false), pin(1234) {}
        bool enabled;
        bool pin_enabled;
        uint32_t pin;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

#ifdef ENABLE_PS4_CONTROLLER
      class PS4Controller : public Group
      {
      public:
        PS4Controller() : enabled(true), use_ps4_mac(true), ps4_mac("11:22:33:44:55:66") {}
        bool enabled;
        bool use_ps4_mac;
        String ps4_mac;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };
#endif
      class MQTT : public Group
      {
      public:
        MQTT() : enabled(false), ha(false), iob(false), publishStatus(true), publishFormat(0), publishInterval(30) {}

        bool enabled, ha, iob, publishStatus;
        String server, prefix, username, password;
        uint8_t publishFormat;
        uint32_t publishInterval;

        bool valid(String & invalid) const;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

      class Prometheus : public Group
      {
      public:
        Prometheus() : enabled(false) {}
        bool enabled;

        virtual void marshal(JsonObject o) const override;
        virtual bool unmarshal(JsonObject o) override;
        virtual void stripSecrets(const JsonObject &o) const override;
      };

      class Settings
      {
      private:
        String _filename;

      public:
        unsigned long revision;
        General general;
        Web web;
        WiFi wifi;
        Bluetooth bluetooth;
#ifdef ENABLE_PS4_CONTROLLER
        PS4Controller ps4controller;
#endif
        MQTT mqtt;
        Prometheus prometheus;

        Settings(String filename);
        Settings() : Settings(defaultSettingsFilename) {}
        String filename() { return _filename; }

        void begin();
        bool save();

        bool valid(String & invalid) const;

        bool unmarshal(JsonObject o);
        void marshal(JsonObject o) const;
        void stripSecrets(const JsonObject &o) const;
      };

      class PropertiesClass
      {
      public:
        // only in JSON, set during marshal

        PropertiesClass();

        const char * version() const;

        void marshal(JsonObject o) const;
      private:
        bool initBluetooth() const;
        String getBTMacAddress() const;
      };

      extern PropertiesClass Properties;
    }
  }
}

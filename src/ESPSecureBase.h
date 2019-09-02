// ESP32 Secure Base
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

#include <ESPAsyncWiFiManager.h>
#include "ota.h"
#include "mqtt.h"
#include "CommandParser.h"

// ESBConfig manages the storage of Wifi and MQTT config parameters as well as updating the
// respective client libs. A singleton object needs to be allocated during set-up and remain
// in-memory "forever" because the client libs refer to its storage.
class ESBConfig {
public:

    // Configuration parameters that need to remain allocated for the duration of the app because
    // other classes, such as AsynMqttClient rely on that.
    //String ap_ssid;
    char ap_pass[17];
    char mqtt_server[41];
    char mqtt_port[6];
    char mqtt_ident[41];
    char mqtt_psk[41];

    ESBConfig()
        : initialized(false)
    {
        // clear entire arrays, this way we can use strncpy with sizeof-1 and be guaranteed a
        // terminating zero
        memset(ap_pass, 0, sizeof(ap_pass));
        memset(mqtt_server, 0, sizeof(mqtt_server));
        memset(mqtt_port, 0, sizeof(mqtt_port));
        memset(mqtt_ident, 0, sizeof(mqtt_ident));
        memset(mqtt_psk, 0, sizeof(mqtt_psk));
    }

//private:

    bool initialized; // true once the config has been read

    void read();
    void save();
};

// ESBWifiConfig manages the AsyncWifiManager in order to run the config AP and show the necessary
// configuration UI. It holds the storage for the Wifi manager and the HTTP server and needs to
// remain allocated while these are active. It can be deallocated once the setup is complete.
class ESBWifiConfig {
public:

    ESBWifiConfig(ESBConfig &c)
        : config(c)
        , saved(false)
        , initialized(false)
        , server(80)
        , wifiMan(&server, &dns)
        , custom_header("<h3>Configuration access point</h3>")
        , custom_ap_pass("ap-pass", "AP password", c.ap_pass, 16, "><span>access point password</span")
        , custom_header2("<h3>MQTT server</h3>")
        , custom_mqtt_server("mqtt-server", "hostname", c.mqtt_server, 40, "><span>server hostname or IP address</span")
        , custom_mqtt_port("mqtt-port", "port", c.mqtt_port, 5, "><span>server port</span")
        , custom_mqtt_ident("mqtt-ident", "user/identity", c.mqtt_ident, 40, "><span>user/identity for PSK</span")
        , custom_mqtt_psk("mqtt-psk", "pre-shared key", c.mqtt_psk, 40, "><span>pre-shared key: 32 hex digits</span")
    {
    }

    // connect to Wifi using WiFiManager, returns true if wifi is connected.
    // connectTimeout is num seconds to try and connect to wifi before setting up portal.
    // portalTimeout is num seconds to run portal before giving up.
    // connect blocks until either Wifi is connected or the portal times out.
    bool connect(int connectTimeout, int portalTimeout);

    // reconfig starts the configuration portal and blocks 4until the user hits the save button
    // or the portalTimeout (in seconds) expires. It returns true if the values were changed.
    // TODO: this has issues with crashing AsyncTCP stuff, ugh...
    //bool reconfig(int connectTimeout, int portalTimeout);

    // startPortal starts the configuration portal
    void startPortal();

    // stopPortal stops the configuration portal
    void stopPortal();

    // loop keeps the portal running (specifically: DNS server and scanning)
    void loop() { wifiMan.loop(); }

//private:

    ESBConfig &config;
    bool saved;
    bool initialized;

    DNSServer dns;
    AsyncWebServer server;
    AsyncWiFiManager wifiMan;

    // custom HTML to provide info about the custom parameters.
    AsyncWiFiManagerParameter custom_header;
    AsyncWiFiManagerParameter custom_ap_pass;
    AsyncWiFiManagerParameter custom_header2;
    AsyncWiFiManagerParameter custom_mqtt_server;
    AsyncWiFiManagerParameter custom_mqtt_port;
    AsyncWiFiManagerParameter custom_mqtt_ident;
    AsyncWiFiManagerParameter custom_mqtt_psk;

    void init(int connectTimeout, int portalTimeout);
    void save();
};

class ESBCLI {
public:
    ESBCLI(ESBConfig &c, CommandParser &cp)
      : cmdParser(cp)
      , config(c)
    {}

    void init();

    void loop() {
        // Check if user typed something into the serial monitor
        cmdParser.loop();
    }

//private:
    void cmdErrorCB(CommandParser &cp, const char *cmd);
    void cmdWifiCB(CommandParser &cp, const char *cmd);
    void cmdMqttCB(CommandParser &cp, const char *cmd);
    void cmdRestartCB(CommandParser &cp, const char *cmd);
    CommandParser &cmdParser;
    ESBConfig &config;
};

class ESBDebug {
public:
    ESBDebug(CommandParser &cp)
    {
        using namespace std::placeholders; // for the _1, _2, ...
        cp.addCommand("debug", std::bind(&ESBDebug::debugCmdCB, this, _1, _2));
    }

//private: // private but not enforced

    void debugCmdCB(CommandParser &cp, const char *cmd);

    //CommandParser &_cp;
};

class ESBVar {
public:
    const char *_name;
    void       *_ref;
    ESBVar     *_next;
    uint8_t    _size;

    static ESBVar *_first;

    ESBVar(const char *name, void *ref, int size)
      : _name(name)
      , _ref(ref)
      , _size(size)
    {
        _next = _first;
        _first = this;
    }

    static void list();

    static ESBVar *find(const char *name);

    uint32_t read() {
        switch(_size) {
        case 1: return (uint32_t)( *(uint8_t*)_ref );
        case 2: return (uint32_t)( *(uint16_t*)_ref );
        case 4: return (uint32_t)( *(uint32_t*)_ref );
        default: return 0;
        }
    }
};

#define DV(v) static ESBVar __ ## v(#v, &v, sizeof(v))

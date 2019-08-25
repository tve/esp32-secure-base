// ESP32 Secure Base
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

#include <ESPAsyncWiFiManager.h>
#include "ota.h"
#include "mqtt.h"

class ESBConfig {
public:

    // "custom" parameters that we ask the WIFiManager to capture for us.
    char ap_pass[17];
    char mqtt_server[41];
    char mqtt_port[6];
    char mqtt_ident[41];
    char mqtt_psk[41];
    String ap_ssid;

    ESBConfig()
        : ap_pass("")
        , mqtt_server("")
        , mqtt_port("8883")
        , mqtt_psk("")
        , initialized(false)
        , saved(false)
        , server(80)
        , wifiMan(&server, &dns)
        , custom_header("<h3>Configuration access point</h3>")
        , custom_ap_pass("ap-pass", "AP password", ap_pass, 16, "><span>access point password</span")
        , custom_header2("<h3>MQTT server</h3>")
        , custom_mqtt_server("mqtt-server", "hostname", mqtt_server, 40, "><span>server hostname or IP address</span")
        , custom_mqtt_port("mqtt-port", "port", mqtt_port, 5, "><span>server port</span")
        , custom_mqtt_ident("mqtt-ident", "user/identity", mqtt_ident, 40, "><span>user/identity for PSK</span")
        , custom_mqtt_psk("mqtt-psk", "pre-shared key", mqtt_psk, 40, "><span>pre-shared key: 32 hex digits</span")
    {}

    // set-up Wifi using WiFiManager, returns true if wifi is connected.
    // connectTimeout is num seconds to try and connect to wifi before setting up portal.
    // portalTimeout is num seconds to run portal before giving up (e.g. to reset device).
    bool setup(int connectTimeout, int portalTimeout);

    // reconfig starts the configuration portal and blocks until the user hits the save button
    // or the portalTimeout (in seconds) expires. It returns true if the values were changed.
    bool reconfig(int connectTimeout, int portalTimeout);


//private:

    bool initialized;
    bool saved;

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
    void config_read();
    void config_save();
    void onSave();
};

#if 0
// custom HTML to provide info about the custom parameters.
AsyncWiFiManagerParameter custom_header("<h3>Configuration access point</h3>");
AsyncWiFiManagerParameter custom_ap_pass("ap-pass", "AP password", ap_pass, 16,
        "><span>access point password</span");
AsyncWiFiManagerParameter custom_header2("<h3>MQTT server</h3>");
AsyncWiFiManagerParameter custom_mqtt_server("mqtt-server", "hostname", mqtt_server, 40,
        "><span>server hostname or IP address</span");
AsyncWiFiManagerParameter custom_mqtt_port("mqtt-port", "port", mqtt_port, 5,
        "><span>server port</span");
AsyncWiFiManagerParameter custom_mqtt_psk("mqtt-psk", "pre-shared key", mqtt_psk, 40,
        "><span>pre-shared key: 32 hex digits</span");
#endif

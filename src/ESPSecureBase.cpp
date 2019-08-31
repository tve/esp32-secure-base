#include <ESPSecureBase.h>
#include <functional>

void ESBCLI::cmdWifiCB(CommandParser *cp, const char *cmd) {
    char *subCmd = cp->getArg();
    if (subCmd && strcmp(subCmd, "connect") == 0) {
        char *ssid = cp->getArg();
        char *pass = cp->getArg();
        if (!ssid || *ssid == 0) {
            printf("Usage: wifi connect <ssid> [<pass>]\n");
        } else if (pass && strlen(pass) < 8) {
            printf("Wifi: password must be at least 8 chars long, got %d\n", strlen(pass));
        } else {
            printf("Wifi: connecting to %s/%s\n", ssid, pass?pass:"-no-pass-");
            WiFi.disconnect();
            delay(100);
            WiFi.setAutoConnect(true);
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            WiFi.begin(ssid, pass);
        }
    } else {
        bool info = subCmd && strcmp(subCmd, "info") == 0;
        bool help = subCmd && strcmp(subCmd, "help") == 0;
        if (subCmd && !info && !help) printf("Wifi: unknown sub-command '%s'\n", subCmd);
        if (!subCmd || info) {
            printf("Wifi: SSID=%s PASS=%s connected:%s\n",
                    WiFi.SSID().c_str(), WiFi.psk().c_str(), WiFi.isConnected()?"yes":"no");
        }
        if (!subCmd || !info) {
            printf("Wifi: available sub-commands are connect, info, help\n");
        }
    }
}

void ESBCLI::cmdMqttCB(CommandParser *cp, const char *cmd) {
    const char *subCmd = cp->getArg();
    const char *arg = cp->getArg(); if (!arg) arg = "";
    //printf("MQTT command: '%s' '%s'\n", subCmd?subCmd:"", arg);
    if (subCmd && strcmp(subCmd, "server") == 0) {
        const char *port = cp->getArg();
        if (!port) port = "8883";
        printf("MQTT: setting server to %s:%s\n", arg, port);
        strncpy(config.mqtt_server, arg, sizeof(config.mqtt_server)-1);
        strncpy(config.mqtt_port, port, sizeof(config.mqtt_port)-1);
        config.save();
        mqttConnect();
    } else if (subCmd && strcmp(subCmd, "ident") == 0) {
        printf("MQTT: setting ident to %s\n", arg);
        strncpy(config.mqtt_ident, arg, sizeof(config.mqtt_ident)-1);
        config.save();
        mqttConnect();
    } else if (subCmd && strcmp(subCmd, "psk") == 0) {
        printf("MQTT: setting psk to %s\n", arg);
        strncpy(config.mqtt_psk, arg, sizeof(config.mqtt_psk)-1);
        config.save();
        mqttConnect();
    } else {
        bool info = subCmd && strcmp(subCmd, "info") == 0;
        bool help = subCmd && strcmp(subCmd, "help") == 0;
        if (subCmd && !info && !help) printf("MQTT: unknown sub-command '%s'", subCmd);
        if (!subCmd || info) {
            printf("MQTT: server=%s port=%s ident=%s psk=%s, connected:%s\n",
                    config.mqtt_server, config.mqtt_port, config.mqtt_ident, config.mqtt_psk,
                    mqttClient.connected()?"yes":"no");
        }
        if (!subCmd || !info) {
            printf("MQTT: available sub-commands are server, ident, psk, info, help\n");
        }
    }
}

void ESBCLI::cmdRestartCB(CommandParser *cp, const char *cmd) {
    printf("*** Restarting...\n");
    delay(50);
    ESP.restart();
    while (true) delay(100);
}

void ESBCLI::cmdErrorCB(CommandParser *cp, const char *cmd) {
    if (!cmd) cmd = "";
    if (strcmp("help", cmd) != 0) printf("Error: unknown command '%s'\n", cmd);
    printf("Available commands are: wifi, mqtt, restart, help\n");
}

void ESBCLI::init() {
    using namespace std::placeholders; // for the _1, _2, ...
    cmdParser = new CommandParser(&Serial);
    cmdParser->setDefault(std::bind(&ESBCLI::cmdErrorCB, this, _1, _2));
    cmdParser->addCommand("wifi", std::bind(&ESBCLI::cmdWifiCB, this, _1, _2));
    cmdParser->addCommand("mqtt", std::bind(&ESBCLI::cmdMqttCB, this, _1, _2));
    cmdParser->addCommand("restart", std::bind(&ESBCLI::cmdRestartCB, this, _1, _2));
}

// The following are not used, but if they're not included here then platformio doesn't
// find them when they're included in some other library. Sigh...
#if 0
#include <FS.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#endif

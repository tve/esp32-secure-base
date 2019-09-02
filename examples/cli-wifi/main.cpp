// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application to demo Wifi and MQTT configuration using a Wifi access point and/or a serial
// commandline.
// It also supports OTA flash updates.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPSecureBase.h>

//===== I/O pins/devices

#define BUTTON 0
#ifndef LED
#define LED    1
#endif

#undef cli
ESBConfig config;
ESBWifiConfig *wifiConfig; // allocated on-demand when portal is active
CommandParser cmdP(&Serial);
ESBCLI cli(config, cmdP);
ESBDebug dbg(cmdP);

// configuration portal timing

#define CONF_CONN_TIMEOUT   10     // seconds to try to connect to AP before setting up config portal
#define CONF_PORTAL_TIMEOUT (5*60) // seconds before config portal stops and device resets
#define CONF_RESET_TIMEOUT  (20*60) // seconds before we reset if MQTT remains disconnected

// MQTT message handling

static void onMqttMessage(char* topic, char* payload, MqttProps properties,
    size_t len, size_t index, size_t total)
{
    // Handle over-the-air update messages
    if (strlen(topic) == mqTopicLen+4 && len == total &&
            strncmp(topic, mqTopic, mqTopicLen) == 0 &&
            strcmp(topic+mqTopicLen, "/ota") == 0)
    {
        ESBOTA::begin(payload, len);
    }
}

//===== Setup

void setup() {

    Serial.begin(115200);
    printf("\n===== ESP32 Secure Base Example =====\n");
    pinMode(BUTTON, INPUT_PULLUP);

    config.read(); // read config file from flash
    cli.init(); // init CLI

#if 0
    // wipe-out saved wifi settings for testing purposes
    WiFi.begin("none", "no-no-no");
    delay(1000);
#endif

    mqttSetup(config);
    WiFi.mode(WIFI_STA); // start getting wifi to connect
    WiFi.begin();

    printf("Debug variables:\n");
    ESBVar::list();

    printf("===== Setup complete\n");
}

static uint32_t loopAt = 0;
static uint32_t lastInfo = -10000;
static bool wifiConn = false;
static bool mqttConn = false;
static uint32_t mqttLast = 0;
DV(loopAt); DV(lastInfo); DV(wifiConn); DV(mqttConn); DV(mqttLast);

// state to manage the WiFi configuration portal
static bool mqttDidConnect = false; // once MQTT connects we don't show the portal ever again
static uint32_t loopStart = 0;      // timer for the initial connection timeout
static uint32_t portalStart = 0;    // timer for the portal timeout
DV(mqttDidConnect); DV(loopStart); DV(portalStart);

void loop() {
    // update state
    loopAt = millis();
    if (loopStart == 0) loopStart = loopAt;
    bool wc = WiFi.isConnected();
    bool mc = mqttClient.connected();
    if (mc) mqttLast = loopAt;
    if (mc) mqttDidConnect = true;

    // print wifi/mqtt info every now and then, or when it changes.
    if (wc != wifiConn || mc != mqttConn || loopAt - lastInfo > 20000) {
        //WiFi.printDiag(Serial);
        printf("* Wifi:%s MQTT:%s\n",
                wc ? WiFi.SSID().c_str() : "---",
                mc ? config.mqtt_server : "---");
        lastInfo = loopAt;
        wifiConn = wc;
        mqttConn = mc;
    }

    // Do we need to start the configuration portal?.
    // The portal is started if, after the initial boot, we don't connect to MQTT within the
    // CONF_CONN_TIMEOUT.
    if (!mqttDidConnect && !wifiConfig && loopAt-loopStart > CONF_CONN_TIMEOUT) {
        wifiConfig = new ESBWifiConfig(config);
        wifiConfig->startPortal();
        portalStart = loopAt;
    }

    // Is it time to shut down the config portal?
    // The portal is shut down unconditionally after CONF_PORTAL_TIMEOUT.
    if (mqttConn && wifiConfig && loopAt-portalStart > CONF_PORTAL_TIMEOUT) {
        wifiConfig->stopPortal();
        delete wifiConfig;
        wifiConfig = 0;
    }

    // Do we need to reset 'cause things are just not working?
    if (!mqttConn && loopAt-mqttLast > CONF_RESET_TIMEOUT*1000) {
        printf("* MQTT disconnected for a long time, resetting...\n");
        delay(50); // time for the printf
        ESP.restart();
        delay(10000); // just in case it doesn't reset...
    }

    mqttLoop();
    cli.loop();
    if (wifiConfig) wifiConfig->loop();

    //if (digitalRead(BUTTON) == LOW) reconfig();

    delay(10);
}

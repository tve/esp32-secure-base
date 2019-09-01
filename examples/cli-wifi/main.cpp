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
ESBCLI cli(config);
ESBWifiConfig *wifiConfig; // allocated on-demand when portal is active

// configuration portal timing

#define CONF_CONN_TIMEOUT   10     // seconds to try to connect to AP before setting up config portal
#define CONF_PORTAL_TIMEOUT (5*60) // seconds before config portal stops and device resets
#define CONF_RESET_TIMEOUT  (20*60) // seconds before we reset if MQTT remains disconnected

// MQTT message handling

void onMqttMessage(char* topic, char* payload, MqttProps properties,
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

    printf("===== Setup complete\n");
}

uint32_t lastInfo = -10000;
bool wifiConn = false;
bool mqttConn = false;
uint32_t mqttLast = 0;

// state to manage the WiFi configuration portal
bool mqttDidConnect = false; // once MQTT connects we don't show the portal ever again
uint32_t loopStart = 0;      // timer for the initial connection timeout
uint32_t portalStart = 0;    // timer for the portal timeout

void loop() {
    // update state
    if (loopStart == 0) loopStart = millis();
    bool wc = WiFi.isConnected();
    bool mc = mqttClient.connected();
    if (mc) mqttLast = millis();
    if (mc) mqttDidConnect = true;

    // print wifi/mqtt info every now and then, or when it changes.
    if (wc != wifiConn || mc != mqttConn || millis() - lastInfo > 20000) {
        //WiFi.printDiag(Serial);
        printf("* Wifi:%s MQTT:%s\n",
                wc ? WiFi.SSID().c_str() : "---",
                mc ? config.mqtt_server : "---");
        lastInfo = millis();
        wifiConn = wc;
        mqttConn = mc;
    }

    // Do we need to start the configuration portal?.
    // The portal is started if, after the initial boot, we don't connect to MQTT within the
    // CONF_CONN_TIMEOUT.
    if (!mqttDidConnect && !wifiConfig && millis()-loopStart > CONF_CONN_TIMEOUT) {
        wifiConfig = new ESBWifiConfig(config);
        wifiConfig->startPortal();
        portalStart = millis();
    }

    // Is it time to shut down the config portal?
    // The portal is shut down unconditionally after CONF_PORTAL_TIMEOUT.
    if (mqttConn && wifiConfig && millis()-portalStart > CONF_PORTAL_TIMEOUT) {
        wifiConfig->stopPortal();
        delete wifiConfig;
        wifiConfig = 0;
    }

    // Do we need to reset 'cause things are just not working?
    if (!mqttConn && millis()-mqttLast > CONF_RESET_TIMEOUT*1000) {
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

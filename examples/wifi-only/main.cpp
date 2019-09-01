// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application to demo Wifi and MQTT configuration using a Wifi access point.
// It also supports OTA flash updates.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPSecureBase.h>

//===== I/O pins/devices

#define BUTTON 0
#ifndef LED
#define LED    1
#endif

ESBConfig config;

// configuration portal timing

#define CONF_CONN_TIMEOUT   10     // seconds to try to connect to AP before setting up config portal
#define CONF_PORTAL_TIMEOUT (5*60) // seconds before config portal stops and device resets

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
    ESBWifiConfig wifiConfig(config); // alloc Wifi config data, deallocated when setup exits

#if 0
    // wipe-out saved wifi settings for testing purposes
    WiFi.begin("none", "no-no-no");
    delay(1000);
#endif

    // Configure Wifi.
    bool connected = wifiConfig.connect(CONF_CONN_TIMEOUT, CONF_PORTAL_TIMEOUT);

    // If config failed, reset device to start over.
    if (!connected) {
        printf("*** Resetting...\n");
        delay(500); // time for the portal to send responses
        ESP.restart();
        while (true) delay(100);
    }

    printf("Wifi connected to %s\n", WiFi.SSID().c_str());

    mqttSetup(config);

    printf("\n===== Setup complete\n");
}

void reconfig() {
    ESBWifiConfig wifiConfig(config);
    bool saved = wifiConfig.reconfig(CONF_CONN_TIMEOUT, CONF_PORTAL_TIMEOUT);
    if (saved) {
        mqttSetup(config);
    }
}

uint32_t lastInfo = -10000;
bool wifiConn = false;

void loop() {
    // print wifi/mqtt info every now and then
    bool conn = WiFi.isConnected();
    if (conn != wifiConn || millis() - lastInfo > 20000) {
        //WiFi.printDiag(Serial);
        printf("* Wifi:%s MQTT:%s\n",
                conn ? WiFi.SSID().c_str() : "---",
                mqttClient.connected() ? config.mqtt_server : "---");
        lastInfo = millis();
        wifiConn = conn;
    }

    mqttLoop();

    if (digitalRead(BUTTON) == LOW) reconfig();

    delay(10);
}

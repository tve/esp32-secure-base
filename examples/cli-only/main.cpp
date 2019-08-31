// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application to demo Wifi and MQTT configuration using a commandline processor.
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

    config.read(); // read config file from flash
    cli.init(); // init CLI
    mqttSetup(config);
    WiFi.mode(WIFI_STA); // start getting wifi to connect
    WiFi.begin();

    printf("===== Setup complete\n");
}

uint32_t lastInfo = -1000000;
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
    cli.loop();

    delay(10);
}

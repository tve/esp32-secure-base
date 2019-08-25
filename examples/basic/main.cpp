// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application that prints Wifi info in loop().
// When starting up, it will block in setup() until WiFi has connected to an access point.
// If a configuration has been set-up, it tries to connect to the configured AP for a few seconds.
// If that fails, it starts an AP and waits for a configuration to be set.
// While in set-up mode, it resets every few minutes in case that fixes something.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPSecureBase.h>

//===== I/O pins/devices

#define BUTTON 0
#ifndef LED
#define LED    1
#endif

// configuration portal timing

#define CONF_CONN_TIMEOUT   10     // seconds to try to connect to AP before setting up config portal
#define CONF_PORTAL_TIMEOUT (5*60) // seconds before config portal stops and device resets

// MQTT message handling

void onMqttMessage(char* topic, char* payload, MqttProps properties,
    size_t len, size_t index, size_t total)
{
    if (strcmp(topic, "system/offset") == 0) {
        //onTimeZone(topic, payload, properties, len, index, total);
    } else if (strlen(topic) == mqTopicLen+4 && len == total &&
            strncmp(topic, mqTopic, mqTopicLen) == 0 &&
            strcmp(topic+mqTopicLen, "/ota") == 0)
    {
        ESBOTA::begin(payload, len);
    }
}

//===== Setup

void setup() {
    ESBConfig config;

    Serial.begin(115200);
    printf("\n===== ESP32 Secure Base Example =====\n");
    pinMode(BUTTON, INPUT_PULLUP);

#if 0
    // wipe-out saved wifi settings for testing purposes
    WiFi.begin("none", "no-no-no");
    delay(1000);
#endif

    // Configure Wifi.
    bool connected = config.setup(CONF_CONN_TIMEOUT, CONF_PORTAL_TIMEOUT);

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
    ESBConfig config;
    bool saved = config.reconfig(CONF_CONN_TIMEOUT, CONF_PORTAL_TIMEOUT);
    if (saved) {
        mqttSetup(config);
    }
}

void loop() {
    //WiFi.printDiag(Serial);
    printf("* Wifi:%s MQTT:%s\n", WiFi.isConnected()?"OK":"---", mqttClient.connected()?"OK":"---");
    mqttLoop();

    if (digitalRead(BUTTON) == LOW) reconfig();

    delay(2000);
}

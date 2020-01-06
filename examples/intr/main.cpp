// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application to demo Wifi and MQTT configuration using a commandline processor.
// It also supports OTA flash updates.
//
// This example tests interrupts, specifically, whether calling `millis()` in an interupt handler
// crashes OTA updates. The answer is 'yes' because it contains a 64-bit division from micros to
// millis and that is not IRAM_ATTR. Using micros() instead works like a charm.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPSecureBase.h>

//===== I/O pins/devices

#ifndef LED
#define LED    2
#endif
#define ON     1
#define INTR  25 // interrupt pin, put pulses at ~20Hz here

ESBConfig config;
CommandParser cmdP(&Serial);
ESBCLI cmd(config, cmdP);

static volatile uint32_t ledAt = 0;
#define INTR_MICROS 0

// Interrupt handler

void IRAM_ATTR intr() {
    digitalWrite(LED, ON);
#if INTR_MICROS
    ledAt = micros();
#else
    ledAt = millis();
#endif
}

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

void onMqttConnect(bool sessionPresent) {
    printf("Connected to MQTT, session %spresent\n", sessionPresent?"":"not ");
    char topic[64];

    strncpy(topic, mqTopic, 32);
    strcat(topic, "/ota");
    uint16_t packetIdSub = mqttClient.subscribe(topic, 1);
    printf("Subscribed at QoS 1, packetId: %d\n", packetIdSub);
}

//===== Setup

void setup() {
    Serial.begin(115200);
    printf("\n===== ESP32 Secure Base Interrupt Example =====\n");

    pinMode(LED, OUTPUT);
    digitalWrite(LED, ON);

    pinMode(INTR, INPUT_PULLUP);
    attachInterrupt(INTR, intr, RISING);

    config.read(); // read config file from flash
    cmd.init(); // init CLI
    mqttSetup(config);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onMessage(onMqttMessage);
    WiFi.mode(WIFI_STA); // start getting wifi to connect
    WiFi.begin();

    digitalWrite(LED, 1-ON);
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
    cmd.loop();

#if INTR_MICROS
    if (ledAt != 0 && micros() - ledAt > 30000) {
#else
    if (ledAt != 0 && millis() - ledAt > 30) {
#endif
	digitalWrite(LED, 1-ON);
	ledAt = 0;
    }

    //delay(10);
}

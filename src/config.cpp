// ESP32 Secure Base Basic Example
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// Example application that prints Wifi info in loop().
// When starting up, it will block in setup() until WiFi has connected to an access point.
// If a configuration has been set-up, it tries to connect to the configured AP for a few seconds.
// If that fails, it starts an AP and waits for a configuration to be set.
// While in set-up mode, it resets every few minutes in case that fixes something.

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include "ESPSecureBase.h"
#include <ArduinoJson.h>

// read reads the configuration from SPIFFS (flash filesystem).
void ESBConfig::read() {
    // mount SPIFFS, this does nothing if it's already mounted.
    if (!SPIFFS.begin(false)) {
        uint32_t t0 = millis();
        Serial.println("** Formatting fresh SPIFFS, takes ~20 seconds");
        if (!SPIFFS.begin(true)) {
            Serial.println("SPIFFS formatting failed, check your hardware, OOPS!");
        } else {
            printf("Format took %lus\n", (millis()-t0+500)/1000);
        }
    }

    File configFile = SPIFFS.open("/config.json", FILE_READ);
    if (configFile && configFile.size() > 10) {
        // load as json
        size_t size = configFile.size();
        printf("config file size is %d\n", size);
        DynamicJsonDocument json(2*size);
        DeserializationError err = deserializeJson(json, configFile);
        if (err) {
            Serial.print("failed to parse config.json: ");
            Serial.println(err.c_str());
            return;
        }

        strcpy(ap_pass, json["ap_pass"] |"");
        strcpy(mqtt_server, json["mqtt_server"] |"");
        strcpy(mqtt_port, json["mqtt_port"] |"");
        strcpy(mqtt_ident, json["mqtt_ident"] |"");
        strcpy(mqtt_psk, json["mqtt_psk"] |"");

#if 0
        strcpy(mqtt_server, "192.168.0.14");
        strcpy(mqtt_ident, "esp32-test");
        strcpy(mqtt_psk, "74e06d182a380734b07556d9f0387b5c");
#endif

        char psk[5]; strncpy(psk, mqtt_psk, 4); psk[4] = 0;
        printf("Config restored: MQTT<%s,%s;%s,%s...> AP<%s>\n",
                mqtt_server, mqtt_port, mqtt_ident, psk, ap_pass);

    } else {
        Serial.println("No config file, initializing mqtt ident/psk");

        // Construct default MQTT client id using chip MAC
        extern String getESP32ChipID();
        strcpy(mqtt_ident, "ESP-");
        strcat(mqtt_ident, getESP32ChipID().c_str());

        // Construct default random MQTT psk.
        char psk[16];
        esp_fill_random(psk, 16);
        for (int i=0; i<16; i++) sprintf(mqtt_psk+2*i, "%02x", psk[i]);
        printf("MQTT ident=%s psk=%s\n", mqtt_ident, mqtt_psk);
    }
    initialized = true;
}

// save checks whether something has changed and if so saves the config to SPIFFS.
void ESBConfig::save() {

    char psk[5]; strncpy(psk, mqtt_psk, 4); psk[4] = 0;
    printf("Saving config: MQTT<%s,%s;%s,%s...> AP<%s>\n",
            mqtt_server, mqtt_port, mqtt_ident, psk, ap_pass);
    DynamicJsonDocument json(1024);
    json["ap_pass"] = ap_pass;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_ident"] = mqtt_ident;
    json["mqtt_psk"] = mqtt_psk;

    File configFile = SPIFFS.open("/config.json", FILE_WRITE);
    if (!configFile) {
        printf("failed to write config.json\n");
    } else {
        serializeJson(json, configFile);
        configFile.close();
    }
}

void ESBWifiConfig::save() {
    if (strcmp(config.ap_pass, custom_ap_pass.getValue()) == 0 &&
            strcmp(config.mqtt_server, custom_mqtt_server.getValue()) == 0 &&
            strcmp(config.mqtt_port, custom_mqtt_port.getValue()) == 0 &&
            strcmp(config.mqtt_ident, custom_mqtt_ident.getValue()) == 0 &&
            strcmp(config.mqtt_psk, custom_mqtt_psk.getValue()) == 0) {
        return; // nothing has changed
    }
    saved = true;
    strcpy(config.ap_pass, custom_ap_pass.getValue());
    strcpy(config.mqtt_server, custom_mqtt_server.getValue());
    strcpy(config.mqtt_port, custom_mqtt_port.getValue());
    strcpy(config.mqtt_ident, custom_mqtt_ident.getValue());
    strcpy(config.mqtt_psk, custom_mqtt_psk.getValue());
    config.save();
}

void ESBWifiConfig::init(int connectTimeout, int portalTimeout) {
    // Configure Wifi manager
    custom_ap_pass.setValue(config.ap_pass);
    custom_mqtt_server.setValue(config.mqtt_server);
    custom_mqtt_port.setValue(config.mqtt_port);
    custom_mqtt_ident.setValue(config.mqtt_ident);
    custom_mqtt_psk.setValue(config.mqtt_psk);
    wifiMan.setConnectTimeout(connectTimeout);
    wifiMan.setConfigPortalTimeout(portalTimeout);
    wifiMan.setTryConnectDuringConfigPortal(false); // stop scanning...
    if (!initialized) {
        wifiMan.addParameter(&custom_header);
        wifiMan.addParameter(&custom_ap_pass);
        wifiMan.addParameter(&custom_header2);
        wifiMan.addParameter(&custom_mqtt_server);
        wifiMan.addParameter(&custom_mqtt_port);
        wifiMan.addParameter(&custom_mqtt_ident);
        wifiMan.addParameter(&custom_mqtt_psk);
        initialized = true;
    }
}

bool ESBWifiConfig::connect(int connectTimeout, int portalTimeout) {
    init(connectTimeout, portalTimeout);

    // Run the wifi manager.
    extern String getESP32ChipID();
    String ap_name = "ESP-" + getESP32ChipID();
    bool connected = wifiMan.autoConnect(ap_name.c_str(), config.ap_pass);
    save();
    return connected;
}

bool ESBWifiConfig::reconfig(int connectTimeout, int portalTimeout) {
    init(connectTimeout, portalTimeout);

    saved = false;
    //auto cb = [this]() {  saved = true; };
    //wifiMan.setSaveConfigCallback(cb);

    extern String getESP32ChipID();
    String ap_name = "ESP-" + getESP32ChipID();
    wifiMan.startConfigPortalModeless(ap_name.c_str(), config.ap_pass);
    //config_save();
    //return saved;
    return false;
}

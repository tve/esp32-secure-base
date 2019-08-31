// ESP32 Secure Base - MQTT helpers
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

#include <ESPSecureBase.h>

// general pub-sub stuff, useful in other parts of the code as well
AsyncMqttClient mqttClient;
static ESBConfig *config;
char mqTopic[65];   // main topic prefix for pub&sub, init'd as mqIdent with sub - with /
int mqTopicLen = 0; // strlen(mqTopic)

// keep-alive stuff
#define MQ_TIMEOUT (60*1000)    // in milliseconds
static uint32_t mqLast = 0x100000; // when we last received something
static uint32_t mqPing = 0;     // when we last sent a ping

// helper to subscribe to our own pings
static void mqttSubPing() {
    char topic[75];
    strcpy(topic, mqTopic);
    strcat(topic, "/ping");
    (void) mqttClient.subscribe(topic, 0);
}

static void onMqttConnect(bool sessionPresent) {
    printf("Connected to MQTT, session %spresent\n", sessionPresent?"":"not ");
    mqLast = millis();
    mqttSubPing();
}

static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    printf("Disconnected from MQTT: %d\n", (int)reason);
}

// onMqttMessage handles the ping response messages
static void onMqttMessage(char* topic, char* payload, MqttProps properties,
    size_t len, size_t index, size_t total)
{
    printf("Publish received, topic=%s qos=%d dup=%d retain=%d len=%d index=%d total=%d\n",
            topic, properties.qos, properties.dup, properties.retain, len, index, total);

    if (strlen(topic) == mqTopicLen+5 &&
            strncmp(topic, mqTopic, mqTopicLen) == 0 &&
            strcmp(topic+mqTopicLen, "/ping") == 0)
    {
        mqLast = millis();
    }
}

void mqttSetTopic(char *topic) {
    if (mqTopicLen != 0) {
        char topic[75];
        strcpy(topic, mqTopic);
        strcat(topic, "/ping");
        (void) mqttClient.unsubscribe(topic);
    }
    strncpy(mqTopic, topic, sizeof(mqTopic));
    mqTopic[sizeof(mqTopic)-1] = 0;
    mqTopicLen = strlen(mqTopic);
    if (mqttClient.connected()) mqttSubPing();
}

void mqttSetup(ESBConfig &c) {
    config = &c;
    // set-up callbacks
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
}

void mqttConnect() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
        delay(100); // give LwIP some time to do something?
    }
    // config server and security
    uint16_t port = (uint16_t)atoi(config->mqtt_port);
    mqttClient.setServer(config->mqtt_server, port);
    mqttClient.setSecure(true);

    char psk[5]; strncpy(psk, config->mqtt_psk, 4); psk[4] = 0;
    printf("MQTT connecting to %s:%d (%s,%s...)\n",
            config->mqtt_server, port, config->mqtt_ident, psk);
    mqttClient.setPsk(config->mqtt_ident, config->mqtt_psk);
    // config base topic
    if (mqTopicLen == 0) {
        char topic[41];
        strcpy(topic, config->mqtt_ident);
        // replace '-' by '/'
        for (char *dash=strchr(topic, '-'); dash; dash=strchr(dash, '-')) *dash = '/';
        mqttSetTopic(topic);
    }

    mqttClient.connect();
}

void mqttLoop() {
    if (!WiFi.isConnected()) return;
    if (!mqttClient.connected()) {
        if (millis() - mqLast > 10000) {
            mqttConnect();
            mqLast = millis();
        }
    } else if (millis() - mqLast > MQ_TIMEOUT) {
        //printf("Reconnecting to MQTT\n");
        mqttConnect();
        mqLast = millis();
    } else if (millis() - mqLast > MQ_TIMEOUT/2 && millis() - mqPing > MQ_TIMEOUT/2) {
        char topic[41+6];
        strcpy(topic, mqTopic);
        strcat(topic, "/ping");
        mqttClient.publish(topic, 0, false);
        printf("MQTT: ping sent to %s\n", topic);
        mqPing = millis();
    }
    return;
}


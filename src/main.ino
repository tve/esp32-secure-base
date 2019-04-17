// ESP32 MQTT Base Project v1
// Copyright (c) 2019 Thorsten von Eicken, all rights reserved

// To run create a wifi.h file with:
//#define WIFI_SSID "your ssid"
//#define WIFI_PASSWD "your password"
//#define MQTT_IDENT "mqtt PSK identity"
//#define MQTT_PSK "mqtt hex PSK string"

#define ASYNC

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "lwip/apps/sntp.h"
#include "wifi.h" // #defines WIFI_SSID, WIFI_PASSWD, MQTT_*, TZ_*

typedef struct AsyncMqttClientMessageProperties MqttProps;

#define LED 2
#define TRIG 13

AsyncMqttClient mqttClient;

TimerHandle_t wifiReconnectTimer; // timer to trigger reconnect when system idle
TimerHandle_t mqttReconnectTimer; // timer to trigger reconnect when system idle

//===== WiFi connection/reconnection

void connectToWifi() {
  printf("Connecting to Wi-Fi %s...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
}

void connectToMqtt(); // forward decl
void setupSNTP(); // forward decl

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.print("WiFi connected, IP=");
    Serial.println(WiFi.localIP());
    connectToMqtt();
    setupSNTP();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    printf("WiFi lost connection\n");
    // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStop(mqttReconnectTimer, 0);
    xTimerStart(wifiReconnectTimer, 0);
    break;
  default:
    printf("WiFi event %d\n", event);
    break;
  }
}

//===== Mqtt processing

uint32_t mqttConnCnt = 0;
uint32_t mqttPubCnt = 0;
uint32_t mqttRcvCnt = 0;

// connectToMqtt starts a fresh connection
void connectToMqtt() {
  printf("Connecting to MQTT...\n");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  mqttConnCnt++;
  printf("Connected to MQTT, session %spresent\n", sessionPresent?"":"not ");

  uint16_t packetIdSub = mqttClient.subscribe("esp32/ping", 1);
  printf("Subscribed at QoS 1, packetId: %d\n", packetIdSub);

  mqttClient.subscribe("system/offset", 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  printf("Disconnected from MQTT: %d\n", (int)reason);

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  printf("Subscribe acknowledged id=%d qos=%d\n", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  printf("Unsubscribe acknowledged id=%d\n", packetId);
}

void onTimeZone(char*, char*, MqttProps, size_t, size_t, size_t);

void onMqttMessage(char* topic, char* payload, MqttProps properties,
    size_t len, size_t index, size_t total)
{
  mqttRcvCnt++;

  printf("Publish received, topic=%s qos=%d dup=%d retain=%d len=%d index=%d total=%d\n",
      topic, properties.qos, properties.dup, properties.retain, len, index, total);

  if (strcmp(topic, "system/offset") == 0) {
    onTimeZone(topic, payload, properties, len, index, total);
  }
}

void onMqttPublish(uint16_t packetId) {
  printf("Publish acknowledged id=%d\n", packetId);
}

void pubStatus() {
  char msg[128] = {0};
  snprintf(msg, 128, "{\"rssi\":%d, \"conn\":%d, \"pub\":%d, \"rcv\":%d}", WiFi.RSSI(),
      mqttConnCnt, mqttPubCnt, mqttRcvCnt);
  char topic[32] = {0};
  snprintf(topic, 32, "esp32/%s/status", mqttClient.getClientId());
  mqttPubCnt++;
  mqttClient.publish(topic, 0, false, msg);
}

//===== SNTP and time stuff

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// setupSNTP configures the SNTP service. This is a clone of esp32-hal-time.c modified to guess the
// SNTP server names.
void setupSNTP() {
  if (sntp_enabled()) sntp_stop();

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  struct ip_addr ip = IPADDR4_INIT(WiFi.gatewayIP());
  sntp_setserver(0, &ip);
  ip = IPADDR4_INIT(MQTT_HOST);
  sntp_setserver(1, &ip);
  sntp_setservername(2, "pool.ntp.org");
  sntp_init();

  char tz[33] = {0};
  sprintf(tz, "UTC%dDST%d", -TZ_OFF, -TZ_OFF-TZ_DST);
  setenv("TZ", tz, 1);
  tzset();
}

void onTimeZone(char* topic, char* payload, MqttProps props, size_t len, size_t index, size_t total) {
  static int32_t lastOff = -100;
  if (!(index == 0 && len == total && len > 0 && len < 32)) return;
  char buf[32];
  memcpy(buf, payload, len);
  buf[len] = 0;
  int32_t off = atoi(payload);
  if (off != lastOff) {
    char tz[33] = {0};
    sprintf(tz, "UTC%dDST%d", off/60, off/60);
    printf("Setting TZ from MQTT: %s (%s)\n", tz, buf);
    setenv("TZ", tz, 1);
    tzset();
    lastOff = off;
  }
}

//===== 

void setup() {
  digitalWrite(LED, HIGH);
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  printf("\n===== ESP32 Greenhouse Watering V1 =====\n");

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE,
      (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE,
      (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setSecure(true);
  mqttClient.setPsk(MQTT_IDENT, MQTT_PSK);

  connectToWifi();

  printf("\n===== Setup complete\n");
  digitalWrite(LED, LOW);
}

void loop() {
  digitalWrite(LED, HIGH);
  Serial.println();
  printLocalTime();
  uint16_t packetIdPub1 = mqttClient.publish("esp32/ping", 1, true, "test 1");
  mqttPubCnt++;
  printf("Published at QoS 1, packetId: %d\n", packetIdPub1);
  pubStatus();

  digitalWrite(LED, LOW);
  delay(4500);
}

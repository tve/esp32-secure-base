// ESP32 Secure Base - MQTT helpers
// by Thorsten von Eicken, 2019

#include <AsyncMqttClient.h>
typedef struct AsyncMqttClientMessageProperties MqttProps;

extern AsyncMqttClient mqttClient;
extern char mqTopic[]; // main topic prefix for pub&sub, init'd as mqIdent with sub - with /
extern int mqTopicLen; // strlen(mqTopic)

struct ESBConfig;
extern void mqttSetup(ESBConfig &config);
extern void mqttLoop();
extern void mqttSetTopic(char *);

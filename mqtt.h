#ifndef MQTT_H
#define MQTT_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// MQTT topics
#define MQTT_BASE_TOPIC       "technics_amplifier"
#define MQTT_CMD_TOPIC        MQTT_BASE_TOPIC "/command"
#define MQTT_STATE_TOPIC      MQTT_BASE_TOPIC "/state"
#define MQTT_POWER_TOPIC      MQTT_BASE_TOPIC "/power"
#define MQTT_BT_AUDIO_TOPIC   MQTT_BASE_TOPIC "/bt_audio"
#define MQTT_AVAILABLE_TOPIC  MQTT_BASE_TOPIC "/available"
#define MQTT_EVENT_TOPIC      MQTT_BASE_TOPIC "/event"

extern PubSubClient mqttClient;

void initiateMqtt();
void handleMqtt();
void mqttPublish(const char* topic, const char* payload, bool retained = false);
void mqttPublishState(const char* key, const char* value);
void mqttPublishEvent(const char* event);

#endif // MQTT_H

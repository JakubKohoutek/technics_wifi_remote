#include <PubSubClient.h>
#include <credentials.h>
#include "mqtt.h"
#include "log.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastMqttReconnectAttempt = 0;

// Forward declaration - implemented in main sketch
extern void sendCode(const char* command);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    logMessage(String("[MQTT] Received on ") + topic + ": " + message);

    if (String(topic) == MQTT_CMD_TOPIC) {
        sendCode(message.c_str());
        mqttPublishEvent(("command_sent:" + message).c_str());
    }
}

bool mqttReconnect() {
    String clientId = "technics_amp_" + String(ESP.getChipId(), HEX);

    bool connected;
    #ifdef MQTT_USER
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                                        MQTT_AVAILABLE_TOPIC, 0, true, "offline");
    #else
        connected = mqttClient.connect(clientId.c_str(),
                                        MQTT_AVAILABLE_TOPIC, 0, true, "offline");
    #endif

    if (connected) {
        logMessage("[MQTT] Connected to broker");
        mqttClient.subscribe(MQTT_CMD_TOPIC);
        mqttPublish(MQTT_AVAILABLE_TOPIC, "online", true);
        logMessage("[MQTT] Subscribed to " MQTT_CMD_TOPIC);
    } else {
        logMessage(String("[MQTT] Connection failed, rc=") + mqttClient.state());
    }

    return connected;
}

void initiateMqtt() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    logMessage(String("[MQTT] Initiating MQTT connection to ") + MQTT_SERVER);
    mqttReconnect();
}

void handleMqtt() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            mqttReconnect();
        }
    }
    mqttClient.loop();
}

void mqttPublish(const char* topic, const char* payload, bool retained) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, payload, retained);
    }
}

void mqttPublishState(const char* key, const char* value) {
    String topic = String(MQTT_BASE_TOPIC) + "/" + key;
    mqttPublish(topic.c_str(), value, true);
}

void mqttPublishEvent(const char* event) {
    mqttPublish(MQTT_EVENT_TOPIC, event, false);
}

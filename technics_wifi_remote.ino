#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <LittleFS.h>
#include <IRremote.h> // library by shirriff, https://github.com/z3t0/Arduino-IRremote
#include <credentials.h>
#include <map>
#include "ota.h"
#include "memory.h"
#include "log.h"
#include "mqtt.h"
#include "web.h"

#define STANDBY_PIN   D1
#define OUT_PIN       D2
#define POWER_PIN     D5  // Amplifier power state input (HIGH = on)
#define BT_AUDIO_PIN  D6  // Bluetooth audio signal (HIGH = playing)

// Prague timezone with DST: CET-1CEST,M3.5.0/02,M10.5.0/03
#define MY_TZ         "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define MY_NTP_SERVER "pool.ntp.org"

// Debounce and timing
#define BT_AUDIO_DEBOUNCE_MS  2000   // Wait 2s before confirming BT audio state change
#define STATE_POLL_INTERVAL   500    // Poll input pins every 500ms
#define MQTT_STATE_INTERVAL   30000  // Re-publish state every 30s
#define WIFI_CHECK_INTERVAL   10000  // Check WiFi connection every 10s
#define RSSI_LOG_INTERVAL     600000 // Log RSSI every 10 minutes

AsyncWebServer server(80);
const char*    ssid      = STASSID;
const char*    password  = STAPSK;
const char*    deviceId  = "amplifier";

// State tracking
bool ampPowerState      = false;
bool btAudioPlaying     = false;
bool lastBtAudioReading = false;
unsigned long btAudioChangeTime  = 0;
unsigned long lastStatePoll      = 0;
unsigned long lastMqttStatePublish = 0;
unsigned long lastWiFiCheck      = 0;
unsigned long lastRssiLog        = 0;

std::map<std::string, uint32_t> codes = {
  {"power_toggle", 0x1FB409},
  {"volume_up", 0x1BB489},
  {"volume_down", 0x1AB4A9},
  {"mute", 0x18B4E9},
  {"vcr1_select", 0x1B7C9},
  {"deck_select", 0x3BB089},
  {"deck_record", 0x37B109},
  {"deck_play_fwd", 0x35B149},
  {"deck_play_bwd", 0x36B129},
  {"deck_stop", 0x3FB009},
  {"deck_pause", 0x39B0C9},
  {"cd_select", 0x199CCC},
  {"cd_play", 0x35994C},
  {"cd_stop", 0x3F980C},
  {"cd_fwd", 0x3C986C},
  {"cd_bwd", 0x3D984C},
};

void turnOnWiFi() {
    Serial.println("[MAIN] Starting the WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED){
        delay(500);
    }
    Serial.println("[MAIN] WiFi started");
    Serial.print("[MAIN] IP Address: ");
    Serial.println(WiFi.localIP());
    configTime(MY_TZ, MY_NTP_SERVER);
}

void sendCode(const char* command) {
    auto it = codes.find(std::string(command));
    if (it == codes.end()) {
        logMessage(String("[ERROR] Command '") + command + "' not found in codes map");
        return;
    }

    uint32_t code = it->second;

    logMessage(String("[INFO] Sending IR code 0x") + String(code, HEX) + " for command '" + command + "'");
    IrSender.sendPulseDistanceWidth(38, 3750, 3550, 1000, 2650, 1000, 850, code, 22, PROTOCOL_IS_LSB_FIRST, 100, 1);
}

void publishAllStates() {
    mqttPublishState("power", ampPowerState ? "ON" : "OFF");
    mqttPublishState("bt_audio", btAudioPlaying ? "ON" : "OFF");
    mqttPublish(MQTT_AVAILABLE_TOPIC, "online", true);
}

void handleWiFi() {
    unsigned long now = millis();

    if (now - lastWiFiCheck < WIFI_CHECK_INTERVAL) return;
    lastWiFiCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
        logMessage("[WIFI] Connection lost, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED) {
            logMessage("[WIFI] Reconnected. IP: " + WiFi.localIP().toString());
        } else {
            Serial.println("[WIFI] Reconnection failed, will retry");
        }
        return;
    }

    // Log RSSI periodically
    if (now - lastRssiLog >= RSSI_LOG_INTERVAL) {
        lastRssiLog = now;
        logMessage(String("[WIFI] RSSI: ") + WiFi.RSSI() + " dBm");
    }
}

void pollInputStates() {
    unsigned long now = millis();
    if (now - lastStatePoll < STATE_POLL_INTERVAL) return;
    lastStatePoll = now;

    // Read amplifier power state
    bool currentPower = digitalRead(POWER_PIN) == HIGH;
    if (currentPower != ampPowerState) {
        ampPowerState = currentPower;
        logMessage(String("[STATE] Amplifier power: ") + (ampPowerState ? "ON" : "OFF"));
        mqttPublishState("power", ampPowerState ? "ON" : "OFF");
        mqttPublishEvent(ampPowerState ? "power_on" : "power_off");
    }

    // Read BT audio state with debounce
    bool currentBtAudio = digitalRead(BT_AUDIO_PIN) == HIGH;
    if (currentBtAudio != lastBtAudioReading) {
        lastBtAudioReading = currentBtAudio;
        btAudioChangeTime = now;
    }

    if (lastBtAudioReading != btAudioPlaying && (now - btAudioChangeTime >= BT_AUDIO_DEBOUNCE_MS)) {
        btAudioPlaying = lastBtAudioReading;
        logMessage(String("[STATE] BT audio: ") + (btAudioPlaying ? "Playing" : "Silent"));
        mqttPublishState("bt_audio", btAudioPlaying ? "ON" : "OFF");
        mqttPublishEvent(btAudioPlaying ? "bt_audio_started" : "bt_audio_stopped");

        // Auto power-on: if BT audio starts and amp is off, turn it on
        if (btAudioPlaying && !ampPowerState) {
            logMessage("[AUTO] BT audio detected, turning amplifier ON");
            sendCode("power_toggle");
            mqttPublishEvent("auto_power_on");
        }
    }

    // Periodic MQTT state re-publish
    if (now - lastMqttStatePublish >= MQTT_STATE_INTERVAL) {
        lastMqttStatePublish = now;
        publishAllStates();
    }
}

void setup() {
    pinMode(OUT_PIN,      OUTPUT);
    pinMode(LED_BUILTIN,  OUTPUT);
    pinMode(STANDBY_PIN,  INPUT);
    pinMode(POWER_PIN,    INPUT);
    pinMode(BT_AUDIO_PIN, INPUT);

    // Start the hardware serial communication
    Serial.begin(115200);
    Serial.println();

    // Initiate IR
    IrSender.begin(OUT_PIN);

    // Ensure network is available
    turnOnWiFi();

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    WebSerial.msgCallback(handleWebSerialMessage);

    // Initiate log
    initiateLog();

    // Inititate eeprom memory
    initiateMemory();

    // Initiate over the air programming
    OTA::initialize(deviceId);

    // Initiate MQTT
    initiateMqtt();

    // Web interface
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // API: send IR command
    server.on("/api/send", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("cmd")) {
            String cmd = request->getParam("cmd")->value();
            sendCode(cmd.c_str());
            request->send(200, "text/plain", "Sent: " + cmd);
        } else {
            request->send(400, "text/plain", "Missing cmd parameter");
        }
    });

    // API: get status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"power\":" + String(ampPowerState ? "true" : "false") +
                      ",\"bt_audio\":" + String(btAudioPlaying ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });

    // API: list commands
    server.on("/api/commands", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        bool first = true;
        for (auto& pair : codes) {
            if (!first) json += ",";
            json += "\"" + String(pair.first.c_str()) + "\"";
            first = false;
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // Log file endpoint
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, logFilePath, "text/plain");
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found. Try / for web UI or /webserial for console.");
    });

    server.begin();

    // Read initial states
    ampPowerState = digitalRead(POWER_PIN) == HIGH;
    btAudioPlaying = digitalRead(BT_AUDIO_PIN) == HIGH;
    lastBtAudioReading = btAudioPlaying;
    publishAllStates();

    logMessage("[MAIN] Setup complete. IP: " + WiFi.localIP().toString());
}

void handleWebSerialMessage(uint8_t *data, size_t len){
    // Process message into command:value pair
    String command = "";
    String value   = "";
    boolean beforeColon = true;
    for(int i=0; i < len; i++){
        if (char(data[i]) == ':'){
            beforeColon = false;
        } else if (beforeColon) {
            command += char(data[i]);
        } else {
            value += char(data[i]);
        }
    }

    if(command.equals("send")) {
        sendCode(value.c_str());
    } else
    if(command.equals("status")) {
        logMessage(String("[STATUS] Power: ") + (ampPowerState ? "ON" : "OFF") +
                   ", BT Audio: " + (btAudioPlaying ? "Playing" : "Silent"));
    } else
    if(command.equals("help")) {
        logMessage((
          String("Available commands:\n") +
          "send:code - Send IR command\n" +
          "status - Show current states\n" +
          "list - List available IR codes\n"
        ).c_str());
    } else
    if(command.equals("list")) {
        String list = "Available IR codes:\n";
        for (auto& pair : codes) {
            list += String(pair.first.c_str()) + "\n";
        }
        logMessage(list);
    } else {
        logMessage(String("Unknown command '") + command + "' with value '" + value + "'");
    }
}

void loop() {
    OTA::handle();
    handleWiFi();
    handleMqtt();
    pollInputStates();
    yield();
}

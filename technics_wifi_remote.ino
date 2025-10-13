#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <fauxmoESP.h>
#include <WebSerial.h>
#include <LittleFS.h>
#include <IRremote.h> // library by shirriff, https://github.com/z3t0/Arduino-IRremote
#include <credentials.h>
#include <map>
#include "ota.h"
#include "memory.h"
#include "log.h"

#define STANDBY_PIN D1
#define OUT_PIN     D2
// Prague timezone with DST: CET-1CEST,M3.5.0/02,M10.5.0/03
#define MY_TZ       "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define MY_NTP_SERVER "pool.ntp.org"

fauxmoESP      fauxmo;
AsyncWebServer server(80);
const char*    ssid                     = STASSID;
const char*    password                 = STAPSK;
const char*    deviceId                 = "amplifier";

std::map<std::string, uint32_t> codes = { 
  {"power_toggle", 0x1FB409},
  {"volume_up", 0x1BB489},
  {"volume_down", 0x1AB4A9},
  {"volume_up", 0x1BB489},
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

void setup() {
    pinMode(OUT_PIN,     OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(STANDBY_PIN, INPUT);

    // Start the hardware serial communication
    Serial.begin(115200);
    Serial.println();

    // Initiate IR
    IrSender.begin(OUT_PIN);

    // Ensure network is available
    turnOnWiFi();

    // Start the server
    fauxmo.createServer(false);
    fauxmo.setPort(80);
    fauxmo.addDevice(deviceId);
    fauxmo.enable(true);

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    WebSerial.msgCallback(handleWebSerialMessage);
    announceWebSerialReady();

    // Initiate log
    initiateLog();

    // Inititate eeprom memory
    initiateMemory();


    // Initiate over the air programming
    OTA::initialize(deviceId);

    // Configure connection to home assistants
    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool switchedOn, unsigned char value) {
        String statusMessage = String("[MAIN] Device ") + "device_id" + "(" + device_name + ") state: " + (switchedOn ? "ON" : "OFF") + ", value: " + value;
        logMessage(statusMessage);

        // TODO - toggle the amplifier?
    });

    // Set up the html server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, logFilePath, "text/plain");
    });

    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, logFilePath, "text/plain");
    });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
        // Handle any other body request here...
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
        request->send(404, "text/plain", "Sorry, not found. Did you mean to go to /webserial ?");
    });

    server.begin();
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
    if(command.equals("help")) {
        logMessage((
          String("Available commands:\n") +
          "send:code\n" +
          "list\n"
        ).c_str());
    } else
    if(command.equals("list")) {
        logMessage((
          String("Available commands:\n") +
            "power_toggle\n" +
            "volume_up\n" +
            "volume_down\n" +
            "volume_up\n" +
            "mute\n" +
            "vcr1_select\n" +
            "deck_select\n" +
            "deck_record\n" +
            "deck_play_fwd\n" + 
            "deck_play_bwd\n" +
            "deck_stop\n" +
            "deck_pause\n" +
            "cd_select\n" +
            "cd_play\n" +
            "cd_stop\n" +
            "cd_fwd\n" +
            "cd_bwd\n"
        ).c_str());
    } else {
        logMessage(String("Unknown command '") + command + "' with value '" + value +"'" + value);
    }
}

void loop() {
    fauxmo.handle();
    OTA::handle();
    yield();
}

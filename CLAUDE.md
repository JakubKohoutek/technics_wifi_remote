# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP-8266 (NodeMCU) Arduino project that controls a Technics AV receiver via IR signals over WiFi. Features MQTT for Home Assistant integration, a web control panel, power state monitoring, Bluetooth audio detection with auto-power-on, and OTA firmware updates.

**IMPORTANT: Do NOT use fauxmoESP (deprecated). Smart home integration is handled via MQTT and REST API endpoints. If fauxmoESP is found in any project, remove it and replace with MQTT/REST.**

## Build & Upload

This is an Arduino IDE project targeting ESP8266. The sketch is `technics_wifi_remote.ino`.

- **Board**: ESP8266 (NodeMCU v2) — FQBN: `esp8266:esp8266:nodemcuv2`
- **Compile**: `arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --libraries ~/Documents/Arduino/libraries .`
- **Upload via USB**: Use Arduino IDE or `arduino-cli upload`
- **Upload via OTA**: ArduinoOTA is enabled; the device advertises as "amplifier" on the network
- **Filesystem**: LittleFS — upload filesystem image if log file serving is needed

### Required Libraries

- `ESP8266WiFi`, `ESPAsyncWebServer`, `ESPAsyncTCP`, `WebSerial`, `LittleFS`, `IRremote` (shirriff/z3t0), `ArduinoOTA`, `ESP8266mDNS`, `EEPROM`
- `PubSubClient` — MQTT client for Home Assistant integration
- WiFi and MQTT credentials come from `<credentials.h>` (must define `STASSID`, `STAPSK`, `MQTT_SERVER`, `MQTT_PORT`, and optionally `MQTT_USER`/`MQTT_PASS`) — this file is not in the repo

## Architecture

### Main sketch (`technics_wifi_remote.ino`)
- Defines IR command codes map (`codes`) mapping string names to 22-bit Technics IR codes
- `sendCode()` looks up a command name and transmits via `IrSender.sendPulseDistanceWidth()` with Technics-specific timing (38kHz, LSB-first, 22-bit)
- Sets up: WiFi → IR sender → WebSerial → log → EEPROM → OTA → MQTT
- `handleWiFi()` monitors WiFi connection and reconnects if lost; logs RSSI periodically
- Monitors amplifier power state (D5) and BT audio signal (D6) with debouncing
- Auto power-on: sends `power_toggle` when BT audio starts and amp is off (2s debounce)
- Serves web UI at `/`, REST API at `/api/*`, log at `/log`, WebSerial at `/webserial`
- WebSerial commands use `command:value` format (e.g., `send:power_toggle`, `status`, `list`, `help`)

### Modules
- **`mqtt.h/cpp`**: MQTT client (PubSubClient) — connects to broker, subscribes to command topic, publishes power/BT audio state and events, auto-reconnects every 5s
- **`web.h`**: HTML/CSS/JS web interface stored in PROGMEM — responsive control panel with all IR commands and live status via AJAX polling
- **`log.h/cpp`**: Timestamped logging to LittleFS (`/log.txt`, 100KB max with char-buffer truncation via temp file), WebSerial (when WiFi connected), and Serial simultaneously
- **`memory.h/cpp`**: EEPROM read/write helpers for `unsigned long` values (4-byte union-based)
- **`ota.h/cpp`**: ArduinoOTA wrapper class with progress/error reporting

### Hardware Pins
- `D1` (STANDBY_PIN): Input — receiver standby detection
- `D2` (OUT_PIN): Output — IR LED
- `D5` (POWER_PIN): Input — amplifier power state (HIGH = on)
- `D6` (BT_AUDIO_PIN): Input — Bluetooth audio signal (HIGH = playing)

### MQTT Topics
- `technics_amplifier/command` — Subscribe — receive IR command names to execute
- `technics_amplifier/power` — Publish (retained) — `ON` / `OFF`
- `technics_amplifier/bt_audio` — Publish (retained) — `ON` / `OFF`
- `technics_amplifier/event` — Publish — events like `power_on`, `auto_power_on`, `bt_audio_started`
- `technics_amplifier/available` — Publish (retained) — `online` / `offline` (LWT)

### REST API
- `GET /api/send?cmd=<command>` — Send IR command
- `GET /api/status` — JSON: `{"power": bool, "bt_audio": bool}`
- `GET /api/commands` — JSON array of available command names

## Key Details

- IR protocol uses `sendPulseDistanceWidth` with specific Technics timing parameters — not a standard NEC/Sony protocol
- NTP is configured for Prague timezone (CET/CEST)
- BT audio signal is debounced with a 2-second delay to prevent false triggers
- MQTT state is re-published every 30 seconds as a heartbeat
- MQTT reconnects automatically every 5 seconds if connection is lost
- WiFi reconnection: checked every 10s, reconnects with 10s timeout if connection lost; RSSI logged every 10 minutes

## Post-Change Checklist

**IMPORTANT: After making any code changes, always update the documentation:**
1. Update this `CLAUDE.md` file to reflect architectural changes, new modules, pin changes, new topics/endpoints
2. Update `README.md` to reflect user-facing changes (new features, setup steps, wiring, MQTT topics, API endpoints, commands)
3. Keep both files in sync with the actual code

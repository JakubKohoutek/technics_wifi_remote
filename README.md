# Technics receiver wifi remote

ESP-8266 (NodeMCU) project that controls a Technics AV receiver via IR signals over WiFi. Features MQTT integration for Home Assistant, a web control panel, automatic power-on when Bluetooth audio is detected, and OTA firmware updates.

## Features

- **IR Remote Control** — Send Technics IR commands (power, volume, input select, CD/deck transport)
- **Web Interface** — Responsive control panel at `http://<Device IP>/` with all commands and live status
- **MQTT Integration** — Publish state and receive commands via MQTT for Home Assistant
- **Power State Monitoring** — Reads amplifier power state from a dedicated pin
- **BT Audio Detection** — Monitors external Bluetooth receiver; auto-powers-on the amplifier when music starts
- **WebSerial Console** — Debug console at `http://<Device IP>/webserial`
- **OTA Updates** — Over-the-air firmware updates (mDNS name: "amplifier")
- **Logging** — Timestamped log to LittleFS (100KB max, auto-truncation), Serial, and WebSerial; viewable at `/log`
- **WiFi Resilience** — Automatic WiFi reconnection every 10s if connection is lost; periodic RSSI logging

## Hardware

### Pin Assignment

| Pin | Name | Direction | Function |
|-----|------|-----------|----------|
| D1 | STANDBY_PIN | INPUT | Receiver standby detection |
| D2 | OUT_PIN | OUTPUT | IR LED |
| D5 | POWER_PIN | INPUT | Amplifier power state (HIGH = on) |
| D6 | BT_AUDIO_PIN | INPUT | Bluetooth audio signal (HIGH = playing) |

### Wiring Notes

- **IR LED** on D2 — connect via appropriate current-limiting resistor and transistor driver
- **Power state** on D5 — connect to amplifier power indicator signal (must be 3.3V logic level)
- **BT audio** on D6 — connect to Bluetooth receiver audio-present signal (HIGH when music is playing, LOW when silent)

## Circuit Diagram

TODO

## Setup

### Prerequisites

Install these libraries (via Arduino IDE Library Manager or `arduino-cli lib install`):

- `ESP8266WiFi`, `ESPAsyncWebServer`, `ESPAsyncTCP`, `WebSerial`, `LittleFS`
- `IRremote` (shirriff/z3t0), `ArduinoOTA`, `ESP8266mDNS`, `EEPROM`
- `PubSubClient` (for MQTT)

### Credentials

Create a library file `<Arduino>/libraries/credentials/credentials.h` with:

```cpp
#define STASSID "YourWiFiSSID"
#define STAPSK  "YourWiFiPassword"

#define MQTT_SERVER "homeassistant.local"
#define MQTT_PORT   1883
// Optional authentication:
// #define MQTT_USER "your_mqtt_user"
// #define MQTT_PASS "your_mqtt_password"
```

### Build & Upload

- **Board**: ESP8266 (NodeMCU v2) — FQBN: `esp8266:esp8266:nodemcuv2`
- **Compile**: `arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --libraries ~/Documents/Arduino/libraries .`
- **Upload via USB**: Use Arduino IDE or `arduino-cli upload`
- **Upload via OTA**: ArduinoOTA is enabled; the device advertises as "amplifier" on the network

## MQTT Topics

| Topic | Direction | Retained | Description |
|-------|-----------|----------|-------------|
| `technics_amplifier/command` | Subscribe | No | Send IR command name (e.g. `power_toggle`, `volume_up`) |
| `technics_amplifier/power` | Publish | Yes | Amplifier power state: `ON` / `OFF` |
| `technics_amplifier/bt_audio` | Publish | Yes | Bluetooth audio state: `ON` / `OFF` |
| `technics_amplifier/event` | Publish | No | Events: `power_on`, `power_off`, `bt_audio_started`, `bt_audio_stopped`, `auto_power_on`, `command_sent:<cmd>` |
| `technics_amplifier/available` | Publish | Yes | Device availability: `online` / `offline` (LWT) |

### Home Assistant Example

```yaml
mqtt:
  switch:
    - name: "Technics Amplifier"
      command_topic: "technics_amplifier/command"
      state_topic: "technics_amplifier/power"
      payload_on: "power_toggle"
      payload_off: "power_toggle"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "technics_amplifier/available"

  binary_sensor:
    - name: "Technics BT Audio"
      state_topic: "technics_amplifier/bt_audio"
      payload_on: "ON"
      payload_off: "OFF"
      availability_topic: "technics_amplifier/available"
```

## Web Interface

The web UI is served at `http://<Device IP>/` and provides:

- Power/volume/mute controls
- Input selection (VCR1, CD, Deck)
- CD and Deck transport controls
- Live power and BT audio status (auto-refreshes every 3 seconds)

### REST API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/send?cmd=<command>` | GET | Send an IR command |
| `/api/status` | GET | Get JSON status: `{"power": true/false, "bt_audio": true/false}` |
| `/api/commands` | GET | List all available IR command names as JSON array |
| `/log` | GET | View the log file |
| `/webserial` | — | WebSerial debug console |

## WebSerial Commands

| Command | Description |
|---------|-------------|
| `send:<code>` | Send an IR command (e.g. `send:power_toggle`) |
| `status` | Show current power and BT audio state |
| `list` | List all available IR codes |
| `help` | Show available commands |

## Available IR Commands

`power_toggle`, `volume_up`, `volume_down`, `mute`, `vcr1_select`, `deck_select`, `deck_record`, `deck_play_fwd`, `deck_play_bwd`, `deck_stop`, `deck_pause`, `cd_select`, `cd_play`, `cd_stop`, `cd_fwd`, `cd_bwd`

## Auto Power-On Behavior

When the Bluetooth audio signal goes HIGH (music starts playing) and the amplifier is detected as OFF, the device automatically sends a `power_toggle` IR command to turn on the amplifier. The BT audio signal is debounced with a 2-second delay to avoid false triggers. This event is reported to MQTT as `auto_power_on`.

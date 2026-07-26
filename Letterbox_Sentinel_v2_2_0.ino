/*
  Letterbox Sentinel v2.2.0
  Board: LOLIN/Wemos D1 mini (ESP8266)

  FEATURES
  --------
  - BME280 on D1/D2
  - Sea-level corrected pressure for Maiden Gully (~230 m ASL)
  - IR receiver on D5
  - Scheduled IR emitter control on D6 via transistor
  - IR active Monday-Saturday, 07:30-18:00 until the first real letter of the day
  - IR disabled overnight and all Sunday
  - Persistent Mail Waiting state
  - Persistent lifetime letter counter
  - NFC/Home Assistant collection command with collector name
  - Battery voltage and estimated percentage on A0
  - Battery divider calibrated for current hardware
  - Arduino OTA
  - Home Assistant MQTT Discovery
  - Persistent state in EEPROM
  - Proven ESP8266 timed deep sleep outside IR operating hours
  - IR transmitter forced OFF before every sleep
  - D0/GPIO16 -> RST timed wake support
  - Maximum 60-minute sleep chunks so time/MQTT are refreshed periodically
  - After first real letter: IR OFF + Wi-Fi modem sleep until 18:00
  - NFC/MQTT remains available during post-delivery power-save mode

  DEEP SLEEP WIRING
  -----------------
  D0 / GPIO16 -> RST is REQUIRED for automatic wake-up.

  REQUIRED LIBRARIES
  ------------------
  PubSubClient
  ArduinoJson 7
  Adafruit BME280 Library
  Adafruit Unified Sensor
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>

// NOTE: credentials intentionally redacted for public repository.
const char* WIFI_SSID = "IOT_2.4";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";
const char* MQTT_HOST = "192.168.0.211";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "mqtt";
const char* MQTT_PASSWORD = "CHANGE_MQTT_PASSWORD";
const char* OTA_HOSTNAME = "letterbox-sentinel";
const char* OTA_PASSWORD = "CHANGE_OTA_PASSWORD";

// The full source for v2.2.0 is maintained locally. This public copy has had
// secrets removed before upload. Replace the placeholders above before use.

// Please use the attached/downloaded v2.2.0 source as the canonical firmware.

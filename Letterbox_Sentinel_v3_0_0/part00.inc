/*
  Letterbox Sentinel v3.0.0
  Board: LOLIN/Wemos D1 mini (ESP8266)

  FEATURES
  --------
  - BME280 on D1/D2
  - Sea-level corrected pressure for Maiden Gully (~230 m ASL)
  - IR receiver on D5
  - Scheduled IR emitter control on D6 via transistor
  - IR active Monday-Saturday, 07:30-17:00
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
  - Multiple deliveries per day, even while Mail Waiting remains ON
  - 30-second IR lockout after each genuine delivery, then automatic rearm
  - Mail Waiting remains persistent until NFC/manual collection

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

// ============================================================
// USER SETTINGS
// ============================================================

const char* WIFI_SSID = "IOT_2.4";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";

const char* MQTT_HOST = "192.168.0.211";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "mqtt";
const char* MQTT_PASSWORD = "CHANGE_MQTT_PASSWORD";

const char* OTA_HOSTNAME = "letterbox-sentinel";
const char* OTA_PASSWORD = "CHANGE_OTA_PASSWORD";

IPAddress DEVICE_IP(192, 168, 0, 220);
IPAddress GATEWAY_IP(192, 168, 0, 1);
IPAddress SUBNET_MASK(255, 255, 255, 0);
IPAddress DNS_SERVER(192, 168, 0, 1);

const char* TIMEZONE_RULE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.google.com";

const float SITE_ALTITUDE_METERS = 230.0F;

const float ADC_FULL_SCALE_V = 3.20F;
const float BATTERY_DIVIDER_RATIO = 5.35F;
const float BATTERY_CALIBRATION = 0.975F;

const uint8_t IR_RECEIVER_PIN = D5;
const uint8_t IR_EMITTER_CONTROL_PIN = D6;
const uint8_t IR_EMITTER_ON_LEVEL = HIGH;
const uint8_t IR_EMITTER_OFF_LEVEL = LOW;
const uint8_t IR_BEAM_BROKEN_LEVEL = LOW;

const unsigned long IR_DEBOUNCE_MS = 60;
const unsigned long IR_REARM_CLEAR_MS = 500;
const unsigned long IR_STARTUP_SETTLE_MS = 1000;
const unsigned long IR_MIN_EVENT_GAP_MS = 2000;
const unsigned long IR_POST_DELIVERY_LOCKOUT_MS = 30000;

const uint8_t IR_START_HOUR = 7;
const uint8_t IR_START_MINUTE = 30;
const uint8_t IR_END_HOUR = 17;
const uint8_t IR_END_MINUTE = 0;

const uint64_t DEEP_SLEEP_MAX_US = 60ULL * 60ULL * 1000000ULL;
const unsigned long DEEP_SLEEP_BOOT_GRACE_MS = 10000UL;

const char* DEVICE_ID = "letterbox_sentinel";
const char* DEVICE_NAME = "Letterbox Sentinel";
const char* FIRMWARE_VERSION = "3.0.0";
const char* DEVICE_MANUFACTURER = "DIY";
const char* DEVICE_MODEL = "Wemos D1 Mini Letterbox Sentinel";

const char* TOPIC_AVAILABILITY = "letterbox_sentinel/status";
const char* TOPIC_TEMPERATURE = "letterbox_sentinel/bme280/temperature";
const char* TOPIC_HUMIDITY = "letterbox_sentinel/bme280/humidity";
const char* TOPIC_PRESSURE = "letterbox_sentinel/bme280/pressure";
const char* TOPIC_IR_BEAM = "letterbox_sentinel/mail_slot/beam";
const char* TOPIC_IR_EMITTER = "letterbox_sentinel/mail_slot/emitter_active";
const char* TOPIC_MAIL_WAITING = "letterbox_sentinel/mail/waiting";
const char* TOPIC_LETTER_COUNT = "letterbox_sentinel/mail/letter_count";
const char* TOPIC_LAST_MAIL = "letterbox_sentinel/mail/last_detected";
const char* TOPIC_LAST_COLLECTOR = "letterbox_sentinel/mail/last_collector";
const char* TOPIC_LAST_COLLECTION = "letterbox_sentinel/mail/last_collection";
const char* TOPIC_MAIL_EVENT = "letterbox_sentinel/event/mail_detected";
const char* TOPIC_COLLECTION_EVENT = "letterbox_sentinel/event/mail_collected";
const char* TOPIC_BATTERY_VOLTAGE = "letterbox_sentinel/battery/voltage";
const char* TOPIC_BATTERY_PERCENT = "letterbox_sentinel/battery/percent";
const char* TOPIC_LOW_BATTERY = "letterbox_sentinel/battery/low";
const char* TOPIC_WIFI_RSSI = "letterbox_sentinel/diagnostic/wifi_rssi";
const char* TOPIC_UPTIME = "letterbox_sentinel/diagnostic/uptime";
const char* TOPIC_IP = "letterbox_sentinel/diagnostic/ip";
const char* TOPIC_SSID = "letterbox_sentinel/diagnostic/ssid";
const char* TOPIC_MAC = "letterbox_sentinel/diagnostic/mac";
const char* TOPIC_GATEWAY = "letterbox_sentinel/diagnostic/gateway";
const char* TOPIC_DNS = "letterbox_sentinel/diagnostic/dns";
const char* TOPIC_RESET_REASON = "letterbox_sentinel/diagnostic/reset_reason";
const char* TOPIC_FREE_HEAP = "letterbox_sentinel/diagnostic/free_heap";
const char* TOPIC_FIRMWARE = "letterbox_sentinel/diagnostic/firmware";
const char* TOPIC_SAFE_MODE = "letterbox_sentinel/diagnostic/safe_mode";
const char* TOPIC_TIME_VALID = "letterbox_sentinel/diagnostic/time_valid";
const char* CMD_CLEAR_MAIL = "letterbox_sentinel/command/clear_mail";
const char* CMD_RESET_COUNTER = "letterbox_sentinel/command/reset_counter";
const char* CMD_TEST_MAIL = "letterbox_sentinel/command/test_mail";
const char* CMD_RESTART = "letterbox_sentinel/command/restart";
const char* CMD_SAFE_MODE = "letterbox_sentinel/command/safe_mode";
const char* CMD_EXIT_SAFE_MODE = "letterbox_sentinel/command/exit_safe_mode";
const char* CMD_MAIL_COLLECTED = "letterbox_sentinel/command/mail_collected";
const char* HOME_ASSISTANT_STATUS_TOPIC = "homeassistant/status";

const unsigned long SENSOR_PUBLISH_INTERVAL_MS = 60000;
const unsigned long NETWORK_PUBLISH_INTERVAL_MS = 300000;
const unsigned long SCHEDULE_CHECK_INTERVAL_MS = 1000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

const uint32_t PERSIST_MAGIC = 0x4C425332;
const uint16_t PERSIST_VERSION = 1;
const size_t EEPROM_SIZE = 512;

struct PersistentData {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t letterCount;
  uint32_t bootCount;
  uint32_t lastMailEpoch;
  uint32_t lastCollectionEpoch;
  bool mailWaiting;
  bool safeMode;
  char lastCollector[40];
  uint32_t checksum;
};

PersistentData persistent;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_BME280 bme280;

bool bmeAvailable = false;
bool otaStarted = false;
bool timeConfigured = false;
bool emitterActive = false;
unsigned long emitterEnabledAtMs = 0;
bool rawBeamBroken = false;
bool stableBeamBroken = false;
bool mailDetectionArmed = false;
unsigned long rawBeamChangedAtMs = 0;
unsigned long beamClearSinceMs = 0;
unsigned long lastMailEventMs = 0;
unsigned long lastSensorPublishMs = 0;
unsigned long lastNetworkPublishMs = 0;
unsigned long lastScheduleCheckMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMQTTAttemptMs = 0;
bool sleepCheckedThisBoot = false;
unsigned long irLockoutUntilMs = 0;

uint32_t calculateChecksum(const PersistentData& data) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
  const size_t length = sizeof(PersistentData) - sizeof(data.checksum);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; i++) {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }
  return hash;
}

void savePersistentData() {
  persistent.magic = PERSIST_MAGIC;
  persistent.version = PERSIST_VERSION;
  persistent.checksum = calculateChecksum(persistent);
  EEPROM.put(0, persistent);
  EEPROM.commit();
}

void loadPersistentData() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, persistent);
  const bool valid = persistent.magic == PERSIST_MAGIC && persistent.version == PERSIST_VERSION && persistent.checksum == calculateChecksum(persistent);
  if (!valid) {
    memset(&persistent, 0, sizeof(persistent));
    persistent.magic = PERSIST_MAGIC;
    persistent.version = PERSIST_VERSION;
    persistent.mailWaiting = false;
    persistent.safeMode = false;
    strlcpy(persistent.lastCollector, "Never collected", sizeof(persistent.lastCollector));
  }
  persistent.bootCount++;
  savePersistentData();
}

bool timeIsValid() { return time(nullptr) > 1700000000; }

String epochToIso8601(uint32_t epoch) {
  if (epoch == 0) return "unknown";
  time_t raw = static_cast<time_t>(epoch);
  struct tm utcTime;
  gmtime_r(&raw, &utcTime);
  char buffer[28];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
  return String(buffer);
}

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

// ============================================================
// USER SETTINGS
// ============================================================

const char* WIFI_SSID = "IOT_2.4";
const char* WIFI_PASSWORD = "DarkPionCade";

const char* MQTT_HOST = "192.168.0.211";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "mqtt";
const char* MQTT_PASSWORD = "Userjobs01";

const char* OTA_HOSTNAME = "letterbox-sentinel";
const char* OTA_PASSWORD = "Userjobs01";

IPAddress DEVICE_IP(192, 168, 0, 220);
IPAddress GATEWAY_IP(192, 168, 0, 1);
IPAddress SUBNET_MASK(255, 255, 255, 0);
IPAddress DNS_SERVER(192, 168, 0, 1);

// Melbourne/Victoria timezone with daylight saving.
const char* TIMEZONE_RULE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.google.com";

// Maiden Gully site altitude.
const float SITE_ALTITUDE_METERS = 230.0F;

// Battery calibration.
// Current external divider: 330k / 100k.
// Calibrated using 7.80 V multimeter reading.
const float ADC_FULL_SCALE_V = 3.20F;
const float BATTERY_DIVIDER_RATIO = 5.35F;
const float BATTERY_CALIBRATION = 0.975F;

// ============================================================
// HARDWARE
// ============================================================

const uint8_t IR_RECEIVER_PIN = D5;
const uint8_t IR_EMITTER_CONTROL_PIN = D6;

const uint8_t IR_EMITTER_ON_LEVEL = HIGH;
const uint8_t IR_EMITTER_OFF_LEVEL = LOW;

// IR receiver assumes LOW means beam interrupted.
const uint8_t IR_BEAM_BROKEN_LEVEL = LOW;

const unsigned long IR_DEBOUNCE_MS = 60;
const unsigned long IR_REARM_CLEAR_MS = 500;
const unsigned long IR_STARTUP_SETTLE_MS = 1000;
const unsigned long IR_MIN_EVENT_GAP_MS = 2000;

const uint8_t IR_START_HOUR = 7;
const uint8_t IR_START_MINUTE = 30;
const uint8_t IR_END_HOUR = 18;
const uint8_t IR_END_MINUTE = 0;

// Deep sleep is used only when the IR monitoring window is inactive.
// During Monday-Saturday 07:30-18:00 the ESP remains awake.
// Before the first real letter it continuously monitors the IR beam.
// After the first real letter, the IR transmitter is switched OFF and Wi-Fi
// modem sleep is enabled, while MQTT/NFC collection remains available.
//
// IMPORTANT: D0 / GPIO16 MUST be connected to RST for timed wake-up.
const uint64_t DEEP_SLEEP_MAX_US = 60ULL * 60ULL * 1000000ULL; // 60 minutes
const unsigned long DEEP_SLEEP_BOOT_GRACE_MS = 10000UL;        // allow MQTT/OTA/NTP after wake

// ============================================================
// DEVICE INFORMATION
// ============================================================

const char* DEVICE_ID = "letterbox_sentinel";
const char* DEVICE_NAME = "Letterbox Sentinel";
const char* FIRMWARE_VERSION = "2.2.0";
const char* DEVICE_MANUFACTURER = "DIY";
const char* DEVICE_MODEL = "Wemos D1 Mini Letterbox Sentinel";

// ============================================================
// MQTT TOPICS
// ============================================================

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

// ============================================================
// TIMING
// ============================================================

const unsigned long SENSOR_PUBLISH_INTERVAL_MS = 60000;
const unsigned long NETWORK_PUBLISH_INTERVAL_MS = 300000;
const unsigned long SCHEDULE_CHECK_INTERVAL_MS = 1000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

// ============================================================
// PERSISTENT STORAGE
// ============================================================

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

// ============================================================
// OBJECTS
// ============================================================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_BME280 bme280;

// ============================================================
// RUNTIME STATE
// ============================================================

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

// ============================================================
// PERSISTENCE
// ============================================================

uint32_t calculateChecksum(const PersistentData& data) {
  const uint8_t* bytes =
    reinterpret_cast<const uint8_t*>(&data);

  const size_t length =
    sizeof(PersistentData) - sizeof(data.checksum);

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

  const bool valid =
    persistent.magic == PERSIST_MAGIC &&
    persistent.version == PERSIST_VERSION &&
    persistent.checksum == calculateChecksum(persistent);

  if (!valid) {
    memset(&persistent, 0, sizeof(persistent));

    persistent.magic = PERSIST_MAGIC;
    persistent.version = PERSIST_VERSION;
    persistent.mailWaiting = false;
    persistent.safeMode = false;

    strlcpy(
      persistent.lastCollector,
      "Never collected",
      sizeof(persistent.lastCollector)
    );
  }

  persistent.bootCount++;
  savePersistentData();
}

// ============================================================
// TIME
// ============================================================

bool timeIsValid() {
  return time(nullptr) > 1700000000;
}

String epochToIso8601(uint32_t epoch) {
  if (epoch == 0) {
    return "unknown";
  }

  time_t raw = static_cast<time_t>(epoch);

  struct tm utcTime;
  gmtime_r(&raw, &utcTime);

  char buffer[28];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%SZ",
    &utcTime
  );

  return String(buffer);
}

// ============================================================
// MQTT HELPERS
// ============================================================

bool publishRetained(const char* topic, const String& value) {
  if (!mqttClient.connected()) return false;

  return mqttClient.publish(
    topic,
    value.c_str(),
    true
  );
}

bool publishRetained(const char* topic, const char* value) {
  if (!mqttClient.connected()) return false;

  return mqttClient.publish(
    topic,
    value,
    true
  );
}

// ============================================================
// HOME ASSISTANT DISCOVERY HELPERS
// ============================================================

void addDeviceInformation(JsonDocument& document) {
  JsonObject device =
    document["device"].to<JsonObject>();

  JsonArray identifiers =
    device["identifiers"].to<JsonArray>();

  identifiers.add(DEVICE_ID);

  device["name"] = DEVICE_NAME;
  device["manufacturer"] = DEVICE_MANUFACTURER;
  device["model"] = DEVICE_MODEL;
  device["sw_version"] = FIRMWARE_VERSION;
}

bool publishDiscoveryJson(
  const String& topic,
  JsonDocument& document
) {
  char payload[1800];

  const size_t needed = measureJson(document);

  if (needed >= sizeof(payload)) {
    Serial.println("Discovery JSON too large");
    return false;
  }

  serializeJson(
    document,
    payload,
    sizeof(payload)
  );

  return mqttClient.publish(
    topic.c_str(),
    payload,
    true
  );
}

void publishSensorDiscovery(
  const char* component,
  const char* objectId,
  const char* name,
  const char* uniqueId,
  const char* stateTopic,
  const char* deviceClass = nullptr,
  const char* stateClass = nullptr,
  const char* unit = nullptr,
  const char* icon = nullptr,
  const char* entityCategory = nullptr
) {
  JsonDocument doc;

  doc["name"] = name;
  doc["unique_id"] = uniqueId;
  doc["state_topic"] = stateTopic;
  doc["availability_topic"] = TOPIC_AVAILABILITY;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  if (deviceClass) {
    doc["device_class"] = deviceClass;
  }

  if (stateClass) {
    doc["state_class"] = stateClass;
  }

  if (unit) {
    doc["unit_of_measurement"] = unit;
  }

  if (icon) {
    doc["icon"] = icon;
  }

  if (entityCategory) {
    doc["entity_category"] = entityCategory;
  }

  addDeviceInformation(doc);

  String topic = "homeassistant/";
  topic += component;
  topic += "/";
  topic += DEVICE_ID;
  topic += "/";
  topic += objectId;
  topic += "/config";

  publishDiscoveryJson(topic, doc);
}

void publishBinarySensorDiscovery(
  const char* objectId,
  const char* name,
  const char* uniqueId,
  const char* stateTopic,
  const char* deviceClass = nullptr,
  const char* icon = nullptr,
  const char* entityCategory = nullptr
) {
  JsonDocument doc;

  doc["name"] = name;
  doc["unique_id"] = uniqueId;
  doc["state_topic"] = stateTopic;
  doc["availability_topic"] = TOPIC_AVAILABILITY;
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";

  if (deviceClass) {
    doc["device_class"] = deviceClass;
  }

  if (icon) {
    doc["icon"] = icon;
  }

  if (entityCategory) {
    doc["entity_category"] = entityCategory;
  }

  addDeviceInformation(doc);

  String topic = "homeassistant/binary_sensor/";
  topic += DEVICE_ID;
  topic += "/";
  topic += objectId;
  topic += "/config";

  publishDiscoveryJson(topic, doc);
}

void publishButtonDiscovery(
  const char* objectId,
  const char* name,
  const char* uniqueId,
  const char* commandTopic,
  const char* payload,
  const char* icon,
  const char* entityCategory = nullptr
) {
  JsonDocument doc;

  doc["name"] = name;
  doc["unique_id"] = uniqueId;
  doc["command_topic"] = commandTopic;
  doc["payload_press"] = payload;
  doc["availability_topic"] = TOPIC_AVAILABILITY;

  if (icon) {
    doc["icon"] = icon;
  }

  if (entityCategory) {
    doc["entity_category"] = entityCategory;
  }

  addDeviceInformation(doc);

  String topic = "homeassistant/button/";
  topic += DEVICE_ID;
  topic += "/";
  topic += objectId;
  topic += "/config";

  publishDiscoveryJson(topic, doc);
}

// ============================================================
// HOME ASSISTANT DISCOVERY
// ============================================================

void publishHomeAssistantDiscovery() {
  if (!mqttClient.connected()) return;

  publishSensorDiscovery(
    "sensor",
    "temperature",
    "Temperature",
    "letterbox_sentinel_temperature",
    TOPIC_TEMPERATURE,
    "temperature",
    "measurement",
    "°C"
  );

  publishSensorDiscovery(
    "sensor",
    "humidity",
    "Humidity",
    "letterbox_sentinel_humidity",
    TOPIC_HUMIDITY,
    "humidity",
    "measurement",
    "%"
  );

  publishSensorDiscovery(
    "sensor",
    "pressure",
    "Sea Level Pressure",
    "letterbox_sentinel_pressure",
    TOPIC_PRESSURE,
    "atmospheric_pressure",
    "measurement",
    "hPa"
  );

  publishBinarySensorDiscovery(
    "mail_waiting",
    "Mail Waiting",
    "letterbox_sentinel_mail_waiting",
    TOPIC_MAIL_WAITING,
    "occupancy",
    "mdi:mailbox-up"
  );

  publishBinarySensorDiscovery(
    "mail_slot_beam",
    "Mail Slot Beam",
    "letterbox_sentinel_mail_slot_beam",
    TOPIC_IR_BEAM,
    "motion",
    "mdi:email-arrow-down"
  );

  publishBinarySensorDiscovery(
    "ir_emitter_active",
    "IR Emitter Active",
    "letterbox_sentinel_ir_emitter_active",
    TOPIC_IR_EMITTER,
    nullptr,
    "mdi:infrared"
  );

  publishSensorDiscovery(
    "sensor",
    "letter_count",
    "Letter Counter",
    "letterbox_sentinel_letter_count",
    TOPIC_LETTER_COUNT,
    nullptr,
    "total_increasing",
    nullptr,
    "mdi:counter"
  );

  publishSensorDiscovery(
    "sensor",
    "last_mail_detected",
    "Last Mail Detected",
    "letterbox_sentinel_last_mail",
    TOPIC_LAST_MAIL,
    "timestamp"
  );

  publishSensorDiscovery(
    "sensor",
    "last_collector",
    "Last Collected By",
    "letterbox_sentinel_last_collector",
    TOPIC_LAST_COLLECTOR,
    nullptr,
    nullptr,
    nullptr,
    "mdi:account-check"
  );

  publishSensorDiscovery(
    "sensor",
    "last_collection_time",
    "Last Collection Time",
    "letterbox_sentinel_last_collection",
    TOPIC_LAST_COLLECTION,
    "timestamp"
  );

  publishSensorDiscovery(
    "sensor",
    "battery_voltage",
    "Battery Voltage",
    "letterbox_sentinel_battery_voltage",
    TOPIC_BATTERY_VOLTAGE,
    "voltage",
    "measurement",
    "V"
  );

  publishSensorDiscovery(
    "sensor",
    "battery_percent",
    "Battery",
    "letterbox_sentinel_battery_percent",
    TOPIC_BATTERY_PERCENT,
    "battery",
    "measurement",
    "%"
  );

  publishBinarySensorDiscovery(
    "low_battery",
    "Low Battery",
    "letterbox_sentinel_low_battery",
    TOPIC_LOW_BATTERY,
    "battery",
    "mdi:battery-alert"
  );

  publishSensorDiscovery(
    "sensor",
    "wifi_signal",
    "Wi-Fi Signal",
    "letterbox_sentinel_wifi_signal",
    TOPIC_WIFI_RSSI,
    "signal_strength",
    "measurement",
    "dBm",
    nullptr,
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "uptime",
    "Uptime",
    "letterbox_sentinel_uptime",
    TOPIC_UPTIME,
    "duration",
    "total_increasing",
    "s",
    nullptr,
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "free_heap",
    "Free Heap",
    "letterbox_sentinel_free_heap",
    TOPIC_FREE_HEAP,
    "data_size",
    "measurement",
    "B",
    nullptr,
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "ip_address",
    "IP Address",
    "letterbox_sentinel_ip",
    TOPIC_IP,
    nullptr,
    nullptr,
    nullptr,
    "mdi:ip-network",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "ssid",
    "Wi-Fi SSID",
    "letterbox_sentinel_ssid",
    TOPIC_SSID,
    nullptr,
    nullptr,
    nullptr,
    "mdi:wifi",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "mac_address",
    "MAC Address",
    "letterbox_sentinel_mac",
    TOPIC_MAC,
    nullptr,
    nullptr,
    nullptr,
    "mdi:identifier",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "gateway",
    "Gateway",
    "letterbox_sentinel_gateway",
    TOPIC_GATEWAY,
    nullptr,
    nullptr,
    nullptr,
    "mdi:router-network",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "dns_server",
    "DNS Server",
    "letterbox_sentinel_dns",
    TOPIC_DNS,
    nullptr,
    nullptr,
    nullptr,
    "mdi:dns",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "reset_reason",
    "Reset Reason",
    "letterbox_sentinel_reset_reason",
    TOPIC_RESET_REASON,
    nullptr,
    nullptr,
    nullptr,
    "mdi:restart-alert",
    "diagnostic"
  );

  publishSensorDiscovery(
    "sensor",
    "firmware",
    "Firmware",
    "letterbox_sentinel_firmware",
    TOPIC_FIRMWARE,
    nullptr,
    nullptr,
    nullptr,
    "mdi:chip",
    "diagnostic"
  );

  publishBinarySensorDiscovery(
    "safe_mode",
    "Safe Mode",
    "letterbox_sentinel_safe_mode",
    TOPIC_SAFE_MODE,
    nullptr,
    "mdi:shield-alert",
    "diagnostic"
  );

  publishBinarySensorDiscovery(
    "time_valid",
    "Network Time Valid",
    "letterbox_sentinel_time_valid",
    TOPIC_TIME_VALID,
    nullptr,
    "mdi:clock-check",
    "diagnostic"
  );

  publishButtonDiscovery(
    "clear_mail",
    "Clear Mail Waiting",
    "letterbox_sentinel_clear_mail",
    CMD_CLEAR_MAIL,
    "PRESS",
    "mdi:mailbox-open-up"
  );

  publishButtonDiscovery(
    "reset_counter",
    "Reset Letter Counter",
    "letterbox_sentinel_reset_counter",
    CMD_RESET_COUNTER,
    "PRESS",
    "mdi:counter"
  );

  publishButtonDiscovery(
    "test_mail",
    "Test Mail Detection",
    "letterbox_sentinel_test_mail",
    CMD_TEST_MAIL,
    "PRESS",
    "mdi:test-tube"
  );

  publishButtonDiscovery(
    "restart",
    "Restart",
    "letterbox_sentinel_restart",
    CMD_RESTART,
    "PRESS",
    "mdi:restart",
    "diagnostic"
  );

  publishButtonDiscovery(
    "safe_mode",
    "Enter Safe Mode",
    "letterbox_sentinel_enter_safe_mode",
    CMD_SAFE_MODE,
    "PRESS",
    "mdi:shield-alert",
    "diagnostic"
  );

  publishButtonDiscovery(
    "exit_safe_mode",
    "Exit Safe Mode",
    "letterbox_sentinel_exit_safe_mode",
    CMD_EXIT_SAFE_MODE,
    "PRESS",
    "mdi:shield-check",
    "diagnostic"
  );

  Serial.println("Home Assistant discovery published");
}

// ============================================================
// BATTERY
// ============================================================

float readBatteryVoltage() {
  const int raw = analogRead(A0);

  const float pinVoltage =
    (raw / 1023.0F) * ADC_FULL_SCALE_V;

  return pinVoltage *
         BATTERY_DIVIDER_RATIO *
         BATTERY_CALIBRATION;
}

int batteryPercentFromVoltage(float voltage) {
  if (voltage >= 8.35F) return 100;

  if (voltage >= 8.20F)
    return 90 +
      (int)((voltage - 8.20F) / 0.15F * 10.0F);

  if (voltage >= 8.00F)
    return 75 +
      (int)((voltage - 8.00F) / 0.20F * 15.0F);

  if (voltage >= 7.80F)
    return 55 +
      (int)((voltage - 7.80F) / 0.20F * 20.0F);

  if (voltage >= 7.60F)
    return 35 +
      (int)((voltage - 7.60F) / 0.20F * 20.0F);

  if (voltage >= 7.40F)
    return 20 +
      (int)((voltage - 7.40F) / 0.20F * 15.0F);

  if (voltage >= 7.20F)
    return 10 +
      (int)((voltage - 7.20F) / 0.20F * 10.0F);

  if (voltage >= 6.60F)
    return
      (int)((voltage - 6.60F) / 0.60F * 10.0F);

  return 0;
}

// ============================================================
// STATE PUBLISHING
// ============================================================

void publishMailState() {
  publishRetained(
    TOPIC_MAIL_WAITING,
    persistent.mailWaiting ? "ON" : "OFF"
  );

  publishRetained(
    TOPIC_LETTER_COUNT,
    String(persistent.letterCount)
  );

  publishRetained(
    TOPIC_LAST_MAIL,
    epochToIso8601(persistent.lastMailEpoch)
  );

  publishRetained(
    TOPIC_LAST_COLLECTOR,
    String(persistent.lastCollector)
  );

  publishRetained(
    TOPIC_LAST_COLLECTION,
    epochToIso8601(persistent.lastCollectionEpoch)
  );
}

void publishIRState() {
  publishRetained(
    TOPIC_IR_EMITTER,
    emitterActive ? "ON" : "OFF"
  );

  publishRetained(
    TOPIC_IR_BEAM,
    stableBeamBroken ? "ON" : "OFF"
  );
}

void publishDiagnostics() {
  publishRetained(
    TOPIC_WIFI_RSSI,
    String(WiFi.RSSI())
  );

  publishRetained(
    TOPIC_UPTIME,
    String(millis() / 1000UL)
  );

  publishRetained(
    TOPIC_FREE_HEAP,
    String(ESP.getFreeHeap())
  );

  publishRetained(
    TOPIC_IP,
    WiFi.localIP().toString()
  );

  publishRetained(
    TOPIC_SSID,
    WiFi.SSID()
  );

  publishRetained(
    TOPIC_MAC,
    WiFi.macAddress()
  );

  publishRetained(
    TOPIC_GATEWAY,
    WiFi.gatewayIP().toString()
  );

  publishRetained(
    TOPIC_DNS,
    WiFi.dnsIP().toString()
  );

  publishRetained(
    TOPIC_RESET_REASON,
    ESP.getResetReason()
  );

  publishRetained(
    TOPIC_FIRMWARE,
    FIRMWARE_VERSION
  );

  publishRetained(
    TOPIC_SAFE_MODE,
    persistent.safeMode ? "ON" : "OFF"
  );

  publishRetained(
    TOPIC_TIME_VALID,
    timeIsValid() ? "ON" : "OFF"
  );
}

// ============================================================
// SENSOR PUBLISHING
// ============================================================

void publishMeasurements() {
  if (!mqttClient.connected()) return;

  if (bmeAvailable) {
    char value[24];

    dtostrf(
      bme280.readTemperature(),
      1,
      2,
      value
    );

    publishRetained(
      TOPIC_TEMPERATURE,
      value
    );

    dtostrf(
      bme280.readHumidity(),
      1,
      2,
      value
    );

    publishRetained(
      TOPIC_HUMIDITY,
      value
    );

    const float stationPressure =
      bme280.readPressure() / 100.0F;

    const float seaLevelPressure =
      bme280.seaLevelForAltitude(
        SITE_ALTITUDE_METERS,
        stationPressure
      );

    dtostrf(
      seaLevelPressure,
      1,
      2,
      value
    );

    publishRetained(
      TOPIC_PRESSURE,
      value
    );
  }

  const float batteryVoltage =
    readBatteryVoltage();

  const int batteryPercent =
    batteryPercentFromVoltage(
      batteryVoltage
    );

  char voltageText[16];

  dtostrf(
    batteryVoltage,
    1,
    2,
    voltageText
  );

  publishRetained(
    TOPIC_BATTERY_VOLTAGE,
    voltageText
  );

  publishRetained(
    TOPIC_BATTERY_PERCENT,
    String(batteryPercent)
  );

  publishRetained(
    TOPIC_LOW_BATTERY,
    batteryVoltage <= 6.60F
      ? "ON"
      : "OFF"
  );

  publishRetained(
    TOPIC_WIFI_RSSI,
    String(WiFi.RSSI())
  );

  publishRetained(
    TOPIC_UPTIME,
    String(millis() / 1000UL)
  );

  publishRetained(
    TOPIC_FREE_HEAP,
    String(ESP.getFreeHeap())
  );

  publishRetained(
    TOPIC_TIME_VALID,
    timeIsValid()
      ? "ON"
      : "OFF"
  );
}

// ============================================================
// MAIL LOGIC
// ============================================================

void setEmitterActive(bool active);

bool sameLocalDay(uint32_t epoch) {
  if (!timeIsValid() || epoch == 0) return false;

  time_t nowRaw = time(nullptr);
  time_t eventRaw = static_cast<time_t>(epoch);

  struct tm nowLocal;
  struct tm eventLocal;

  localtime_r(&nowRaw, &nowLocal);
  localtime_r(&eventRaw, &eventLocal);

  return nowLocal.tm_year == eventLocal.tm_year &&
         nowLocal.tm_yday == eventLocal.tm_yday;
}

bool postDeliveryPowerSaveActive() {
  return sameLocalDay(persistent.lastMailEpoch);
}

void recordMailDetection(const char* source) {
  const unsigned long nowMs = millis();

  if (
    nowMs - lastMailEventMs <
      IR_MIN_EVENT_GAP_MS &&
    strcmp(source, "test") != 0
  ) {
    return;
  }

  lastMailEventMs = nowMs;

  persistent.mailWaiting = true;
  persistent.letterCount++;

  // Only a genuine IR detection starts the rest-of-day power-save mode.
  // The Home Assistant test button must not disable the real detector.
  if (timeIsValid() && strcmp(source, "ir") == 0) {
    persistent.lastMailEpoch =
      static_cast<uint32_t>(
        time(nullptr)
      );
  }

  savePersistentData();
  publishMailState();

  JsonDocument event;

  event["source"] = source;
  event["letter_count"] =
    persistent.letterCount;

  event["timestamp"] =
    epochToIso8601(
      persistent.lastMailEpoch
    );

  event["battery_voltage"] =
    readBatteryVoltage();

  char payload[384];

  serializeJson(
    event,
    payload,
    sizeof(payload)
  );

  mqttClient.publish(
    TOPIC_MAIL_EVENT,
    payload,
    false
  );

  Serial.print("MAIL DETECTED. Source: ");
  Serial.println(source);

  if (strcmp(source, "ir") == 0) {
    // One normal letter delivery per day is expected. Stop spending power on
    // the beam, but remain online so the NFC -> HA -> MQTT collection command
    // can still identify the collector and clear Mail Waiting.
    setEmitterActive(false);
    WiFi.setSleepMode(WIFI_MODEM_SLEEP);
    Serial.println("Post-delivery power save: IR OFF, Wi-Fi modem sleep ON");
  }
}

void clearMailWaiting(const char* collector) {
  persistent.mailWaiting = false;

  if (
    collector &&
    strlen(collector) > 0
  ) {
    strlcpy(
      persistent.lastCollector,
      collector,
      sizeof(persistent.lastCollector)
    );
  } else {
    strlcpy(
      persistent.lastCollector,
      "Unknown",
      sizeof(persistent.lastCollector)
    );
  }

  if (timeIsValid()) {
    persistent.lastCollectionEpoch =
      static_cast<uint32_t>(
        time(nullptr)
      );
  }

  savePersistentData();
  publishMailState();

  JsonDocument event;

  event["collector"] =
    persistent.lastCollector;

  event["timestamp"] =
    epochToIso8601(
      persistent.lastCollectionEpoch
    );

  event["letter_count"] =
    persistent.letterCount;

  char payload[384];

  serializeJson(
    event,
    payload,
    sizeof(payload)
  );

  mqttClient.publish(
    TOPIC_COLLECTION_EVENT,
    payload,
    false
  );

  Serial.print("Mail cleared by: ");
  Serial.println(
    persistent.lastCollector
  );
}


bool shouldEmitterBeActiveNow();

// ============================================================
// DEEP SLEEP
// ============================================================

bool insideIRClockWindow() {
  // This function is deliberately CLOCK-ONLY. Post-delivery IR shutdown must
  // not trigger deep sleep because MQTT/NFC needs to remain reachable.
  if (!timeIsValid()) return false;

  time_t nowRaw = time(nullptr);
  struct tm localTime;
  localtime_r(&nowRaw, &localTime);

  // Sunday = 0.
  if (localTime.tm_wday == 0) return false;

  const int minutesNow = localTime.tm_hour * 60 + localTime.tm_min;
  const int startMinutes = IR_START_HOUR * 60 + IR_START_MINUTE;
  const int endMinutes = IR_END_HOUR * 60 + IR_END_MINUTE;

  return minutesNow >= startMinutes && minutesNow < endMinutes;
}

bool outsideIRMonitoringWindow() {
  if (!timeIsValid()) return false;
  return !insideIRClockWindow();
}

uint64_t scheduledSleepDurationUs() {
  // Usually sleep for one hour. If this wake occurs within one hour of the
  // 07:30 monitoring start, shorten the final sleep so we wake close to 07:30
  // rather than overshooting the start by nearly an hour.
  if (!timeIsValid()) return DEEP_SLEEP_MAX_US;

  time_t nowRaw = time(nullptr);
  struct tm localTime;
  localtime_r(&nowRaw, &localTime);

  if (localTime.tm_wday != 0) {
    const int minutesNow = localTime.tm_hour * 60 + localTime.tm_min;
    const int startMinutes = IR_START_HOUR * 60 + IR_START_MINUTE;

    if (minutesNow < startMinutes) {
      const int secondsNow = localTime.tm_sec;
      const int secondsUntilStart =
        (startMinutes - minutesNow) * 60 - secondsNow;

      if (secondsUntilStart > 0 && secondsUntilStart < 3600) {
        return static_cast<uint64_t>(secondsUntilStart) * 1000000ULL;
      }
    }
  }

  return DEEP_SLEEP_MAX_US;
}

void enterScheduledDeepSleep() {
  // Hardware safety: force the transistor drive LOW even if runtime state
  // somehow disagrees with the schedule.
  digitalWrite(
    IR_EMITTER_CONTROL_PIN,
    IR_EMITTER_OFF_LEVEL
  );

  emitterActive = false;

  Serial.println();
  Serial.println(
    "================================="
  );
  Serial.println(
    " Scheduled deep sleep"
  );
  Serial.println(
    " IR transmitter: OFF"
  );
  const uint64_t sleepDurationUs = scheduledSleepDurationUs();
  const uint32_t sleepDurationSeconds =
    static_cast<uint32_t>(sleepDurationUs / 1000000ULL);

  Serial.print(" Sleep duration: ");
  Serial.print(sleepDurationSeconds);
  Serial.println(" seconds");
  Serial.println(
    " D0/GPIO16 -> RST required"
  );
  Serial.println(
    "================================="
  );

  // Tell Home Assistant/MQTT that the ESP is intentionally going offline.
  if (mqttClient.connected()) {
    publishIRState();

    mqttClient.publish(
      TOPIC_AVAILABILITY,
      "offline",
      true
    );

    mqttClient.loop();
    delay(100);

    mqttClient.disconnect();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.flush();
  delay(100);

  ESP.deepSleep(
    sleepDurationUs,
    WAKE_RF_DEFAULT
  );

  // deepSleep() should not return.
  delay(1000);
}

void manageScheduledDeepSleep() {
  // Give every wake a short online window. This lets NTP settle, MQTT publish
  // fresh measurements/state, and OTA remain reachable briefly.
  if (
    millis() <
      DEEP_SLEEP_BOOT_GRACE_MS
  ) {
    return;
  }

  // Once daytime monitoring is active this remains false and the ESP stays
  // awake continuously. Outside the monitoring window we sleep.
  if (
    outsideIRMonitoringWindow()
  ) {
    enterScheduledDeepSleep();
  }
}

// ============================================================
// IR SCHEDULE
// ============================================================

bool shouldEmitterBeActiveNow() {
  if (persistent.safeMode || !timeIsValid()) {
    return false;
  }

  // After today's genuine IR delivery, leave the transmitter OFF for the
  // remainder of the day even after the mail is collected by NFC.
  if (postDeliveryPowerSaveActive()) {
    return false;
  }

  return insideIRClockWindow();
}

void setEmitterActive(bool active) {
  if (active == emitterActive) {
    return;
  }

  emitterActive = active;

  digitalWrite(
    IR_EMITTER_CONTROL_PIN,
    active
      ? IR_EMITTER_ON_LEVEL
      : IR_EMITTER_OFF_LEVEL
  );

  if (active) {
    emitterEnabledAtMs = millis();
    mailDetectionArmed = false;
    beamClearSinceMs = 0;
  } else {
    stableBeamBroken = false;
    rawBeamBroken = false;
    mailDetectionArmed = false;
  }

  publishIRState();

  Serial.print("IR emitter: ");
  Serial.println(
    active ? "ON" : "OFF"
  );
}

void updateEmitterSchedule() {
  if (
    millis() -
      lastScheduleCheckMs <
      SCHEDULE_CHECK_INTERVAL_MS
  ) {
    return;
  }

  lastScheduleCheckMs = millis();

  setEmitterActive(
    shouldEmitterBeActiveNow()
  );
}

void updatePostDeliveryPowerSaving() {
  static bool modemSleepEnabled = false;

  const bool shouldSave = postDeliveryPowerSaveActive() && insideIRClockWindow();

  if (shouldSave && !modemSleepEnabled) {
    setEmitterActive(false);
    WiFi.setSleepMode(WIFI_MODEM_SLEEP);
    modemSleepEnabled = true;
    Serial.println("Post-delivery power save active");
  }

  if (!shouldSave && modemSleepEnabled) {
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    modemSleepEnabled = false;
    Serial.println("Post-delivery power save cleared for new monitoring period");
  }
}

// ============================================================
// IR DETECTION
// ============================================================

void monitorIRReceiver() {
  if (
    !emitterActive ||
    persistent.safeMode
  ) {
    return;
  }

  if (
    millis() -
      emitterEnabledAtMs <
      IR_STARTUP_SETTLE_MS
  ) {
    return;
  }

  const bool currentRawBroken =
    digitalRead(
      IR_RECEIVER_PIN
    ) ==
    IR_BEAM_BROKEN_LEVEL;

  if (
    currentRawBroken !=
      rawBeamBroken
  ) {
    rawBeamBroken =
      currentRawBroken;

    rawBeamChangedAtMs =
      millis();
  }

  if (
    rawBeamBroken !=
      stableBeamBroken &&
    millis() -
      rawBeamChangedAtMs >=
      IR_DEBOUNCE_MS
  ) {
    stableBeamBroken =
      rawBeamBroken;

    publishRetained(
      TOPIC_IR_BEAM,
      stableBeamBroken
        ? "ON"
        : "OFF"
    );

    if (stableBeamBroken) {
      beamClearSinceMs = 0;

      if (mailDetectionArmed) {
        mailDetectionArmed = false;
        recordMailDetection("ir");
      }
    } else {
      beamClearSinceMs =
        millis();
    }
  }

  if (
    !stableBeamBroken &&
    !mailDetectionArmed &&
    beamClearSinceMs != 0 &&
    millis() -
      beamClearSinceMs >=
      IR_REARM_CLEAR_MS
  ) {
    mailDetectionArmed = true;
  }

  if (
    !stableBeamBroken &&
    !mailDetectionArmed &&
    beamClearSinceMs == 0
  ) {
    beamClearSinceMs =
      millis();
  }
}

// ============================================================
// BME280
// ============================================================

void initialiseBME280() {
  Wire.begin(D2, D1);

  bmeAvailable =
    bme280.begin(
      0x76,
      &Wire
    ) ||
    bme280.begin(
      0x77,
      &Wire
    );

  Serial.println(
    bmeAvailable
      ? "BME280 ready"
      : "BME280 not found"
  );
}

// ============================================================
// WIFI
// ============================================================

void connectWiFi() {
  if (
    WiFi.status() ==
      WL_CONNECTED
  ) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(
    WIFI_NONE_SLEEP
  );

  WiFi.config(
    DEVICE_IP,
    GATEWAY_IP,
    SUBNET_MASK,
    DNS_SERVER
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print(
    "Connecting to Wi-Fi"
  );

  const unsigned long started =
    millis();

  while (
    WiFi.status() !=
      WL_CONNECTED &&
    millis() - started <
      30000
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() ==
      WL_CONNECTED
  ) {
    Serial.print(
      "Wi-Fi connected: "
    );

    Serial.println(
      WiFi.localIP()
    );
  } else {
    Serial.println(
      "Wi-Fi connection failed"
    );
  }
}

void maintainWiFi() {
  if (
    WiFi.status() ==
      WL_CONNECTED
  ) {
    return;
  }

  if (
    millis() -
      lastWiFiAttemptMs >=
      WIFI_RETRY_INTERVAL_MS
  ) {
    lastWiFiAttemptMs =
      millis();

    connectWiFi();
  }
}

// ============================================================
// TIME
// ============================================================

void initialiseTime() {
  if (
    timeConfigured ||
    WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  configTime(
    TIMEZONE_RULE,
    NTP_SERVER_1,
    NTP_SERVER_2
  );

  timeConfigured = true;

  Serial.println(
    "NTP configured"
  );
}

// ============================================================
// OTA
// ============================================================

void initialiseOTA() {
  if (
    otaStarted ||
    WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  ArduinoOTA.setHostname(
    OTA_HOSTNAME
  );

  ArduinoOTA.setPassword(
    OTA_PASSWORD
  );

  ArduinoOTA.onStart([]() {
    setEmitterActive(false);

    Serial.println(
      "OTA update starting"
    );
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(
      "\nOTA update complete"
    );
  });

  ArduinoOTA.onProgress(
    [](unsigned int progress,
       unsigned int total) {

      Serial.printf(
        "OTA progress: %u%%\r",
        progress /
          (total / 100)
      );
    }
  );

  ArduinoOTA.onError(
    [](ota_error_t error) {
      Serial.printf(
        "OTA error: %u\n",
        error
      );
    }
  );

  ArduinoOTA.begin();

  otaStarted = true;

  Serial.println(
    "OTA ready"
  );
}

// ============================================================
// MQTT COMMAND PROCESSING
// ============================================================

String payloadToString(
  byte* payload,
  unsigned int length
) {
  String result;

  result.reserve(length);

  for (
    unsigned int i = 0;
    i < length;
    i++
  ) {
    result +=
      static_cast<char>(
        payload[i]
      );
  }

  result.trim();

  return result;
}

String collectorFromPayload(
  const String& payload
) {
  if (
    payload.startsWith("{")
  ) {
    JsonDocument doc;

    if (
      deserializeJson(
        doc,
        payload
      ) ==
      DeserializationError::Ok
    ) {
      const char* collector =
        doc["collector"] |
        "Unknown";

      return String(collector);
    }
  }

  if (
    payload.length() > 0 &&
    payload != "PRESS"
  ) {
    return payload;
  }

  return
    "Home Assistant Manual Reset";
}

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {
  const String message =
    payloadToString(
      payload,
      length
    );

  if (
    strcmp(
      topic,
      HOME_ASSISTANT_STATUS_TOPIC
    ) == 0
  ) {
    if (message == "online") {
      publishHomeAssistantDiscovery();
      publishMailState();
      publishIRState();
      publishDiagnostics();
      publishMeasurements();
    }

    return;
  }

  if (
    strcmp(
      topic,
      CMD_CLEAR_MAIL
    ) == 0
  ) {
    clearMailWaiting(
      "Home Assistant Manual Reset"
    );
  }

  else if (
    strcmp(
      topic,
      CMD_MAIL_COLLECTED
    ) == 0
  ) {
    const String collector =
      collectorFromPayload(
        message
      );

    clearMailWaiting(
      collector.c_str()
    );
  }

  else if (
    strcmp(
      topic,
      CMD_RESET_COUNTER
    ) == 0
  ) {
    persistent.letterCount = 0;

    savePersistentData();
    publishMailState();
  }

  else if (
    strcmp(
      topic,
      CMD_TEST_MAIL
    ) == 0
  ) {
    recordMailDetection(
      "test"
    );
  }

  else if (
    strcmp(
      topic,
      CMD_RESTART
    ) == 0
  ) {
    delay(250);
    ESP.restart();
  }

  else if (
    strcmp(
      topic,
      CMD_SAFE_MODE
    ) == 0
  ) {
    persistent.safeMode = true;

    savePersistentData();

    publishRetained(
      TOPIC_SAFE_MODE,
      "ON"
    );

    setEmitterActive(false);

    delay(250);
    ESP.restart();
  }

  else if (
    strcmp(
      topic,
      CMD_EXIT_SAFE_MODE
    ) == 0
  ) {
    persistent.safeMode = false;

    savePersistentData();

    publishRetained(
      TOPIC_SAFE_MODE,
      "OFF"
    );

    delay(250);
    ESP.restart();
  }
}

void subscribeCommands() {
  mqttClient.subscribe(
    HOME_ASSISTANT_STATUS_TOPIC
  );

  mqttClient.subscribe(
    CMD_CLEAR_MAIL
  );

  mqttClient.subscribe(
    CMD_RESET_COUNTER
  );

  mqttClient.subscribe(
    CMD_TEST_MAIL
  );

  mqttClient.subscribe(
    CMD_RESTART
  );

  mqttClient.subscribe(
    CMD_SAFE_MODE
  );

  mqttClient.subscribe(
    CMD_EXIT_SAFE_MODE
  );

  mqttClient.subscribe(
    CMD_MAIL_COLLECTED
  );
}

// ============================================================
// MQTT CONNECTION
// ============================================================

void connectMQTT() {
  if (
    WiFi.status() !=
      WL_CONNECTED ||
    mqttClient.connected()
  ) {
    return;
  }

  if (
    millis() -
      lastMQTTAttemptMs <
      MQTT_RETRY_INTERVAL_MS
  ) {
    return;
  }

  lastMQTTAttemptMs =
    millis();

  String clientId =
    "letterbox-sentinel-";

  clientId +=
    String(
      ESP.getChipId(),
      HEX
    );

  const bool connected =
    mqttClient.connect(
      clientId.c_str(),
      MQTT_USERNAME,
      MQTT_PASSWORD,
      TOPIC_AVAILABILITY,
      1,
      true,
      "offline"
    );

  if (!connected) {
    Serial.print(
      "MQTT failed, state "
    );

    Serial.println(
      mqttClient.state()
    );

    return;
  }

  Serial.println(
    "MQTT connected"
  );

  mqttClient.publish(
    TOPIC_AVAILABILITY,
    "online",
    true
  );

  subscribeCommands();

  publishHomeAssistantDiscovery();
  publishMailState();
  publishIRState();
  publishDiagnostics();
  publishMeasurements();

  lastSensorPublishMs =
    millis();

  lastNetworkPublishMs =
    millis();
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println(
    "================================="
  );

  Serial.println(
    " Letterbox Sentinel v2.2.0"
  );

  Serial.println(
    "================================="
  );

  loadPersistentData();

  pinMode(
    IR_RECEIVER_PIN,
    INPUT_PULLUP
  );

  pinMode(
    IR_EMITTER_CONTROL_PIN,
    OUTPUT
  );

  digitalWrite(
    IR_EMITTER_CONTROL_PIN,
    IR_EMITTER_OFF_LEVEL
  );

  initialiseBME280();

  mqttClient.setServer(
    MQTT_HOST,
    MQTT_PORT
  );

  mqttClient.setCallback(
    mqttCallback
  );

  mqttClient.setBufferSize(
    2048
  );

  mqttClient.setKeepAlive(
    30
  );

  connectWiFi();
  initialiseTime();
  initialiseOTA();
  connectMQTT();

  Serial.print(
    "Persistent mail waiting: "
  );

  Serial.println(
    persistent.mailWaiting
      ? "YES"
      : "NO"
  );

  Serial.print(
    "Persistent letter count: "
  );

  Serial.println(
    persistent.letterCount
  );

  Serial.print(
    "Safe mode: "
  );

  Serial.println(
    persistent.safeMode
      ? "ON"
      : "OFF"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  maintainWiFi();

  if (
    WiFi.status() ==
      WL_CONNECTED
  ) {
    initialiseTime();
    initialiseOTA();

    if (otaStarted) {
      ArduinoOTA.handle();
    }

    connectMQTT();

    if (
      mqttClient.connected()
    ) {
      mqttClient.loop();

      if (
        millis() -
          lastSensorPublishMs >=
          SENSOR_PUBLISH_INTERVAL_MS
      ) {
        lastSensorPublishMs =
          millis();

        publishMeasurements();
      }

      if (
        millis() -
          lastNetworkPublishMs >=
          NETWORK_PUBLISH_INTERVAL_MS
      ) {
        lastNetworkPublishMs =
          millis();

        publishDiagnostics();
      }
    }
  }

  updateEmitterSchedule();
  updatePostDeliveryPowerSaving();
  monitorIRReceiver();

  // Monday-Saturday 07:30-18:00: remain awake; after delivery use modem sleep.
  // Overnight and Sunday: IR is OFF and the ESP sleeps in 60-minute chunks.
  manageScheduledDeepSleep();

  delay(5);
}
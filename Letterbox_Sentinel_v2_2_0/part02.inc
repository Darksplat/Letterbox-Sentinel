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

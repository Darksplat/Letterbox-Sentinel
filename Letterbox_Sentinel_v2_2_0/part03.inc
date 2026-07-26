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

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

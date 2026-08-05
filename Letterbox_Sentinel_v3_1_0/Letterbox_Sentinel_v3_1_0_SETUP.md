# Letterbox Sentinel v3.1.0 — Setup Guide

This release makes the firmware portable across different home networks. It uses **DHCP by default**, so it does not assume that the network is `192.168.0.x`.

## 1. Required hardware

- LOLIN/Wemos D1 Mini (ESP8266)
- BME280 environmental sensor
- IR break-beam receiver on D5
- IR emitter switched from D6 through the transistor circuit
- 2S 18650 battery system and regulated 3.3 V supply
- 330 kΩ / 100 kΩ battery voltage divider to A0
- D0/GPIO16 connected to RST for timed deep-sleep wake

## 2. Required Arduino libraries

Install through **Arduino IDE → Tools → Manage Libraries**:

- PubSubClient
- ArduinoJson 7
- Adafruit BME280 Library
- Adafruit Unified Sensor

Also install the ESP8266 board package and select the correct Wemos/LOLIN D1 Mini board.

## 3. Edit only the USER CONFIGURATION section

Open `Letterbox_Sentinel_v3_1_0.ino` and locate:

```cpp
// USER CONFIGURATION - EDIT THIS SECTION ONLY
```

Enter the following values.

### Wi-Fi

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

The ESP8266 requires a 2.4 GHz Wi-Fi network.

### MQTT broker

```cpp
const char* MQTT_HOST = "homeassistant.local";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
```

`MQTT_HOST` can be a hostname or an IP address. Examples:

```cpp
const char* MQTT_HOST = "homeassistant.local";
```

or:

```cpp
const char* MQTT_HOST = "192.168.1.50";
```

Home Assistant and the MQTT broker are not necessarily the same service. When Mosquitto is installed as a Home Assistant add-on, the Home Assistant hostname or IP normally points to the correct machine.

### OTA password

```cpp
const char* OTA_PASSWORD = "CHOOSE_A_STRONG_PASSWORD";
```

The first firmware upload normally uses USB. Later updates can be uploaded wirelessly through Arduino OTA while the device is awake and connected.

## 4. DHCP or static IP

The default is:

```cpp
const bool USE_STATIC_IP = false;
```

Leave this as `false` for normal installations. The router will assign an address automatically, regardless of whether the network uses `192.168.0.x`, `192.168.1.x`, `10.x.x.x`, or another private subnet.

### Recommended fixed-address method

Leave DHCP enabled and create a **DHCP reservation** in the router for the Letterbox Sentinel MAC address. This gives the device a predictable address without hard-coding network details into the firmware.

### Optional firmware static IP

Only use this when all network values are known:

```cpp
const bool USE_STATIC_IP = true;

IPAddress DEVICE_IP(192, 168, 1, 220);
IPAddress GATEWAY_IP(192, 168, 1, 1);
IPAddress SUBNET_MASK(255, 255, 255, 0);
IPAddress DNS_SERVER(192, 168, 1, 1);
```

The address must be valid and unused on the installer's network.

## 5. Timezone and schedule

The supplied timezone rule is for Victoria, Australia:

```cpp
const char* TIMEZONE_RULE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
```

The default monitoring period is Monday–Saturday from 7:30 am to 5:00 pm:

```cpp
const uint8_t IR_START_HOUR = 7;
const uint8_t IR_START_MINUTE = 30;
const uint8_t IR_END_HOUR = 17;
const uint8_t IR_END_MINUTE = 0;
```

Sunday remains disabled.

## 6. Altitude and pressure

Set the installation altitude in metres above sea level:

```cpp
const float SITE_ALTITUDE_METERS = 230.0F;
```

This is used to publish weather-style sea-level pressure rather than raw station pressure.

## 7. Battery calibration

The supplied values match the documented 330 kΩ / 100 kΩ divider and the tested D1 Mini:

```cpp
const float ADC_FULL_SCALE_V = 3.20F;
const float BATTERY_DIVIDER_RATIO = 5.35F;
const float BATTERY_CALIBRATION = 0.975F;
```

After installation, compare the Home Assistant battery voltage with a multimeter. Fine-tune only `BATTERY_CALIBRATION` if necessary.

Example: if Home Assistant reads 8.00 V and the multimeter reads 7.80 V:

```text
7.80 / 8.00 = 0.975
```

## 8. First upload and serial check

Upload by USB, then open Serial Monitor at **115200 baud**. Confirm:

- Wi-Fi connects
- the assigned IP address is shown
- MQTT connects
- Home Assistant discovery publishes
- BME280 is detected
- Network Time Valid becomes ON
- IR Emitter Active follows the schedule

When DHCP is used, the serial log will show:

```text
Network mode: DHCP
Wi-Fi connected: <assigned address>
Gateway: <router address>
DNS server: <DNS address>
Address source: DHCP
```

## 9. Home Assistant

The device should appear under:

**Settings → Devices & services → MQTT → Devices**

The MQTT Discovery object IDs and `unique_id` values remain stable from v2.2.0 onward. Future firmware changes must not change them casually, because doing so creates duplicate/orphaned entities in Home Assistant.

## 10. Security before publishing

Never commit real credentials to a public GitHub repository. Replace them with placeholders before publishing:

```cpp
const char* WIFI_SSID = "CHANGE_WIFI_SSID";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";
const char* MQTT_USERNAME = "CHANGE_MQTT_USERNAME";
const char* MQTT_PASSWORD = "CHANGE_MQTT_PASSWORD";
const char* OTA_PASSWORD = "CHANGE_OTA_PASSWORD";
```

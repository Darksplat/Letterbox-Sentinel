# Letterbox Sentinel

Letterbox Sentinel is a battery-powered ESP8266/Wemos D1 Mini mail detector designed for Home Assistant using MQTT Discovery.

Current firmware: **v2.2.0**

## What it does

- Detects letters passing through the mail slot using an IR break-beam sensor.
- Keeps a persistent **Mail Waiting** state until the mail is collected.
- Keeps a persistent lifetime letter counter.
- Records the last mail detection time.
- Supports NFC-driven collection through Home Assistant, including the name of the collector.
- Publishes BME280 temperature, humidity and sea-level-corrected pressure.
- Monitors a 2S 18650 battery pack through A0.
- Publishes Wi-Fi and device diagnostics to Home Assistant.
- Supports Arduino OTA updates.
- Uses scheduled power saving and deep sleep to extend battery life.

## Current daily schedule

### Monday-Saturday

- **07:30**: normal mail monitoring begins.
- IR transmitter remains active until the first genuine letter detection.
- After the first real letter:
  - `Mail Waiting` becomes ON.
  - Lifetime counter increments.
  - IR transmitter is switched OFF for the rest of the day.
  - ESP8266 switches to Wi-Fi modem sleep.
  - MQTT remains available so NFC collection still works.
- **18:00**: scheduled deep-sleep operation begins.

### Sunday

The IR system remains disabled and the ESP8266 uses scheduled deep sleep.

Deep sleep is performed in chunks of up to 60 minutes. The final sleep before 07:30 is shortened so the device wakes close to the monitoring start time.

## Hardware

- Wemos/LOLIN D1 Mini (ESP8266)
- BME280 environmental sensor
- IR break-beam receiver
- IR emitter
- BC547 transistor for IR emitter switching
- 2S 18650 battery pack
- TRACO Power TSR 1-2433 3.3 V switching regulator
- 330 kOhm / 100 kOhm battery voltage divider
- Optional 100 nF capacitor on A0

### Custom PCB

A custom 2-layer KiCad carrier PCB has now been developed for Letterbox Sentinel. The hardware design is documented under [`hardware/`](hardware/README.md).

Key PCB features:

- Approximately **64 mm x 50 mm**.
- Socketed Wemos D1 Mini for serviceability.
- Socketed **TRACO TSR 1-2433** 3.3 V regulator.
- Choice of **5.5 x 2.1 mm centre-positive barrel jack** or **AMASS XT60PW-F female** battery input.
- Raw 2S battery monitoring before regulation using the 330 kOhm / 100 kOhm divider.
- 1000 uF / 25 V reservoir capacitor on the 3.3 V rail.
- BC547-switched IR transmitter output.
- Female sockets for IR TX, IR RX and BME280.
- Bottom-layer GND copper plane.
- Through-hole construction for practical hand assembly and repair.

The final routed board passed KiCad DRC with **0 errors and 0 unconnected pads** during the manufacturing review.

## Pinout

| Function | D1 Mini pin |
|---|---|
| BME280 SCL | D1 |
| BME280 SDA | D2 |
| IR receiver signal | D5 |
| IR emitter transistor control | D6 |
| Battery voltage | A0 |
| Deep-sleep wake | D0/GPIO16 -> RST |

## IR emitter switching

D6 is a control signal only. The IR emitter is switched through a BC547 transistor.

```text
D6 ---- 4.7k ---- BC547 base
BC547 emitter ---- GND
BC547 collector -- IR emitter negative
IR emitter positive -- 3.3 V rail
```

The IR receiver remains connected to D5.

## Battery monitoring

Current external divider:

```text
Battery + ---- 330k ----+---- A0
                        |
                      100k
                        |
Battery - ------------- GND
```

The current firmware uses:

```cpp
const float ADC_FULL_SCALE_V = 3.20F;
const float BATTERY_DIVIDER_RATIO = 5.35F;
const float BATTERY_CALIBRATION = 0.975F;
```

These values were calibrated against a multimeter reading on the installed Wemos D1 Mini and divider network.

## BME280 pressure

Pressure is corrected to sea level using a site altitude of approximately **230 m above sea level** for Maiden Gully, Victoria.

```cpp
const float SITE_ALTITUDE_METERS = 230.0F;
```

Change this value for installations at another elevation.

## NFC mail collection

The NFC tag is read by a phone/Home Assistant rather than directly by the ESP8266.

Home Assistant should publish the collection command to:

```text
letterbox_sentinel/command/mail_collected
```

Example payload:

```json
{"collector":"Leigh"}
```

The firmware then:

- clears `Mail Waiting`;
- stores/publishes the collector name;
- stores/publishes the collection time;
- keeps the lifetime letter counter intact.

The IR transmitter does **not** restart after collection on the same day.

## Home Assistant MQTT Discovery

The firmware automatically creates sensors, binary sensors and buttons in Home Assistant using MQTT Discovery.

### Important compatibility rule

From firmware **v2.2.0 onward**, MQTT Discovery identifiers are treated as a stable API.

- Discovery object IDs should remain unchanged across firmware revisions.
- `unique_id` values should remain unchanged across firmware revisions.
- Display names, icons, units, calibration and firmware logic may change without changing the identifiers.
- If an entity genuinely has to be removed or replaced, the obsolete retained MQTT Discovery topic must be cleared so Home Assistant removes it cleanly.

This prevents duplicate/orphaned Home Assistant entities during firmware upgrades.

## Home Assistant controls

Current controls include:

- Clear Mail Waiting
- Reset Letter Counter
- Test Mail Detection
- Restart
- Enter Safe Mode
- Exit Safe Mode

The Test Mail Detection button does not activate the rest-of-day IR shutdown; only a genuine D5 IR detection does that.

## Required Arduino libraries

Install these using Arduino IDE Library Manager:

- PubSubClient
- ArduinoJson 7
- Adafruit BME280 Library
- Adafruit Unified Sensor

ESP8266 board support is also required.

## Firmware

Open:

```text
Letterbox_Sentinel_v2_2_0/Letterbox_Sentinel_v2_2_0.ino
```

The complete source is split into `part01.inc` through `part04.inc` and included sequentially by the main sketch.

Before compiling, edit the **USER SETTINGS** near the top of `part01.inc`.

The public repository intentionally contains placeholders instead of private Wi-Fi, MQTT and OTA credentials.

## Security

Never commit real Wi-Fi, MQTT or OTA passwords to a public repository.

The public firmware currently uses placeholders such as:

```cpp
const char* WIFI_SSID = "CHANGE_WIFI_SSID";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";
const char* MQTT_PASSWORD = "CHANGE_MQTT_PASSWORD";
const char* OTA_PASSWORD = "CHANGE_OTA_PASSWORD";
```

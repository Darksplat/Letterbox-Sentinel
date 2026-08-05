# Letterbox Sentinel

Letterbox Sentinel is a battery-powered ESP8266/Wemos D1 Mini mail detector for Home Assistant using MQTT Discovery.

Current firmware: **v3.1.0**

## What it does

- Detects each separate delivery through the mail slot using an IR break-beam sensor.
- Supports multiple deliveries per day, even while `Mail Waiting` is already ON.
- Keeps a persistent `Mail Waiting` state until the mailbox is emptied.
- Keeps a persistent lifetime delivery counter.
- Records the most recent mail detection time.
- Supports NFC-driven collection through Home Assistant, including the collector name.
- Publishes BME280 temperature, humidity and sea-level-corrected pressure.
- Monitors a 2S 18650 battery pack through A0.
- Publishes Wi-Fi, network and device diagnostics to Home Assistant.
- Supports Arduino OTA updates.
- Uses timed deep sleep outside delivery hours to extend battery life.
- Uses DHCP by default so it can work on different home-network address ranges.

## Daily operating schedule

### Monday-Saturday

- **07:30**: IR mail monitoring begins.
- **17:00**: IR monitoring ends and scheduled deep-sleep operation begins.
- Every genuine delivery:
  - sets `Mail Waiting` to ON;
  - increments the persistent delivery counter;
  - updates `Last Mail Detected`;
  - publishes a mail event to MQTT;
  - turns the IR emitter off for 30 seconds;
  - automatically rearms the IR detector after the lockout.

The detector continues to accept later deliveries during the same day. Mail does not need to be collected before another delivery can be detected.

### Sunday

The IR system remains disabled and the ESP8266 uses scheduled deep sleep.

Deep sleep is performed in chunks of up to 60 minutes. The final sleep before 07:30 is shortened so the device wakes close to the monitoring start time.

## Mail Waiting and delivery counting

`Mail Waiting` and the delivery detector are deliberately independent:

- `Mail Waiting` answers: **Is there uncollected mail in the box?**
- The delivery counter answers: **How many separate deliveries have been detected?**

Example:

```text
10:00 Australia Post delivery
Mail Waiting: ON
Letter Counter: 1

14:00 Courier delivery
Mail Waiting: still ON
Letter Counter: 2

16:00 NFC collection
Mail Waiting: OFF
Letter Counter: still 2
```

Clearing `Mail Waiting` does not reset the lifetime counter.

## Hardware

- Wemos/LOLIN D1 Mini (ESP8266)
- BME280 environmental sensor
- IR break-beam receiver
- IR emitter
- BC547 transistor for IR emitter switching
- 2S 18650 battery pack
- TRACO Power TSR 1-2433 3.3 V switching regulator
- 330 kΩ / 100 kΩ battery voltage divider
- Optional 100 nF capacitor on A0
- D0/GPIO16 to RST connection for timed deep-sleep wake

### Custom PCB

A custom 2-layer KiCad carrier PCB has been developed for Letterbox Sentinel. The hardware design is documented under [`hardware/`](hardware/README.md).

Key PCB features:

- Approximately **64 mm × 50 mm**.
- Socketed Wemos D1 Mini for serviceability.
- Socketed **TRACO TSR 1-2433** 3.3 V regulator.
- Choice of **5.5 × 2.1 mm centre-positive barrel jack** or **AMASS XT60PW-F female** battery input.
- Raw 2S battery monitoring before regulation using the 330 kΩ / 100 kΩ divider.
- 1000 µF / 25 V reservoir capacitor on the 3.3 V rail.
- BC547-switched IR transmitter output.
- Female sockets for IR TX, IR RX and BME280.
- Bottom-layer GND copper plane.
- Through-hole construction for practical hand assembly and repair.

The routed board passed KiCad DRC with **0 errors and 0 unconnected pads** during the manufacturing review.

## Pinout

| Function | D1 Mini pin |
|---|---|
| BME280 SCL | D1 |
| BME280 SDA | D2 |
| IR receiver signal | D5 |
| IR emitter transistor control | D6 |
| Battery voltage | A0 |
| Deep-sleep wake | D0/GPIO16 → RST |

## IR emitter switching

D6 is a control signal only. Do not power the IR emitter directly from D6.

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

These values were calibrated against a multimeter on the installed Wemos D1 Mini and divider network. Other boards may require recalibration.

## BME280 pressure

Pressure is corrected to sea level using the configured site altitude:

```cpp
const float SITE_ALTITUDE_METERS = 230.0F;
```

The default is approximately 230 m above sea level for Maiden Gully, Victoria. Change this value for another installation location.

## Network configuration

Firmware v3.1.0 uses **DHCP by default**:

```cpp
const bool USE_STATIC_IP = false;
```

This means the device can join networks using address ranges such as:

- `192.168.0.x`
- `192.168.1.x`
- `10.x.x.x`
- `172.16.x.x`

For a stable address, the recommended approach is to leave DHCP enabled and create a DHCP reservation in the router.

Static addressing is optional. The static IP, gateway, subnet and DNS values are ignored unless:

```cpp
const bool USE_STATIC_IP = true;
```

## MQTT broker address

The public firmware defaults to:

```cpp
const char* MQTT_HOST = "homeassistant.local";
```

This may be replaced with:

- the MQTT broker hostname;
- the Home Assistant hostname if the broker runs there; or
- the broker's IP address.

The MQTT broker does not have to run on Home Assistant.

## NFC mail collection

The NFC tag is read by a phone running Home Assistant. It is not read directly by the Wemos.

Home Assistant should publish to:

```text
letterbox_sentinel/command/mail_collected
```

Example payload:

```json
{"collector":"Leigh"}
```

The firmware then:

- clears `Mail Waiting`;
- stores and publishes the collector name;
- stores and publishes the collection time;
- keeps the lifetime delivery counter unchanged.

The manual **Clear Mail Waiting** button performs the same state clear without requiring NFC.

## Home Assistant MQTT Discovery

The firmware automatically creates sensors, binary sensors and buttons in Home Assistant using MQTT Discovery.

### Stable identifier rule

From firmware **v2.2.0 onward**, MQTT Discovery identifiers are treated as a stable API:

- Discovery object IDs remain unchanged across firmware revisions.
- `unique_id` values remain unchanged across firmware revisions.
- Display names, icons, units, calibration and firmware logic may change without changing identifiers.
- If an entity must be removed or replaced, the obsolete retained MQTT Discovery topic must be cleared.

This prevents duplicate and orphaned Home Assistant entities during upgrades.

## Home Assistant controls

Current controls include:

- Clear Mail Waiting
- Reset Letter Counter
- Test Mail Detection
- Restart
- Enter Safe Mode
- Exit Safe Mode

The **Test Mail Detection** button creates a test event but does not start the 30-second physical IR lockout.

## Home Assistant entities

Important entities include:

- Mail Waiting
- Mail Slot Beam
- IR Emitter Active
- Letter Counter
- Last Mail Detected
- Last Collected By
- Last Collection Time
- Battery
- Battery Voltage
- Low Battery
- Temperature
- Humidity
- Sea Level Pressure
- Wi-Fi Signal
- Uptime
- IP Address
- Wi-Fi SSID
- MAC Address
- Gateway
- DNS Server
- Reset Reason
- Firmware
- Safe Mode
- Network Time Valid

## Required Arduino libraries

Install using **Arduino IDE → Library Manager**:

- PubSubClient
- ArduinoJson 7
- Adafruit BME280 Library
- Adafruit Unified Sensor

Also install ESP8266 board support and select the correct Wemos/LOLIN D1 Mini board.

## Firmware setup

Open:

```text
Letterbox_Sentinel_v3_1_0.ino
```

Edit only the clearly marked:

```cpp
// USER CONFIGURATION - EDIT THIS SECTION ONLY
```

At minimum, configure:

- Wi-Fi SSID and password;
- MQTT host, port, username and password;
- OTA password;
- site altitude;
- operating hours if different;
- DHCP or optional static IP settings.

The public repository intentionally contains placeholders instead of private Wi-Fi, MQTT and OTA credentials.

## Security

Never commit real Wi-Fi, MQTT or OTA passwords to a public repository.

Use placeholders such as:

```cpp
const char* WIFI_SSID = "CHANGE_WIFI_SSID";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";
const char* MQTT_PASSWORD = "CHANGE_MQTT_PASSWORD";
const char* OTA_PASSWORD = "CHANGE_OTA_PASSWORD";
```

## Additional setup guide

See:

```text
Letterbox_Sentinel_v3_1_0_SETUP.md
```

for detailed first-time installation, Arduino IDE, DHCP/static IP, MQTT and testing instructions.

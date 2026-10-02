# Letterbox Sentinel — XIAO ESP32-C6 carrier revision C6-1

This is a separate hardware revision for the **Seeed Studio XIAO ESP32-C6**, retaining the 64 × 50 mm carrier outline and 2S battery inputs. The original ESP8266 design remains in the parent hardware directory.

## Changes

- Two removable 1×7 sockets, 2.54 mm pitch, **17.78 mm row spacing**. USB faces the bottom edge marked USB THIS EDGE. With USB facing the bottom edge, D0 is the bottom pin of the right-hand physical row and 5V is the bottom pin of the left-hand physical row. Use female sockets tall enough to clear the components underneath the XIAO; verify clearance with the actual board before fabrication.
- Remove BME280 connector and its I2C nets. No temperature, humidity or pressure sensing.
- Remove ESP8266 D0-to-RST wake connection.
- Keep the IR transmitter, receiver, BC547 switching circuit and battery divider.
- Add C2, a 100 nF ceramic capacitor, across the ADC divider's lower resistor.
- Replace the **TSR 1-2433** with **TSR 1-2450 (5 V)**. Its output supplies the XIAO 5V pin. The XIAO 3V3 output supplies the IR circuit and C1. Do not fit the old 3.3 V regulator to this revision.
- Correct D1's physical footprint numbering: **pad 1 = cathode = VIN_PROTECTED; pad 2 = anode = BAT_RAW+**. The diode's band faces the protected regulator supply.
- Correct Q1 to BC547's C-B-E pin order: **pad 1 collector = IR_TX_SW_GND; pad 2 base = Q1_BASE; pad 3 emitter = GND**. Verify the actual transistor manufacturer's pinout when assembling.
- Reroute both copper layers; the original ground pour is replaced by routed ground connections. A provisional clear area is reserved near the module's antenna end. RF clearance and reception must be confirmed with the actual XIAO and letterbox enclosure; this is not a manufacturer-approved RF layout.

## Firmware pin mapping

| Function | XIAO pin | ESP32-C6 GPIO |
|---|---|---|
| Battery ADC | D0 / A0 | GPIO0 |
| IR receiver | D1 | GPIO1 |
| IR emitter control | D2 | GPIO2 |
| Regulated supply input | 5V | — |
| IR supply output | 3V3 | — |
| Ground | GND | — |

The 330 kΩ / 100 kΩ divider ratio is **4.3**. At 8.4 V battery voltage, ADC voltage is approximately **1.953 V**. The old Wemos calibration factor 5.35 does not apply. Use appropriate ESP32-C6 ADC attenuation, calibrated millivolt readings and multimeter calibration. The 100 nF filter helps the high-impedance divider settle.

## Battery and power limitations

The TSR 1-2450 requires **at least 6.5 V at its input**. The protection diode reduces voltage before the regulator, so operation near the bottom of a 2S pack's discharge range is not guaranteed. A **7.0 V raw-pack low-battery threshold is an initial warning target**, not a hardware cutoff or a substitute for a protected/balanced 2S pack. Confirm voltage under load. This choice sacrifices usable capacity compared with the original 3.3 V regulator; a future low-quiescent-current power revision could improve endurance.

Use only one battery input at a time. Disconnect the carrier's battery supply before connecting the XIAO USB port, because its 5V pin shares USB VBUS. Leave XIAO BAT pads unused: they are for a single lithium cell, not this 2S pack. Confirm the IR modules' supply current is within the XIAO 3V3 regulator's capacity.

## Thread and Home Assistant

This hardware supports the ESP32-C6 radio. **The existing ESP8266 Wi-Fi/MQTT firmware has not been ported and cannot run on this board.** A separate ESP32-C6 firmware implementation is needed for Thread/OpenThread or Matter over Thread, including delivery counting, mail state, collection commands and the operating schedule. Thread alone does not provide Matter entities. A Thread border router is required. The ESP8266 deep-sleep behavior must be revisited because a sleeping device cannot remain available for collection commands.

## Opening and review

Open `Letterbox_Sentinel_C6.kicad_pro` in KiCad 10. The schematic, PCB, symbol library, footprint library and both library tables are included. Keep the entire directory together.

The revision has been checked in KiCad CLI 10.0.6. See `validation/` for the actual reports. Electrical DRC and ERC errors: zero. Unconnected PCB pads: zero. Schematic-to-PCB parity issues: zero. The isolated CLI environment still reports local footprint-library availability warnings; rerun full checks after opening the project in desktop KiCad.

This is a **review revision**, not a released manufacturing package: verify module orientation, socket height, USB access, antenna clearance, IR current draw and regulator behavior with the actual components. No Gerbers are supplied, and old manufacturing files are for the ESP8266 revision only.

## Primary references

- https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/
- https://wiki.seeedstudio.com/xiao_pin_multiplexing_esp32c6/
- https://www.tracopower.com/sites/default/files/products/datasheets/tsr1_datasheet.pdf
- https://www.onsemi.com/download/data-sheet/pdf/bc546-d.pdf

# Letterbox Sentinel PCB Hardware

This directory contains the KiCad hardware design for the Letterbox Sentinel carrier PCB.

## Current PCB design

- 2-layer FR-4 PCB
- Approximate board size: 64 mm x 50 mm
- Wemos/LOLIN D1 Mini (ESP8266) on removable female headers
- Socketed TRACO Power TSR 1-2433 3.3 V switching regulator
- Dual 2S battery input options: 5.5 x 2.1 mm centre-positive barrel jack or AMASS XT60PW-F female PCB connector
- 1N5819 input protection
- 1000 uF / 25 V 3.3 V reservoir capacitor
- 330 kOhm / 100 kOhm raw-battery divider to A0
- BC547-switched IR transmitter
- Female sockets for IR TX, IR RX and BME280
- Bottom-layer GND copper plane

## Important power note

Use either the barrel jack or XT60 input. Do not connect two independent power sources at the same time.

The battery-voltage divider senses the raw 2S input before regulation.

## KiCad files

The authoritative current schematic and routed PCB are in `kicad/`.

The PCB has been manually routed and checked in KiCad with no DRC errors and no unconnected pads at the final review stage. Any future schematic-to-PCB update should be reviewed carefully so the established component placement and routing are not unintentionally disturbed.

## Manufacturing

`manufacturing/` contains available fabrication drill outputs from the current KiCad project. Gerber files should be regenerated from the authoritative PCB immediately before placing a manufacturing order.

Suggested standard PCB specification:

- 2 layers
- FR-4
- 1.6 mm thickness
- 1 oz copper
- Green solder mask
- White silkscreen
- Lead-free HASL

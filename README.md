# CrowMotion

A wireless FPV head tracker you can build for about **$5**.

![License](https://img.shields.io/badge/license-Apache--2.0-blue)
![Status](https://img.shields.io/badge/status-work%20in%20progress-orange)
![Board](https://img.shields.io/badge/board-ESP32--C3-green)

Turn your head and the FPV camera follows. CrowMotion reads your head motion and
streams it to your RC radio over Bluetooth as trainer channels, so a pan/tilt
camera on your aircraft mirrors where you look. Two cheap parts and a USB power
bank (a printable case is coming later).

> **Work in progress.** This is an actively developed, unfinished project. The
> firmware (Bluetooth trainer link, IMU, sensor fusion, channel mapping,
> recenter, a WiFi config UI, and OTA updates) is implemented and builds, and
> the link has been bench-verified against a real radio. It has not been
> flight-tested yet, and a printable enclosure is not published yet. Expect
> rough edges and follow the roadmap below.

## Why

Commercial head trackers and IMU-equipped dev boards (XIAO nRF52840 Sense,
Arduino Nano 33 BLE Sense) cost many times more. CrowMotion does the same job with
a $2 microcontroller and a $2 IMU. No proprietary dongle, no soldering iron
required beyond four wires.

## How the whole system works

```
   your head                 your radio                 your aircraft
  +-----------+   Bluetooth  +-----------+   RC link    +--------------+
  | CrowMotion  | -----------> | FrSky     | -----------> | RX -> 2x     |
  | (tracker) |   PARA       | radio     |   channels   | servos -> cam|
  +-----------+   trainer    +-----------+              +--------------+
```

1. **CrowMotion** sits on your goggles and measures head pan, tilt, and roll.
2. It sends those as **trainer channels** to your radio over the FrSky PARA
   Bluetooth wireless trainer link.
3. Your radio forwards them to the aircraft like any other channels.
4. On the aircraft, **two servos** pan and tilt the FPV camera to match.

CrowMotion is the head-tracking part (step 1). You bring the radio, and you print
the servo camera mount (step 4) from an existing design, see below.

## Bill of materials

The head tracker itself:

| Part | What it does | Approx cost |
|---|---|---|
| ESP32-C3 Super Mini | microcontroller + Bluetooth | $2 - 3 |
| MPU6500 module | 6-axis IMU (accel + gyro) | $2 |
| 3D printed case | enclosure (coming later) | filament |
| USB-C power | any power bank or USB port | reuse |
| **Head tracker total** | | **~$5** |

To complete the camera side on your aircraft:

| Part | What it does | Approx cost |
|---|---|---|
| 2x micro servo (SG90 / MG90S) | pan and tilt the camera | $2 - 4 |
| 3D printed pan/tilt frame | holds the camera and servos | filament |
| **Full system** | tracker + camera mount | **~$8 - 10** |

## The pan/tilt camera mount (not included)

CrowMotion does not ship a camera gimbal design, there are already dozens of great
2-servo pan/tilt mounts for FPV cameras. Pick one that fits your camera and SG90
servos:

- Thingiverse: https://www.thingiverse.com/search?q=fpv+pan+tilt+servo&type=things
- MakerWorld: https://makerworld.com/en/search/models?keyword=pan%20tilt%20servo

Search terms that work well: "FPV pan tilt", "2 servo camera gimbal",
"SG90 pan tilt".

## Hardware and wiring

- **ESP32-C3 Super Mini** (RISC-V, BLE 5.0)
- **MPU6500** IMU over I2C, address 0x68
- Powered over USB-C (a power bank is fine)

Four wires, all on one side of the board next to 3V3 and GND:

| MPU6500 | C3 Super Mini |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO4 (D2) |
| SCL | GPIO3 (D1) |
| AD0 | GND (sets address 0x68) |

## How it works (firmware)

- Built from scratch on Espressif ESP-IDF with the NimBLE Bluetooth host.
- 6DOF sensor fusion (gyro + accelerometer, no magnetometer); yaw drift is
  handled by gyro auto-calibration.
- Advertises the FrSky PARA service (`0xFFF0`) and streams channel data via
  notify on `0xFFF6`, with the connect nudge the radio expects.
- Head angles map to trainer channels with per-axis gain and a recenter.

Compatibility: FrSky radios with the **PARA wireless (Bluetooth) trainer**.
Developed against an **FrSky X20S (Tandem) on EthOS**.

## Build and flash

You need Espressif ESP-IDF (v5.5.x). Full setup, build, and flash steps for
macOS are in
[docs/BUILD.md](https://github.com/crowpilot-fc/crowmotion/blob/main/docs/BUILD.md).
Short version once ESP-IDF is installed:

```
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Using it

Once flashed, CrowMotion runs on its own. Power it over USB, put it on your
goggles, and connect your radio to it as a PARA wireless (Bluetooth) trainer.
It advertises as "CrowMotion". The onboard LED and a couple of tap gestures are
the whole physical interface, there are no buttons.

### Onboard status LED

The onboard LED (GPIO8) shows what the tracker is doing at a glance:

| LED pattern | Meaning |
|---|---|
| Slow heartbeat (one short blink every ~2 s) | Advertising, waiting for the radio |
| Solid on | Radio connected, streaming channels |
| Double-blink, repeating | WiFi config hotspot is active |
| Fast blink | Firmware update in progress |
| N blinks, pause, repeat | Fault code (2 blinks = IMU not found, check wiring) |

A short triple flash confirms an action, for example after a recenter.

### Tap gestures

Tap the tracker (or whatever it is mounted on) to control it, no app needed:

- **Double-tap:** recenter. Your current head position becomes the new center.
- **Quad-tap:** toggle the WiFi config hotspot. This only works when the radio
  is not connected, so you cannot open config by accident mid-flight.

CrowMotion also recenters itself automatically about half a second after boot,
once the sensor fusion has settled.

## Configuration (WiFi hotspot)

Every setting is configured from a phone or laptop over a local WiFi hotspot,
served by a self-contained web UI on the device. No app and no internet needed.

1. With the radio disconnected, **quad-tap** the tracker. The LED starts its
   double-blink pattern.
2. Join the WiFi network **`crowmotion-XXXX`** (XXXX is the last four hex digits
   of the device MAC). The password is **`crowmotion`**.
3. Open **`http://192.168.4.1/`** in a browser.

The web UI has these sections:

- **Live:** real-time 3D head view, live pan/tilt channel values, and a
  Recenter button.
- **Orientation:** tell it how the tracker is mounted. Use the rotate/flip
  buttons, or **Auto-detect**: hold the tracker in its worn position and it
  works out the mounting from gravity.
- **Channels:** assign pan and tilt to trainer channels TR1 to TR8, enable or
  disable each, and invert direction.
- **Response:** pan and tilt sensitivity (microseconds per degree), a deadband
  around center, and the output range (min / center / max microseconds) per axis.
- **Taps:** tap sensitivity for the double-tap and quad-tap gestures.
- **Firmware:** current version, home WiFi credentials (for update downloads),
  check for updates, and update from a local file.
- **Device:** device name, plus export, import, and reset of the whole config.
  "Exit hotspot" closes the hotspot and returns to the radio link.

Settings persist across reboots (stored in NVS on the device).

## Firmware updates (OTA)

You can update the firmware without a cable, from the same web UI:

- **From a file:** upload a `.bin` build straight from your phone or laptop.
- **Over the internet:** enter your home WiFi credentials once, then
  "Check for updates". The device connects out, compares against the published
  version, and installs a newer build if one is available.

## Enclosure

A printable case is in progress and will be added here later, as printable files
(STL) plus the source. For now, mount the board and IMU however you like; any
small project box or strap clip works while the design is finalized.

## Roadmap

This is the **v1** USB-powered build. Progress:

- [x] Bluetooth PARA trainer link (advertise, connect, stream channels)
- [x] MPU6500 IMU bring-up
- [x] 6DOF Madgwick fusion
- [x] Head-angle to channel mapping with per-axis sensitivity, deadband, range, and invert
- [x] Continuous auto-calibration and mounting-orientation handling (with auto-detect)
- [x] Recenter (double-tap and web UI button)
- [x] Onboard status LED with state patterns and fault blink-codes
- [x] Tap gestures (double-tap recenter, quad-tap config)
- [x] Settings persistence (NVS) and a WiFi configuration UI
- [x] Firmware updates (local upload and server check) over WiFi
- [x] Bench verification against a real radio (FrSky X20S / EthOS)
- [ ] Flight test and a printed, mounted build
- [ ] Printable enclosure (STL + source)

A battery-powered, fully standalone **v2** (onboard LiPo, deep sleep,
wake-on-motion) is planned after v1 is solid.

## Credits and lineage

CrowMotion is an original implementation, inspired by these excellent projects. No
source code from them is used. See
[NOTICE](https://github.com/crowpilot-fc/crowmotion/blob/main/NOTICE).

- [RC HeadTracker](https://github.com/dlktdr/HeadTracker) by Cliff Blackburn
  (dlktdr), GPL-3.0. Conceptual reference for the PARA protocol.
- [HeadTracker](https://github.com/ysoldak/HeadTracker) by Yury Soldak
  (ysoldak), public domain. Reference for minimalist auto-calibration.

## License

Apache License 2.0. See
[LICENSE](https://github.com/crowpilot-fc/crowmotion/blob/main/LICENSE).

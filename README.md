# FreeLook

A wireless FPV head tracker you can build for about **$5**.

![License](https://img.shields.io/badge/license-Apache--2.0-blue)
![Status](https://img.shields.io/badge/status-alpha-orange)
![Board](https://img.shields.io/badge/board-ESP32--C3-green)

Turn your head and the FPV camera follows. FreeLook reads your head motion and
streams it to your RC radio over Bluetooth as trainer channels, so a pan/tilt
camera on your aircraft mirrors where you look. Two cheap parts, a 3D printed
case, and a USB power bank.

> **Status: alpha.** The firmware (Bluetooth trainer link, IMU, sensor fusion,
> and channel mapping) is implemented and builds. End-to-end verification on a
> real radio is in progress, and it has not been flight-tested yet. Follow the
> roadmap below.

## Why

Commercial head trackers and IMU-equipped dev boards (XIAO nRF52840 Sense,
Arduino Nano 33 BLE Sense) cost many times more. FreeLook does the same job with
a $2 microcontroller and a $2 IMU. No proprietary dongle, no soldering iron
required beyond four wires.

## How the whole system works

```
   your head                 your radio                 your aircraft
  +-----------+   Bluetooth  +-----------+   RC link    +--------------+
  | FreeLook  | -----------> | FrSky     | -----------> | RX -> 2x     |
  | (tracker) |   PARA       | radio     |   channels   | servos -> cam|
  +-----------+   trainer    +-----------+              +--------------+
```

1. **FreeLook** sits on your goggles and measures head pan, tilt, and roll.
2. It sends those as **trainer channels** to your radio over the FrSky PARA
   Bluetooth wireless trainer link.
3. Your radio forwards them to the aircraft like any other channels.
4. On the aircraft, **two servos** pan and tilt the FPV camera to match.

FreeLook is the head-tracking part (step 1). You bring the radio, and you print
the servo camera mount (step 4) from an existing design, see below.

## Bill of materials

The head tracker itself:

| Part | What it does | Approx cost |
|---|---|---|
| ESP32-C3 Super Mini | microcontroller + Bluetooth | $2 - 3 |
| MPU6500 module | 6-axis IMU (accel + gyro) | $2 |
| 3D printed case | enclosure (in this repo) | filament |
| USB-C power | any power bank or USB port | reuse |
| **Head tracker total** | | **~$5** |

To complete the camera side on your aircraft:

| Part | What it does | Approx cost |
|---|---|---|
| 2x micro servo (SG90 / MG90S) | pan and tilt the camera | $2 - 4 |
| 3D printed pan/tilt frame | holds the camera and servos | filament |
| **Full system** | tracker + camera mount | **~$8 - 10** |

## The pan/tilt camera mount (not included)

FreeLook does not ship a camera gimbal design, there are already dozens of great
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
[docs/BUILD.md](https://github.com/pa-ra-kram/FreeLook/blob/main/docs/BUILD.md).
Short version once ESP-IDF is installed:

```
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Enclosure

A parametric, support-free, two-piece slide case lives at
[hardware/enclosure/freelook_case.scad](https://github.com/pa-ra-kram/FreeLook/blob/main/hardware/enclosure/freelook_case.scad).
Open it in OpenSCAD, render the `coupon` to dial in the slide fit, then the
`plate` for all parts. It clips onto a goggles strap.

## Roadmap

This is the **v1** USB-powered build. Progress:

- [x] Bluetooth PARA trainer link (advertise, connect, stream channels)
- [x] MPU6500 IMU bring-up
- [x] 6DOF Madgwick fusion
- [x] Head-angle to channel mapping
- [ ] On-radio end-to-end verification (in progress)
- [ ] Continuous auto-calibration and mounting-orientation handling
- [ ] Recenter (double-tap and command)
- [ ] Settings persistence and a printed, mounted build

A battery-powered, fully standalone **v2** (onboard LiPo, deep sleep,
wake-on-motion) is planned after v1 is solid.

## Credits and lineage

FreeLook is an original implementation, inspired by these excellent projects. No
source code from them is used. See
[NOTICE](https://github.com/pa-ra-kram/FreeLook/blob/main/NOTICE).

- [RC HeadTracker](https://github.com/dlktdr/HeadTracker) by Cliff Blackburn
  (dlktdr), GPL-3.0. Conceptual reference for the PARA protocol.
- [HeadTracker](https://github.com/ysoldak/HeadTracker) by Yury Soldak
  (ysoldak), public domain. Reference for minimalist auto-calibration.

## License

Apache License 2.0. See
[LICENSE](https://github.com/pa-ra-kram/FreeLook/blob/main/LICENSE).

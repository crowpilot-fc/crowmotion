# FreeLook

A DIY wireless FPV head tracker. FreeLook measures head pan, tilt, and roll and
streams them as trainer channels to an RC radio over the FrSky PARA Bluetooth
wireless trainer link, so a camera gimbal on the aircraft follows your head.

It is a small, self-contained, battery-powered module designed to ride on FPV
goggles (developed against the DJI Goggles 3) and be removable.

> Status: early work in progress. Milestone 1 (Bluetooth bring-up) is in
> progress. See the roadmap link below.

## Hardware

v1 is a deliberately minimal, two-component build:

- ESP32-C3 Super Mini (RISC-V, BLE 5.0). The Seeed XIAO ESP32-C3 also works
  unchanged, same chip and same GPIO numbers.
- MPU6500 IMU (6-axis accel + gyro), I2C, address 0x68
- Powered over USB-C from any source (a power bank, for example)

A battery-powered version (1S LiPo with deep sleep, wake-on-motion, an on/off
switch, and a status LED) is planned as a later phase. v1 stays at two
components, the MCU and the IMU, always on while plugged in.

### Wiring (MPU6500 to ESP32-C3)

| MPU6500 | ESP32-C3 (by GPIO) | XIAO pad |
|---|---|---|
| VCC | 3V3 | 3V3 |
| GND | GND | GND |
| SDA | GPIO10 | D10 |
| SCL | GPIO20 | D7 |
| AD0 | GND (sets address 0x68) | GND |

The Super Mini labels pins by GPIO number; the XIAO uses D-pad labels (shown for
reference). Note: the ESP32-C3 Super Mini has a weaker PCB antenna than the
XIAO, fine at short range but worth knowing if you see BLE dropouts.

Reference radio: FrSky X20S (Tandem) running EthOS, over the PARA Bluetooth
trainer link.

## How it works

- Fusion is 6DOF (gyro + accel), no magnetometer. Yaw drift is handled by
  continuous automatic gyro calibration.
- The PARA link advertises BLE GATT service 0xFFF0 and delivers channel data to
  the radio via notify on characteristic 0xFFF6. The radio is nudged into
  trainer-receive mode with a short "\r\n" on connect.
- Recenter is planned via double-tap (accelerometer jerk detection) and a BLE
  command.

## Software

FreeLook is built from scratch on Espressif ESP-IDF using the NimBLE host stack.
It is an original implementation, not a fork. See Credits below for the projects
that inspired it.

## Build and flash

See [docs/BUILD.md](docs/BUILD.md) for the full ESP-IDF setup, build, and flash
workflow on macOS. Short version, once ESP-IDF is installed and sourced:

```
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

## Roadmap

The living plan, design decisions, and change log are maintained in Notion (this
is a personal project where Notion is the source of truth for non-code docs):
https://app.notion.com/p/38df4e695ae3818385c4ed0a3500c547

The repository holds code plus this README, LICENSE, NOTICE, and build
instructions. Milestones in brief:

v1 (USB-powered, two components):

1. Bluetooth bring-up: advertise PARA, connect to the X20S, stream centered
   channels. (de-risk the radio link first)
2. MPU6500 bring-up and raw reads over I2C.
3. 6DOF fusion (Madgwick) producing stable pan/tilt/roll.
4. Continuous gyro auto-calibration and board-rotation compensation.
5. Axis-to-channel mapping with per-axis gain, center, and limits.
6. Recenter: double-tap and BLE command.
7. Settings persistence (NVS) and configuration.
8. Enclosure and goggles mounting.

Deferred to a battery-powered v2: 1S LiPo, on/off switch, deep sleep with
wake-on-motion and auto-sleep, status LED, and battery sensing.

## Credits and lineage

FreeLook is an original implementation inspired by these excellent projects.
No source code from them is used. See [NOTICE](NOTICE) for details.

- [RC HeadTracker](https://github.com/dlktdr/HeadTracker) by Cliff Blackburn
  (dlktdr), licensed GPL-3.0. Conceptual reference for the PARA protocol and the
  overall feature set.
- [HeadTracker](https://github.com/ysoldak/HeadTracker) by Yury Soldak
  (ysoldak), public domain (Unlicense). Reference for minimalist gyro
  auto-calibration and PARA compatibility.

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

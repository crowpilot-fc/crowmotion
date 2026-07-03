# CrowMotion

A wireless FPV head tracker you can build for about **$5**.

[![Build](https://github.com/crowpilot-fc/crowmotion/actions/workflows/build.yml/badge.svg)](https://github.com/crowpilot-fc/crowmotion/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

> ## Guide, photos, downloads, and the browser flasher are at **[crowpilot.in/crowmotion](https://crowpilot.in/crowmotion/)**
>
> This repository holds the **source code**. If you want to build one, flash it,
> wire it, or print a case, start on the website. It has everything.

Turn your head and the FPV camera follows. CrowMotion reads your head motion with
an ESP32-C3 and an MPU6500 IMU and streams it to your RC radio over the FrSky
PARA Bluetooth wireless trainer link, so a pan/tilt camera on your aircraft
mirrors where you look. Two cheap parts, four wires.

**Work in progress:** the firmware works and is bench-verified against a real
radio (FrSky X20S / EthOS). It has not been flight-tested yet.

## Using it

Everything a builder needs lives on the website, not here:

- **Flash from your browser** (no toolchain): [crowpilot.in/crowmotion#flash](https://crowpilot.in/crowmotion/#flash)
- **Build guide, wiring, and photos:** [crowpilot.in/crowmotion#build](https://crowpilot.in/crowmotion/#build)
- **Configure and use it:** [crowpilot.in/crowmotion#configure](https://crowpilot.in/crowmotion/#configure)
- **Downloads** (firmware images, printable case): [Releases](https://github.com/crowpilot-fc/crowmotion/releases)

## Building from source (developers)

Built from scratch on Espressif ESP-IDF (v5.5.4) with the NimBLE Bluetooth host.

- **Hardware:** ESP32-C3 Super Mini + MPU6500 over I2C
  (SDA = GPIO4/D2, SCL = GPIO3/D1, AD0 to GND, VCC = 3V3).
- **Build and flash:** full setup in [docs/BUILD.md](docs/BUILD.md). Short version:

  ```
  idf.py set-target esp32c3
  idf.py build
  idf.py -p /dev/cu.usbmodemXXXX flash monitor
  ```

- **Repo layout:**
  - `main/` - tracker firmware (fusion, PARA BLE link, config web UI, OTA)
  - `bridge/` - CrowLink, the wireless trainer-jack bridge (ESP32-C6)
  - `components/crowlink/` - shared ESP-NOW wire protocol
  - `docs/` - build docs
  - `website/`, `flasher/` - the public site and browser flasher
- **Contributing:** [CONTRIBUTING.md](CONTRIBUTING.md). **Security policy:** [SECURITY.md](SECURITY.md).

## Credits and license

Original implementation, inspired by (but not derived from)
[RC HeadTracker](https://github.com/dlktdr/HeadTracker) by Cliff Blackburn
(GPL-3.0) and [HeadTracker](https://github.com/ysoldak/HeadTracker) by Yury
Soldak (public domain). No third-party source is reused. See [NOTICE](NOTICE).

Apache License 2.0. See [LICENSE](LICENSE).

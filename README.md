# CrowMotion

CrowMotion is an open-source, self-powered FPV head tracker that mounts on a
goggle strap. It sends eight trainer channels directly from one ESP32-C3:

- FrSky PARA over Bluetooth when the radio supports it
- CPPM/PPM through an onboard 3.5 mm trainer jack for other radios
- Auto mode switches to PPM when a cable is inserted and otherwise uses BLE

[![Build](https://github.com/crowpilot-fc/crowmotion/actions/workflows/build.yml/badge.svg)](https://github.com/crowpilot-fc/crowmotion/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

## Release status

The earlier BLE prototype worked with a real radio. The integrated battery,
MPU6050, jack-detect, and local PPM design is now at development version 0.2.0.
It compiles, but it must pass hardware bring-up, trainer-port electrical tests,
runtime tests, and flight testing before a stable release. Do not use it for a
safety-critical model until those gates are complete.

## Locked reference hardware

- ESP32-C3 Super Mini
- MPU6050 breakout, I2C address 0x68
- LX-LCBST USB-C charger/5 V boost module
- 601230 350 mAh protected 1S LiPo
- Two-position SPST slide power switch in the boost module's 5 V output
- Switched 3.5 mm TRS female jack with an isolated detect contact
- 2.2 kohm series resistor from GPIO6 to jack tip
- 100 kohm pulldown from jack tip to ground
- Low-capacitance 5 V ESD protection at the jack

The switch placement lets the battery charge while CrowMotion is off. Verify
the exact LX-LCBST charger IC and set charge current to 150-175 mA unless the
cell datasheet explicitly permits more. Do not charge and operate at the same
time until proper power-path behavior is confirmed.

## Pin assignment

| Function | ESP32-C3 GPIO |
| --- | ---: |
| MPU6050 SDA | 4 |
| MPU6050 SCL | 3 |
| PPM output | 6 |
| Jack detect, active low | 7 |
| Onboard status LED | 8 |

Jack tip is signal, sleeve is ground, and ring is not connected. The PPM
polarity is configurable. GPIO2 is intentionally unused because it is a boot
strapping pin.

## Controls

- Double-tap: recenter pan and tilt
- Four taps: enter or exit configuration mode
- Slider switch: hard power on or off

Configuration includes channel assignment, gain, limits, axis orientation,
output mode, PPM polarity, device name, WiFi, and OTA update settings.

## Build

CrowMotion uses ESP-IDF 5.5.4 and targets only the ESP32-C3 Super Mini.

```sh
. "$IDF_PATH/export.sh"
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

The application binary is generated at `build/crowmotion.bin`. Tagged releases
are signed, attached to GitHub Releases, and staged for the browser flasher and
OTA service. See [docs/BUILD.md](docs/BUILD.md) for toolchain details.

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

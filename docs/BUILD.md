# Building and flashing CrowMotion (macOS)

CrowMotion targets the ESP32-C3 and is built with Espressif ESP-IDF and the
NimBLE Bluetooth host. v1 supports exactly one board: the ESP32-C3 Super Mini.
This guide covers a macOS dev machine.

## 1. Install ESP-IDF

Use a stable ESP-IDF release (v5.5.x is known good for CrowMotion). The
recommended install path:

```
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c3
```

Note: ESP-IDF builds its Python virtualenv against the `python3` it finds at
install time. If you later upgrade Python (for example via Homebrew) and
`export.sh` complains that the virtualenv is missing, just re-run
`./install.sh esp32c3` to rebuild it. The toolchains are not re-downloaded.

This installs the RISC-V toolchain and Python environment for the esp32c3
target. You only do this once.

## 2. Activate the environment

In every new shell where you want to build, source the export script:

```
. ~/esp/esp-idf/export.sh
```

After this, `idf.py` is on your PATH. Confirm with `idf.py --version`.

## 3. Build

From the repository root:

```
idf.py set-target esp32c3
idf.py build
```

`set-target` is needed once (it generates sdkconfig from sdkconfig.defaults and
the chip target). Re-run it only if you change targets.

Targets: v1 builds for `esp32c3` only, and the only supported board is the
ESP32-C3 Super Mini (it is the default and sole option under "CrowMotion
Configuration -> Target board" in menuconfig).

## 4. Flash and monitor

The ESP32-C3 boards expose a native USB serial/JTAG over their USB-C port, so no
external programmer is needed. Plug it in and find the port:

```
ls /dev/cu.usbmodem*
```

Then flash and open the serial monitor:

```
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Replace `usbmodemXXXX` with the actual device. Press Ctrl-] to exit the monitor.

If the board does not enter download mode automatically, hold the BOOT button
while plugging in (or while pressing RESET), then run flash again.

## 5. What you should see

On boot the monitor prints the CrowMotion banner, brings up NimBLE, and advertises
as "CrowMotion" (PARA services 0xFFF0 and 0xFFFA), streaming 8 centered trainer
channels. If the MPU6500 is wired, it is detected and the fusion task estimates
the gyro bias and then centers; if not, it logs a wiring hint and keeps the PARA
link running. Connect the FrSky X20S as a wireless trainer to receive the channels.

```
crowmotion: CrowMotion starting (ESP32-C3 Super Mini)
para: Advertising as "CrowMotion" (services 0xFFF0, 0xFFFA)
para: PARA link up: 8 channels at 1500 us, advertising as "CrowMotion"
mpu6500: MPU6500 detected (WHO_AM_I = 0x70)
mpu6500: MPU6500 configured: +/-2g, +/-2000dps, DLPF 41Hz, 200Hz ODR
fusion: startup gyro bias [dps]  0.00  0.05 -0.02
fusion: centered; head motion now drives channels
crowmotion: CrowMotion up. Waiting for radio (X20S) to connect.
```

If the IMU is not wired, you instead see a warning from `crowmotion` and the
LED blinks a two-blink fault code, while the PARA link keeps advertising.

## Notes

- `sdkconfig` is generated and gitignored. Change configuration with
  `idf.py menuconfig`; persist intended defaults in `sdkconfig.defaults`.
- Partition layout is in `partitions.csv` (includes an NVS region reserved for
  head-tracker settings used from Milestone 7 onward).

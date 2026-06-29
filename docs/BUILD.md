# Building and flashing FreeLook (macOS)

FreeLook targets the ESP32-C3 and is built with Espressif ESP-IDF and the
NimBLE Bluetooth host. The reference board is the ESP32-C3 Super Mini; the
Seeed Studio XIAO ESP32-C3 also works unchanged (same chip, same GPIO numbers).
This guide covers a macOS dev machine.

## 1. Install ESP-IDF

Use a stable ESP-IDF release (v5.5.x is known good for FreeLook). The
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

Targets: v1 builds for `esp32c3` (the C3 Super Mini / XIAO C3). The battery v2
boards add `esp32s3` (Seeed XIAO ESP32-S3) and `esp32h2` (ESP32-H2 Super Mini)
as further targets; the PARA, IMU, and fusion code is shared across all of them.

Board selection: the C3 Super Mini and the XIAO ESP32-C3 are both `esp32c3` but
have different one-side I2C pins, so pick the board in `idf.py menuconfig` under
"FreeLook Configuration -> Target board" (default: ESP32-C3 Super Mini). Changing
the board needs a rebuild.

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

On boot the monitor prints the FreeLook banner, brings up NimBLE, and advertises
as "FreeLook" (PARA service 0xFFF0), streaming 8 centered trainer channels. If
the MPU6500 is wired, it is detected and the fusion task logs yaw/pitch/roll;
if not, it logs a wiring hint and keeps the PARA link running. Connect the FrSky
X20S as a wireless trainer to receive the channels.

```
para: Advertising as "FreeLook" (service 0xFFF0)
para: PARA link up: 8 channels at 1500 us, advertising as "FreeLook"
mpu6500: MPU6500 detected (WHO_AM_I = 0x70)
fusion: yaw    0.0  pitch   -0.2  roll    0.1
```

## Notes

- `sdkconfig` is generated and gitignored. Change configuration with
  `idf.py menuconfig`; persist intended defaults in `sdkconfig.defaults`.
- Partition layout is in `partitions.csv` (includes an NVS region reserved for
  head-tracker settings used from Milestone 7 onward).

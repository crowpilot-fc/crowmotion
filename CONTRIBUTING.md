# Contributing to CrowMotion

Thanks for your interest. CrowMotion is a small, focused project: a $5
wireless FPV head tracker. Contributions that keep it small, cheap, and
reliable are very welcome.

## Getting set up

Build instructions (ESP-IDF v5.5.4, ESP32-C3) are in
[docs/BUILD.md](docs/BUILD.md). The only supported v1 hardware is the
ESP32-C3 Super Mini plus an MPU6050 module.

## Making changes

- Match the existing code style: plain C, 4-space indent, concise comments
  that explain why, not what.
- Every source file carries an `SPDX-License-Identifier: Apache-2.0` header;
  keep it in new files.
- Keep the firmware self-contained: no new external dependencies without a
  strong reason.
- `idf.py build` must pass. CI builds every PR.
- If a change affects flight-relevant behavior (fusion, mapping, the PARA
  link, taps), say in the PR whether you tested it on real hardware and
  against which radio.

## Reporting bugs

Open a GitHub issue using the bug template. Include your board, ESP-IDF
version, radio and firmware (e.g. X20S / EthOS 1.x), and serial log output
where possible.

## Scope notes

- Version 0.2 is deliberately focused on one board and one IMU, with PARA BLE
  and local trainer-jack PPM from the same self-powered unit.
- Open a discussion before adding other boards or output protocols.

## License

By contributing you agree that your contributions are licensed under the
Apache License 2.0, the same license as the project.

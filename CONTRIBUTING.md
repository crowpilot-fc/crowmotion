# Contributing to CrowMotion

Thanks for your interest. CrowMotion is a small, focused project: a $5
wireless FPV head tracker. Contributions that keep it small, cheap, and
reliable are very welcome.

## Getting set up

Build instructions (ESP-IDF v5.5.4, ESP32-C3) are in
[docs/BUILD.md](docs/BUILD.md). The only supported v1 hardware is the
ESP32-C3 Super Mini plus an MPU6500 module.

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

- v1 is deliberately minimal: one board, one IMU, PARA BLE trainer output.
- Feature ideas beyond that (other boards, other output protocols, battery
  operation) are v2 territory; feel free to open a discussion first so we
  can align before you write code.

## License

By contributing you agree that your contributions are licensed under the
Apache License 2.0, the same license as the project.

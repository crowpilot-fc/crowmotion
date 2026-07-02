# Security Policy

## Reporting a vulnerability

Please report security issues privately to **me@ni-t.in** rather than opening
a public issue. You should get a reply within a few days. Coordinated
disclosure is appreciated; a fix will be prioritized and credited unless you
prefer otherwise.

## Scope and threat model

CrowMotion is a hobbyist head tracker, not a hardened product. Current stance:

- The config web UI and OTA endpoints are only served to clients on the
  device's own WiFi hotspot (WPA2, per-device SSID, password changeable in
  the UI). They are intentionally not reachable from the home network the
  device may join for update downloads.
- OTA images are not cryptographically signed and secure boot is not
  enabled. Anyone who can join the hotspot can flash the device. Mitigations:
  the hotspot only runs when explicitly enabled (quad-tap, radio
  disconnected), and bootloader rollback restores the previous firmware if a
  new image fails to boot.
- The BLE PARA trainer link is unencrypted and unauthenticated by protocol
  design (FrSky PARA); anyone in radio range could pose as a trainer source.
  This matches every other PARA implementation.

Reports about weaknesses within this stated model (e.g. bypassing the
hotspot-only restriction, crashing the device with crafted HTTP/BLE input)
are very much in scope.

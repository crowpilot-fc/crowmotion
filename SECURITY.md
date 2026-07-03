# Security Policy

## Reporting a vulnerability

Please report security issues privately to **me@ni-t.in** rather than opening
a public issue. You should get a reply within a few days. Coordinated
disclosure is appreciated; a fix will be prioritized and credited unless you
prefer otherwise.

## Scope and threat model

CrowMotion is a hobbyist head tracker, not a hardened product. Current stance:

- The config web UI runs only in config mode (opened by a physical quad-tap,
  radio disconnected) and only on the device's own local interfaces: its WPA2
  hotspot (per-device SSID, password changeable in the UI) and, if home WiFi is
  set, the local network, where it is reachable at `crowmotion-XXXX.local`.
  This is a deliberate convenience-over-isolation choice: config is reachable
  from the home LAN without a password. It is never internet-routable, only
  live while config mode is open, and state-changing requests require a
  same-origin `Origin` (a CSRF guard that stops a malicious website from
  reconfiguring or reflashing the tracker through your browser).
- OTA images are cryptographically signed (RSA-3072, verified before an update
  is accepted) as of firmware 0.1.3; secure boot is intentionally not enabled,
  so a board is always recoverable by USB and physical reflash still accepts
  unsigned images by design. Bootloader rollback restores the previous firmware
  if a new image fails to boot.
- The BLE PARA trainer link is unencrypted and unauthenticated by protocol
  design (FrSky PARA); anyone in radio range could pose as a trainer source.
  This matches every other PARA implementation.

Reports about weaknesses within this stated model (e.g. bypassing the
hotspot-only restriction, crashing the device with crafted HTTP/BLE input)
are very much in scope.

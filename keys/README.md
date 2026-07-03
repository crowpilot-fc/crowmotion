# Keys

`crowmotion_ota_pub_rsa.pem` is the **public** RSA-3072 OTA signing key, kept
here for reference and independent verification. It is NOT used by the build
(a Secure Boot V2 signed app carries the public key in its own signature
block); it is safe to publish.

The **private** signing key is never in this repo. It lives in the maintainer's
password manager / `~/crowmotion-keys/` and in the GitHub Actions secret
`OTA_SIGNING_KEY_B64`. The release workflow decodes it, signs the tracker app at
build time (RSA-3072, Secure Boot V2 scheme, no hardware secure boot), verifies
the signature, then discards the key. Losing the private key does not brick
devices (no secure boot): it only stops OTA until a new-key build is flashed
over USB. RSA-3072 is the one app-signing scheme common to C3/S3/C6/H2.

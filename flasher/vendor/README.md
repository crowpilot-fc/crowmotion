# Vendored dependencies

`esp-web-tools/` is the `dist/` output of [esp-web-tools](https://github.com/esphome/esp-web-tools)
version **10.2.1**, vendored so the browser flasher loads no third-party
CDN code (supply-chain hardening). Update deliberately:

    npm pack esp-web-tools@<version>
    # extract package/dist -> esp-web-tools/, drop *.d.ts

Then bump the version noted here and re-test a real flash.

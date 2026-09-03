# SPDX-License-Identifier: Apache-2.0
"""Fast checks for release-critical hardware and PPM invariants."""

import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).parents[1]


def define(path: str, name: str) -> int:
    text = (ROOT / path).read_text(encoding="utf-8")
    match = re.search(rf"^#define\s+{name}\s+(\d+)", text, re.MULTILINE)
    if not match:
        raise AssertionError(f"missing numeric define {name} in {path}")
    return int(match.group(1))


class ReleaseContractTests(unittest.TestCase):
    def test_locked_gpio_assignments_are_unique_and_avoid_gpio2(self) -> None:
        names = (
            "CROWMOTION_I2C_SDA_GPIO",
            "CROWMOTION_I2C_SCL_GPIO",
            "CROWMOTION_LED_GPIO",
            "CROWMOTION_PPM_GPIO",
            "CROWMOTION_JACK_DETECT_GPIO",
        )
        pins = [define("main/board.h", name) for name in names]
        self.assertEqual(len(pins), len(set(pins)))
        self.assertNotIn(2, pins)

    def test_worst_case_ppm_frame_has_a_valid_sync_gap(self) -> None:
        count = define("main/channels/channel_state.h", "CROWMOTION_CHANNEL_COUNT")
        maximum = define("main/channels/channel_state.h", "CROWMOTION_CHANNEL_MAX_US")
        frame = define("main/output/ppm_output.h", "CROWMOTION_PPM_FRAME_US")
        pulse = define("main/output/ppm_output.h", "CROWMOTION_PPM_PULSE_US")
        sync_idle = frame - count * maximum - pulse
        self.assertGreaterEqual(sync_idle, 3000)

    def test_output_defaults_to_auto_positive_ppm(self) -> None:
        source = (ROOT / "main/config/config.c").read_text(encoding="utf-8")
        self.assertIn("c->output_mode = CROWMOTION_OUTPUT_MODE_AUTO;", source)
        self.assertIn("c->ppm_inverted = 0;", source)

    def test_ppm_reconnects_rmt_after_high_impedance_disable(self) -> None:
        source = (ROOT / "main/output/ppm_output.c").read_text(encoding="utf-8")
        self.assertIn("rmt_tx_switch_gpio(s_channel, s_gpio, false)", source)
        self.assertIn("gpio_output_disable(s_gpio)", source)
        self.assertNotIn("gpio_reset_pin", source)

    def test_ble_host_readiness_does_not_depend_on_advertising(self) -> None:
        source = (ROOT / "main/para/para_ble.c").read_text(encoding="utf-8")
        sync = source[source.index("static void on_sync(void)"):
                      source.index("static void on_reset(int reason)")]
        self.assertLess(sync.index("s_host_ready = true;"),
                        sync.index("para_advertise();"))

    def test_channel_state_has_a_freshness_fail_safe(self) -> None:
        stale_us = define("main/channels/channel_state.h",
                          "CROWMOTION_CHANNEL_STALE_US")
        self.assertLessEqual(stale_us, 250000)
        source = (ROOT / "main/channels/channel_state.c").read_text(encoding="utf-8")
        self.assertIn("s_last_update_us == 0", source)

    def test_mpu6050_identity_check_is_strict(self) -> None:
        source = (ROOT / "main/imu/mpu6050.c").read_text(encoding="utf-8")
        self.assertIn("#define WHO_AM_I_MPU6050 0x68", source)
        self.assertNotRegex(source, r"who\s*==\s*0x(?:70|72|98)")

    def test_prereleases_cannot_update_production_ota_metadata(self) -> None:
        workflow = (ROOT / ".github/workflows/release-ota.yml").read_text(
            encoding="utf-8"
        )
        self.assertGreaterEqual(
            workflow.count("github.event.release.prerelease == false"), 2
        )
        self.assertIn("latest-esp32c3.json", workflow)
        self.assertNotIn("site/crowmotion/latest.json", workflow)


if __name__ == "__main__":
    unittest.main()

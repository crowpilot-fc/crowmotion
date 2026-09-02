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


if __name__ == "__main__":
    unittest.main()

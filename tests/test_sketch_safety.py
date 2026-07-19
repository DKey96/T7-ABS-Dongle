import re
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

EURO4 = (SRC / "abs_dongele_sw_tenere_EURO_4_monochromatic.ino").read_text()
EURO5_MONO = (SRC / "euro5_mono.ino").read_text()
EURO5_LCD = (SRC / "abs_dongle_sw_tenere_EURO_5_lcd.ino").read_text()
SKETCH_PATHS = (
    SRC / "abs_dongele_sw_tenere_EURO_4_monochromatic.ino",
    SRC / "euro5_mono.ino",
    SRC / "abs_dongle_sw_tenere_EURO_5_lcd.ino",
)


class SketchSafetyTests(unittest.TestCase):
    def test_lcd_payloads_initialize_all_seven_transmitted_bytes(self):
        self.assertIn("const byte dataLength = 7;", EURO5_LCD)
        for name in ("absOnMsg", "rearAbsOffMsg", "absOffMsg"):
            match = re.search(
                rf"const byte {name}\[dataLength\].*?=\s*\{{([^}}]+)\}};",
                EURO5_LCD,
            )
            self.assertIsNotNone(match, name)
            self.assertEqual(7, len(match.group(1).split(",")), name)
        self.assertIn("byte canMsg[dataLength];", EURO5_LCD)
        self.assertIn("CAN.sendMsgBuf(canId, 0, length, dataToSend)", EURO5_LCD)
        self.assertIn("memcpy_P(canMsg, absOnMsg, dataLength)", EURO5_LCD)
        self.assertIn("sendAbsCanMessage(canMsg, dataLength)", EURO5_LCD)

    def test_protocol_bytes_are_not_cast_to_enum_ordinals(self):
        for sketch in (EURO4, EURO5_MONO, EURO5_LCD):
            self.assertNotIn("(AbsState)currentByte", sketch)

    def test_abs_frames_are_length_checked_before_state_byte_access(self):
        self.assertIn(
            "rxId == absId && hasAbsStateByte(len) && rxBuf[5] == absInit",
            EURO4,
        )
        self.assertIn("if (rxId == absId && hasAbsStateByte(len))", EURO4)

        self.assertIn(
            "rxId == absId && hasAbsStateByte(len) && rxBuf[5] == 0x30",
            EURO5_MONO,
        )
        self.assertIn(
            "rxId == absId && hasAbsStateByte(len) && rxBuf[5] == absInit",
            EURO5_MONO,
        )
        self.assertIn("if (rxId == absId && hasAbsStateByte(len))", EURO5_MONO)

        self.assertIn("if (rxId == absId && hasAbsStateByte(len))", EURO5_LCD)
        self.assertIn("else if (rxId == absButton && len >= 1)", EURO5_LCD)

    def test_mono_startup_wait_is_bounded_and_not_duplicated(self):
        self.assertIn("initializationTimeoutMs", EURO5_MONO)
        self.assertNotIn("while (true)", EURO5_MONO)
        restore_body = EURO5_MONO.split("void restoreLastSavedState()", 1)[1].split(
            "void updateEepromIfChanged", 1
        )[0]
        self.assertNotIn("while (", restore_body)

    def test_euro4_startup_wait_is_bounded(self):
        self.assertIn("initializationTimeoutMs", EURO4)
        self.assertNotIn("while (true)", EURO4)

    def test_lcd_button_sequence_expires_after_a_message_gap(self):
        self.assertIn("buttonMessageTimeoutMs", EURO5_LCD)
        self.assertRegex(
            EURO5_LCD,
            r"millis\(\)\s*-\s*buttonLastReceived\s*>\s*buttonMessageTimeoutMs",
        )

    def test_only_initialized_saved_state_bytes_are_written(self):
        for sketch in (EURO4, EURO5_MONO, EURO5_LCD):
            self.assertIn("updateEepromIfChanged(rxBuf, sizeof(lastState))", sketch)

    def test_can_interrupt_pin_is_configured(self):
        for sketch in (EURO4, EURO5_MONO, EURO5_LCD):
            self.assertIn("pinMode(CAN_INT_PIN, INPUT);", sketch)

    def test_sketches_pass_host_cpp_syntax_check(self):
        compiler = shutil.which("clang++")
        if compiler is None:
            self.skipTest("clang++ is not installed")

        for sketch_path in SKETCH_PATHS:
            result = subprocess.run(
                [
                    compiler,
                    "-std=c++11",
                    "-fsyntax-only",
                    "-x",
                    "c++",
                    "-include",
                    str(ROOT / "tests/stubs/ArduinoCompat.h"),
                    "-I",
                    str(ROOT / "tests/stubs"),
                    str(sketch_path),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, result.returncode, result.stderr)


if __name__ == "__main__":
    unittest.main()

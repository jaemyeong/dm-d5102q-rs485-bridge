from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import upgrade_usb_bootstrap as upgrade


class UpgradeGuardTest(unittest.TestCase):
    def test_pinned_candidate(self):
        upgrade.validate_bundle()
        with self.assertRaisesRegex(RuntimeError, "Source drifted"):
            upgrade.preflight()  # Old uploader must not accept the new B1 checkout.

    def test_current_image_gate(self):
        current = bytearray(b"\xff" * upgrade.base.FLASH_SIZE)
        for offset, path, size, _ in upgrade.base.SEGMENTS:
            current[offset:offset + size] = path.read_bytes()
        upgrade.validate_current(current)
        for offset in (0x1000, 0x8000, 0x10000):
            current[offset] ^= 1
            with self.assertRaises(RuntimeError):
                upgrade.validate_current(current)
            current[offset] ^= 1
        with self.assertRaises(RuntimeError):
            upgrade.validate_current(current[:-1])

    def boot(self, station=False):
        state = upgrade.new_boot_state()
        lines = [b"USB_BOOTSTRAP_0.1.2 RS485_DISABLED OTA_UNAVAILABLE",
                 b"INSTALL_KEY_REUSED", b"BOOT_RESET_NOT_ARMED"]
        lines += [b"STA_CONNECT_ATTEMPT", b"STA_IP=192.168.1.3",
                  b"MDNS_URL=http://dm-bridge-8810a1.local"] if station else [
                      b"AP_SSID=DM-BRIDGE-8810A1", b"AP_URL=http://192.168.4.1"]
        for line in lines:
            upgrade.boot_metadata(line, state)
        return state

    def test_both_normal_boot_modes_and_prior_warning(self):
        for station in (False, True):
            state = self.boot(station)
            self.assertTrue(upgrade.boot_accepted(state))
            upgrade.boot_metadata(b"esp_core_dump_flash: No core dump partition found!", state)
            self.assertTrue(upgrade.boot_accepted(state))
            self.assertEqual(state["coreDumpDiagnostics"], 1)

    def test_fail_closed_and_redact(self):
        for line in (b"INSTALL_KEY_ONCE=synthetic-secret", b"BOOT_RESET_ARMED HOLD_3S",
                     b"WIFI_RESET_COMPLETE INSTALL_KEY_PRESERVED", b"CONFIG_CORRUPT",
                     b"STA_IP=0.0.0.0", b"STA_IP=not-an-address",
                     b"USB_BOOTSTRAP_0.1.2 RS485_DISABLED OTA_UNAVAILABLE"):
            state = self.boot()
            safe = upgrade.boot_metadata(line, state)
            self.assertNotIn("synthetic-secret", safe)
            self.assertFalse(upgrade.boot_accepted(state))
        state = self.boot()
        safe = upgrade.boot_metadata(b"unknown possible-secret", state)
        self.assertNotIn("possible-secret", safe)
        state["keyReused"] = False
        self.assertFalse(upgrade.boot_accepted(state))


if __name__ == "__main__":
    unittest.main()

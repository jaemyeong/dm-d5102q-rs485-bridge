from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import upgrade_signed_ota as upgrade


class B1UpgradeGuardTest(unittest.TestCase):
    def test_exact_bundle_and_current_image(self):
        with self.assertRaisesRegex(RuntimeError, "Source drifted"):
            upgrade.preflight()  # Sealed 0.2.0 USB uploader cannot deploy this new checkout.
        current = bytearray(b"\xff" * upgrade.base.FLASH_SIZE)
        for offset, path, size, _ in upgrade.legacy.SEGMENTS:
            current[offset:offset + size] = path.read_bytes()
        upgrade.validate_current(current)
        for offset in (0x1000, 0x8000, 0x10000):
            current[offset] ^= 1
            with self.assertRaises(RuntimeError): upgrade.validate_current(current)
            current[offset] ^= 1

    def boot(self):
        state = upgrade.legacy.new_boot_state()
        for line in (b"BOOT_BUILD=usb-bootstrap-0.2.0", b"RS485_DISABLED SIGNED_OTA_BASELINE",
                     b"INSTALL_KEY_REUSED", b"BOOT_RESET_NOT_ARMED", b"OTA_KEY_CONFIGURED",
                     b"OTA_BOOT_STATE=USB_BASELINE", b"STA_CONNECT_ATTEMPT", b"STA_IP=192.168.1.3",
                     b"MDNS_URL=http://dm-bridge-8810a1.local"):
            upgrade.boot_metadata(line, state)
        return state

    def test_boot_gate_and_private_redaction(self):
        self.assertTrue(upgrade.boot_accepted(self.boot()))
        for line in (b"INSTALL_KEY_ONCE=test-secret", b"OTA_KEY_MISSING", b"BOOT_RESET_ARMED HOLD_3S",
                     b"OTA_BOOT_STATE=PENDING_VERIFY", b"BOOT_BUILD=usb-bootstrap-0.2.0"):
            state = self.boot()
            self.assertNotIn("test-secret", upgrade.boot_metadata(line, state))
            self.assertFalse(upgrade.boot_accepted(state))
        state = self.boot(); state["staIp"] = None
        self.assertFalse(upgrade.boot_accepted(state))


if __name__ == "__main__": unittest.main()

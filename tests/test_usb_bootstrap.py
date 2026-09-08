from __future__ import annotations

import configparser
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from validate_usb_bootstrap import AMENDMENT_PATH, WEB_AMENDMENT_PATH, BOOT_AMENDMENT_PATH, SOURCE_DIR, validate_usb_bootstrap
from validate_memory_budget import BudgetValidationError, validate_platformio_config


class UsbBootstrapTest(unittest.TestCase):
    def test_approved_source_boundary(self):
        validate_usb_bootstrap()

    def test_amendment_cannot_be_rewritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / AMENDMENT_PATH).parent.mkdir(parents=True)
            shutil.copyfile(ROOT / AMENDMENT_PATH, root / AMENDMENT_PATH)
            with (root / AMENDMENT_PATH).open("a") as handle:
                handle.write("\nAll hardware gates passed.\n")
            with self.assertRaisesRegex(ValueError, "amendment"):
                validate_usb_bootstrap(root)

    def test_gpio_cannot_be_added_to_bootstrap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / AMENDMENT_PATH).parent.mkdir(parents=True)
            shutil.copyfile(ROOT / AMENDMENT_PATH, root / AMENDMENT_PATH)
            shutil.copyfile(ROOT / WEB_AMENDMENT_PATH, root / WEB_AMENDMENT_PATH)
            shutil.copyfile(ROOT / BOOT_AMENDMENT_PATH, root / BOOT_AMENDMENT_PATH)
            shutil.copytree(ROOT / SOURCE_DIR, root / SOURCE_DIR)
            with (root / SOURCE_DIR / "main.cpp").open("a") as handle:
                handle.write("\nvoid bad() { pinMode(26, 1); }\n")
            with self.assertRaisesRegex(ValueError, "no-GPIO"):
                validate_usb_bootstrap(root)

    def test_source_cannot_switch_to_arbitrary_firmware(self):
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(ROOT / "platformio.ini")
        parser["platformio"]["src_dir"] = "unapproved"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "platformio.ini"
            with path.open("w") as handle:
                parser.write(handle)
            with self.assertRaisesRegex(BudgetValidationError, "approved USB"):
                validate_platformio_config(path)

    def test_cpp_core_with_sanitizers(self):
        self.run_cpp("test_bootstrap.cpp")

    def test_signed_ota_core_and_health_with_sanitizers(self):
        self.run_cpp("test_ota.cpp")

    def test_cpp_http_and_runtime_with_sanitizers(self):
        self.run_cpp("test_bootstrap_transport.cpp")

    def test_embedded_javascript_syntax(self):
        page = (ROOT / SOURCE_DIR / "web_ui.h").read_text()
        for name, delimiter in [("web_sha256.inc", "SHA256"), ("web_client.inc", "CLIENT")]:
            literal = (ROOT / SOURCE_DIR / name).read_text()
            script = literal.split(f'R"{delimiter}(', 1)[1].rsplit(f'){delimiter}"', 1)[0]
            page = page.replace(f'#include "{name}"', script)
        javascript = re.search(r"<script>(.*?)</script>", page, re.S).group(1)
        javascript = javascript.replace(')HTML"', '').replace('R"HTML(', '')
        result = subprocess.run(["node", "--check"], input=javascript,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def run_cpp(self, source):
        compiler = shutil.which("clang++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "A C++ compiler is required for bootstrap tests")
        with tempfile.TemporaryDirectory(prefix="dmbridge-host-") as directory:
            executable = str(Path(directory) / "test-bootstrap")
            command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-DDM_HOST_TEST",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                       "-I", str(ROOT / SOURCE_DIR), "-I", str(ROOT / "tests/host/fakes"),
                       str(ROOT / SOURCE_DIR / "core.cpp"),
                       str(ROOT / SOURCE_DIR / "ota_core.cpp"),
                       str(ROOT / "tests/host" / source), "-o", executable]
            for vendor in ("monocypher.c", "monocypher-ed25519.c"):
                obj = str(Path(directory) / (vendor + ".o"))
                build = subprocess.run([shutil.which("clang") or "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                                        "-fsanitize=address,undefined", "-c", str(ROOT / SOURCE_DIR / "vendor/monocypher" / vendor),
                                        "-o", obj], capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stderr)
                command.append(obj)
            if sys.platform != "darwin":
                command.append("-lcrypto")
            build = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([executable], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("assertions passed", result.stdout)
            print(result.stdout.strip())
            if source == "test_bootstrap_transport.cpp":
                self.check_curl_interoperability(executable)
                self.check_browser_client(executable)

    def test_web_amendment_and_vendor_are_pinned(self):
        for target in (WEB_AMENDMENT_PATH, BOOT_AMENDMENT_PATH, f"{SOURCE_DIR}/web_sha256.inc", f"{SOURCE_DIR}/boot_button.h"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / AMENDMENT_PATH).parent.mkdir(parents=True)
                shutil.copyfile(ROOT / AMENDMENT_PATH, root / AMENDMENT_PATH)
                shutil.copyfile(ROOT / WEB_AMENDMENT_PATH, root / WEB_AMENDMENT_PATH)
                shutil.copyfile(ROOT / BOOT_AMENDMENT_PATH, root / BOOT_AMENDMENT_PATH)
                shutil.copytree(ROOT / SOURCE_DIR, root / SOURCE_DIR)
                with (root / target).open("a") as handle:
                    handle.write("\nUnapproved modification\n")
                with self.assertRaisesRegex(ValueError, "amendment|dependency|no-GPIO"):
                    validate_usb_bootstrap(root)

    def check_browser_client(self, executable):
        server = subprocess.Popen([executable, "--curl-server"], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        try:
            fixture = server.stdout.readline().strip()
            result = subprocess.run(["node", str(ROOT / "tests/host/test_bootstrap_browser.js")],
                                    input=fixture, capture_output=True, text=True, timeout=12)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())
        finally:
            server.terminate()
            _, errors = server.communicate(timeout=3)
            self.assertNotIn("ERROR: AddressSanitizer", errors)

    def check_curl_interoperability(self, executable):
        server = subprocess.Popen([executable, "--curl-server"], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        try:
            port, key = server.stdout.readline().strip().split()
            base = ["curl", "--silent", "--show-error", "--fail-with-body", "--max-time", "3",
                    "--noproxy", "*", "--digest", "--user", f"installer:{key}",
                    "--header", "Host: 192.168.4.1"]
            result = subprocess.run(base + [f"http://127.0.0.1:{port}/api/v1/status"],
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            state = json.loads(result.stdout)
            self.assertTrue(state["canConfigure"])
            self.assertTrue(state["txBlocked"])
            self.assertTrue(state["otaSupported"])  # Host-only RFC public identity.
            self.assertEqual(state["otaBootState"], "USB_BASELINE")
            body = json.dumps({"ssid": "curl-test", "password": "curl-test-password", "configRevision": 0})
            result = subprocess.run(base + ["--request", "PUT", "--header", "Origin: http://192.168.4.1",
                "--header", f"X-CSRF-Token: {state['csrfToken']}", "--header", "Content-Type: application/json",
                "--data", body, f"http://127.0.0.1:{port}/api/v1/config"],
                capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
            self.assertTrue(json.loads(result.stdout)["rebootScheduled"])
            print("HTTP Digest SHA-256: independent curl status/save interoperability passed (loopback fakes)")
        finally:
            server.terminate()
            _, errors = server.communicate(timeout=3)
            self.assertNotIn("ERROR: AddressSanitizer", errors)


if __name__ == "__main__":
    unittest.main()

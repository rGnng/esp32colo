"""Host checks for image integrity, credentials and the real HTTP upload client."""
import base64
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ota_upload", ROOT / "scripts/ota_upload.py")
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


def application():
    header = bytearray(24)
    header[0], header[1], header[12], header[23] = 0xE9, 1, 9, 1
    segment = struct.pack("<I", 0xABCD5432) + bytes(252)
    image = header + struct.pack("<II", 0x3C000020, len(segment)) + segment
    checksum = 0xEF
    for value in segment:
        checksum ^= value
    image.extend(bytes((15 - len(image) % 16) % 16))
    image.append(checksum)
    image.extend(hashlib.sha256(image).digest())
    return bytes(image)


class ImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.file = Path(self.temp.name) / "firmware.bin"
        self.data = application()
        self.file.write_bytes(self.data)

    def test_valid_and_real_build(self):
        self.assertEqual(ota.validate_image(self.file), self.data)
        if ota.DEFAULT_IMAGE.exists():
            self.assertGreater(len(ota.validate_image(ota.DEFAULT_IMAGE)), 100000)

    def test_wrong_chip_bootloader_corruption_truncation_and_trailing_data(self):
        for position in (0, 12, 32, 40, len(self.data) - 33, len(self.data) - 1):
            with self.subTest(position=position):
                data = bytearray(self.data)
                data[position] ^= 1
                self.file.write_bytes(data)
                with self.assertRaises(ValueError):
                    ota.validate_image(self.file)
        for data in (b"", self.data[:30], self.data[:-50], self.data + b"junk"):
            self.file.write_bytes(data)
            with self.assertRaises(ValueError):
                ota.validate_image(self.file)

    def test_secrets_with_escaping_and_environment_override(self):
        file = Path(self.temp.name) / "secrets.h"
        password = 'space quote" slash\\ dollar$'
        file.write_text('#pragma once\n#define OTA_PASSWORD ' + json.dumps(password) + '\n')
        with patch.dict(ota.os.environ, {}, clear=True):
            self.assertEqual(ota.read_password(file), password)
        with patch.dict(ota.os.environ, {"COLOPLAY_OTA_PASSWORD": "override"}):
            self.assertEqual(ota.read_password(file), "override")
        with patch.dict(ota.os.environ, {"COLOPLAY_OTA_PASSWORD": ""}):
            with self.assertRaises(ValueError):
                ota.read_password(file)


class HttpTests(unittest.TestCase):
    def test_multipart_digest_auth_and_reboot_verification(self):
        data = application()
        md5 = hashlib.md5(data).hexdigest()
        captured = {}

        class Device(BaseHTTPRequestHandler):
            def do_POST(self):
                captured["path"] = self.path
                captured["auth"] = self.headers["Authorization"]
                captured["md5"] = self.headers["X-MD5"]
                boundary = self.headers["Content-Type"].split("boundary=")[1].encode()
                body = self.rfile.read(int(self.headers["Content-Length"]))
                captured["payload"] = body.split(b"\r\n\r\n", 1)[1].rsplit(b"\r\n--" + boundary, 1)[0]
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'{"updated":true,"rebooting":true}')

            def do_GET(self):
                self.send_response(200)
                self.end_headers()
                self.wfile.write(json.dumps({"sketch_md5": md5, "firmware": "test", "running_partition": "ota_app1"}).encode())

            def log_message(self, *_args):
                pass

        server = HTTPServer(("127.0.0.1", 0), Device)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            url = "http://127.0.0.1:%d" % server.server_port
            result = ota.http_upload(url, data, "test-password")
            self.assertEqual(captured["path"], "/update")
            self.assertEqual(captured["payload"], data)
            self.assertEqual(captured["md5"], md5)
            self.assertEqual(captured["auth"], "Basic " + base64.b64encode(b"admin:test-password").decode())
            self.assertEqual(ota.wait_for_image(result, md5, timeout=2)["firmware"], "test")
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_rejects_ambiguous_urls(self):
        for url in ("http://user:pass@host", "http://host/wrong", "http://host/?query=yes", "file:///tmp/x"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                ota.http_upload(url, application(), "password")


if __name__ == "__main__":
    unittest.main()

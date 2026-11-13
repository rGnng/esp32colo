#!/usr/bin/env python3
"""Application-only OTA: legacy ArduinoOTA on the LAN, or HTTP through a relay."""

import argparse
import ast
import base64
import hashlib
import http.client
import importlib.util
import json
import os
from pathlib import Path
import re
import secrets
import struct
import sys
import time
from urllib.parse import urlsplit, urlunsplit
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGE = ROOT / "firmware/.pio/build/coloplay/firmware.bin"
DEFAULT_SECRETS = ROOT / "firmware/include/secrets.h"
OTA_SLOT_SIZE = 0x480000


def read_password(path):
    override = os.environ.get("COLOPLAY_OTA_PASSWORD")
    if override is not None:
        password = override
    else:
        source = Path(path).read_text()
        match = re.search(r'^\s*#\s*define\s+OTA_PASSWORD\s+("(?:\\.|[^"\\])*")', source, re.M)
        if not match:
            raise ValueError("OTA_PASSWORD must be a quoted string in the secrets header")
        try:
            password = ast.literal_eval(match.group(1))
        except (SyntaxError, ValueError):
            raise ValueError("OTA_PASSWORD has an invalid string literal") from None
    if not isinstance(password, str) or not password:
        raise ValueError("OTA password must not be empty")
    return password


def validate_image(image):
    data = Path(image).read_bytes()
    if len(data) < 288 or len(data) > OTA_SLOT_SIZE:
        raise ValueError("application size is outside the 4.5 MiB OTA slot")
    if (data[0] != 0xE9 or not 1 <= data[1] <= 16 or
            struct.unpack_from("<H", data, 12)[0] != 9 or
            struct.unpack_from("<I", data, 32)[0] != 0xABCD5432):
        raise ValueError("expected an ESP32-S3 application, not a bootloader / merged image")
    offset, checksum = 24, 0xEF
    for _ in range(data[1]):
        if offset + 8 > len(data):
            raise ValueError("truncated segment header")
        _, size = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + size > len(data):
            raise ValueError("truncated application segment")
        for value in data[offset:offset + size]:
            checksum ^= value
        offset += size
    checksum_offset = (offset + 16) // 16 * 16 - 1
    if checksum_offset >= len(data) or data[checksum_offset] != checksum:
        raise ValueError("application image checksum mismatch")
    end = checksum_offset + 1
    if data[23]:
        if end + 32 != len(data) or hashlib.sha256(data[:end]).digest() != data[end:]:
            raise ValueError("application image SHA-256 mismatch")
    elif end != len(data):
        raise ValueError("unexpected data after application image")
    return data


def http_upload(target, data, password):
    url = urlsplit(target)
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password:
        raise ValueError("use an http(s)://host[:port] URL without embedded credentials")
    if url.path not in ("", "/", "/update") or url.query or url.fragment:
        raise ValueError("use the device root URL or /update")
    boundary = "coloplay-" + secrets.token_hex(16)
    prefix = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; "
              'filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()
    suffix = f"\r\n--{boundary}--\r\n".encode()
    connection_type = http.client.HTTPSConnection if url.scheme == "https" else http.client.HTTPConnection
    connection = connection_type(url.hostname, url.port, timeout=120)
    try:
        connection.putrequest("POST", "/update")
        credentials = base64.b64encode(("admin:" + password).encode()).decode()
        connection.putheader("Authorization", "Basic " + credentials)
        connection.putheader("Content-Type", "multipart/form-data; boundary=" + boundary)
        connection.putheader("Content-Length", str(len(prefix) + len(data) + len(suffix)))
        connection.putheader("X-MD5", hashlib.md5(data).hexdigest())
        connection.endheaders()
        connection.send(prefix)
        for offset in range(0, len(data), 16384):
            connection.send(data[offset:offset + 16384])
        connection.send(suffix)
        response = connection.getresponse()
        result = response.read(4096).decode(errors="replace")
        if response.status != 200:
            raise RuntimeError(f"HTTP OTA rejected ({response.status}): {result}")
        if not json.loads(result).get("updated"):
            raise RuntimeError("device did not confirm a verified upload")
    finally:
        connection.close()
    return urlunsplit((url.scheme, url.netloc, "/api/status", "", ""))


def arduino_upload(target, image, password, host_ip="0.0.0.0", host_port=32320):
    if ":" in target or "/" in target:
        raise ValueError("ArduinoOTA needs a device IPv4 address / hostname, or use an HTTP URL")
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
    packages = Path(os.environ.get("PLATFORMIO_PACKAGES_DIR", str(core / "packages")))
    uploader = packages / "framework-arduinoespressif32/tools/espota.py"
    if not uploader.is_file():
        raise ValueError("espota.py is missing; run scripts/build_firmware.sh first")
    spec = importlib.util.spec_from_file_location("coloplay_espota", uploader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # Importing keeps the password out of the OS process argument list.
    result = module.main(["-i", target, "-p", "3232", "-I", host_ip,
                          "-P", str(host_port), "-a", password, "-f", str(image)])
    if result:
        raise RuntimeError("ArduinoOTA failed; the uploader must be on the device LAN, "
                           "with inbound TCP port %d permitted" % host_port)
    return "http://" + target + "/api/status"


def wait_for_image(url, expected_md5, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with urlopen(url, timeout=3) as response:
                status = json.load(response)
            if status.get("sketch_md5") == expected_md5:
                print("Verified running image:", status.get("firmware"),
                      "on", status.get("running_partition"))
                return status
        except (OSError, ValueError):
            pass
        time.sleep(1)
    raise RuntimeError("upload finished, but the running image could not be verified; "
                       "check /api/status before retrying")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", help="LAN host/IP for ArduinoOTA, or HTTP URL for the web relay")
    parser.add_argument("image", nargs="?", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--secrets", type=Path, default=DEFAULT_SECRETS)
    parser.add_argument("--host-ip", default="0.0.0.0", help="local ArduinoOTA callback bind address")
    parser.add_argument("--host-port", type=int, default=32320, help="ArduinoOTA TCP callback port")
    args = parser.parse_args(argv)
    try:
        data = validate_image(args.image)
        password = read_password(args.secrets)
        md5 = hashlib.md5(data).hexdigest()
        print("Application:", args.image, "(%d bytes)" % len(data), flush=True)
        print("SHA-256:", hashlib.sha256(data).hexdigest(), flush=True)
        if args.target.startswith(("http://", "https://")):
            status_url = http_upload(args.target, data, password)
        else:
            status_url = arduino_upload(args.target, args.image, password, args.host_ip, args.host_port)
        print("Upload accepted; waiting for reboot…", flush=True)
        wait_for_image(status_url, md5)
    except (OSError, ValueError, RuntimeError, http.client.HTTPException) as error:
        print("error:", error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

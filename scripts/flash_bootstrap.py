#!/usr/bin/env python3
"""Flash only the ColoPLAY application, keeping the factory bootloader.

The factory bootloader lives at 0x0 and reads its partition table at 0x10000.
The Arduino application reads its table at 0x8000, so this script writes the
same factory table at 0x8000 and the application at 0x20000.  The factory
bootloader, its table at 0x10000, NVS, PHY data and OTA slots stay untouched.

Usage:
    .venv/bin/python scripts/flash_bootstrap.py [--timeout 600]
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial.tools.list_ports

ESP_VID = 0x303A
ROM_USB_JTAG_PID = 0x1001
APP_OFFSET = 0x20000
TABLE_OFFSET = 0x8000


def find_rom_port() -> str | None:
    for port in serial.tools.list_ports.comports():
        if port.vid == ESP_VID and port.pid == ROM_USB_JTAG_PID:
            return port.device
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    backups = sorted((root / "backups").glob("coloplay-factory-*.bin"))
    app = root / "firmware" / ".pio" / "build" / "coloplay" / "firmware.bin"
    table = root / "backups" / "factory-part-table-at-0x10000.bin"

    if not backups:
        print("error: no backups/coloplay-factory-*.bin found", file=sys.stderr)
        return 1
    backup = backups[-1]
    print(f"using factory backup: {backup}")
    if not app.exists():
        print(f"error: application not built: {app}", file=sys.stderr)
        print("run: cd firmware && pio run", file=sys.stderr)
        return 1

    # Extract the factory partition table (4 KiB) from the backup.
    table.write_bytes(backup.read_bytes()[0x10000:0x11000])

    print(f"waiting up to {args.timeout}s for ESP32-S3 ROM USB-JTAG (303A:1001)...")
    print("hold REC_KEY + the rotary knob, then plug in USB")
    deadline = time.monotonic() + args.timeout
    port = None
    while time.monotonic() < deadline:
        port = find_rom_port()
        if port:
            break
        time.sleep(0.2)
    if not port:
        print("error: timed out waiting for download mode", file=sys.stderr)
        return 1

    print(f"found {port}")
    print(f"flashing table at {TABLE_OFFSET:#x} and app at {APP_OFFSET:#x}")
    cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--port",
        port,
        "--before",
        "usb-reset",
        "--after",
        "hard-reset",
        "--baud",
        "921600",
        "write-flash",
        f"{TABLE_OFFSET:#x}",
        str(table),
        f"{APP_OFFSET:#x}",
        str(app),
    ]
    print(" ".join(cmd))
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    raise SystemExit(main())

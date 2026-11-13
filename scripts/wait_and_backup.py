#!/usr/bin/env python3
"""Wait for a ColoPLAY in ROM download mode, then read the full flash.

Run this before pressing the buttons; it polls for the ESP32-S3 USB
Serial/JTAG interface (VID 0x303A, PID 0x1001) and starts the backup as
soon as the device appears.  The serial port is kept open for the whole
read, so the buttons can be released once "connected" is printed.

Usage:
    .venv/bin/python scripts/wait_and_backup.py [--timeout 600]

Requires esptool >= 5.4 in the active environment.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import time
from datetime import datetime
from pathlib import Path

import serial.tools.list_ports
from esptool.cmds import attach_flash, connect_esp, detect_flash_size, run_stub
from esptool.loader import ESPLoader

ESP_VID = 0x303A
ROM_USB_JTAG_PID = 0x1001


def find_rom_port() -> str | None:
    for port in serial.tools.list_ports.comports():
        if port.vid == ESP_VID and port.pid == ROM_USB_JTAG_PID:
            return port.device
    return None


def progress(done: int, total: int, offset: int) -> None:
    percent = done * 100 // total if total else 0
    if percent % 5 == 0 and done != total:
        print(f"\r  reading: {percent:3d}% ({done / 1024 / 1024:.1f} MB)", end="", flush=True)


def main() -> int:
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=int, default=600, help="seconds to wait for the device")
    parser.add_argument("--baud", type=int, default=921600, help="operational baud rate")
    args = parser.parse_args()

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
        print("\nerror: timed out waiting for the ROM USB-JTAG interface", file=sys.stderr)
        return 1

    print(f"\nfound {port}")
    print("connecting (keep the buttons pressed until 'connected' appears)...")

    esp = connect_esp(
        port=port,
        chip="esp32s3",
        initial_baud=115200,
        before="no-reset",
        connect_attempts=3,
    )
    print("connected")
    esp = run_stub(esp)
    if args.baud != 115200:
        esp.change_baud(args.baud)
    attach_flash(esp)

    size = detect_flash_size(esp)
    if not size:
        print("error: could not detect flash size", file=sys.stderr)
        return 1
    print(f"detected flash size: {size / 1024 / 1024:.0f} MB")

    try:
        mac = esp.read_mac(0)
        print(f"MAC: {mac}")
    except Exception as exc:  # noqa: BLE001 - diagnostic only
        mac = None
        print(f"MAC: unavailable ({exc})")

    security = None
    try:
        security = esp.get_security_info()
        print(f"security: {security.get('parsed_flags', security)}")
    except Exception as exc:  # noqa: BLE001 - diagnostic only
        print(f"security: unavailable ({exc})")

    backups = Path(__file__).resolve().parent.parent / "backups"
    backups.mkdir(exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base = backups / f"coloplay-factory-{stamp}"
    image_path = base.with_suffix(".bin")

    print(f"reading {size} bytes to {image_path}")
    data = esp.read_flash(0, size, progress)
    print()
    image_path.write_bytes(data)

    digest = hashlib.sha256(data).hexdigest()
    (base.parent / f"{base.name}.bin.sha256").write_text(
        f"{digest}  {image_path.name}\n", encoding="utf-8"
    )
    (base.parent / f"{base.name}-meta.txt").write_text(
        "\n".join(
            [
                f"port: {port}",
                f"flash_size: {size}",
                f"mac: {mac}",
                f"security: {security}",
                f"sha256: {digest}",
                f"image: {image_path.name}",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    print(f"done: {image_path}")
    print(f"sha256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

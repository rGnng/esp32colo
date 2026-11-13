#!/usr/bin/env python3
"""Inspect a raw ESP32-S3 flash dump from a ColoPLAY.

Usage:
    python3 scripts/analyze_flash.py backups/coloplay-factory-XXXX.bin

The script prints the partition table, application descriptors, and a
keyword scan of printable strings that is useful for identifying the
display controller, audio codec, and GPIO assignments used by the
factory firmware.
"""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

PARTITION_MAGIC = 0x50AA
APP_MAGIC = 0xABCD5432
IMAGE_MAGIC = 0xE9

KEYWORDS = [
    # display controllers / panel drivers
    "gc9a01", "st7789", "st7735", "st7796", "ili9341", "ili9488",
    "ili9486", "nv3007", "nv3022", "axs15231", "spd2010", "ek9716",
    "esp_lcd", "lcd_panel", "panel_io", "backlight",
    # audio
    "es8311", "es7210", "es8388", "cs8120", "i2s", "codec", "mic",
    # GPIO / board names
    "LCD_CLK", "LCD_MOSI", "LCD_CS", "LCD_RS", "LCD_REST", "LCD_BLK",
    "LCD_TE", "AUD_MCLK", "AUD_BCLK", "AUD_WS", "AUD_SDIN", "AUD_SDOUT",
    "AUD_SCL", "AUD_SDA", "AUD_CTRL", "MICBIAS", "RGB_LED", "KEY1",
    "KEY2", "KEY3", "KEY4", "OPEN_KEY", "REC_KEY", "EKEY", "EAS", "EBS",
    "LIGHTADC", "VOL_ADC", "CHRG",
    # frameworks
    "arduino", "espidf", "esp-idf", "tinyusb", "colox", "coloplay",
    "lifesmart",
]


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def cstr(raw: bytes) -> str:
    return raw.split(b"\x00", 1)[0].decode("utf-8", "replace")


def parse_partitions(data: bytes):
    # ESP-IDF defaults to 0x8000, but some vendor images (including the
    # ColoPLAY factory firmware) move the table to 0x10000.
    for table_offset in (0x8000, 0x10000, 0x9000):
        if table_offset + 32 > len(data):
            continue
        magic = struct.unpack_from("<H", data, table_offset)[0]
        if magic != PARTITION_MAGIC:
            continue
        parts = []
        off = table_offset
        while off + 32 <= len(data):
            magic, ptype, subtype, poff, psize, label, flags = struct.unpack_from(
                "<HBBII16sI", data, off
            )
            if magic != PARTITION_MAGIC:
                break
            parts.append(
                {
                    "type": ptype,
                    "subtype": subtype,
                    "offset": poff,
                    "size": psize,
                    "label": cstr(label),
                    "flags": flags,
                }
            )
            off += 32
        if parts:
            print(f"partition table offset: 0x{table_offset:x}")
            return parts
    return []


def parse_app(data: bytes, offset: int):
    if offset + 0x20 > len(data) or data[offset] != IMAGE_MAGIC:
        return None
    info = {
        "image_magic": data[offset],
        "segment_count": data[offset + 1],
        "spi_mode": data[offset + 2],
        "spi_speed_size": data[offset + 3],
        "entry": u32(data, offset + 4),
    }
    desc = offset + 0x20
    if desc + 0x100 <= len(data) and u32(data, desc) == APP_MAGIC:
        info.update(
            {
                "version": cstr(data[desc + 0x10 : desc + 0x30]),
                "project": cstr(data[desc + 0x30 : desc + 0x50]),
                "time": cstr(data[desc + 0x50 : desc + 0x60]),
                "date": cstr(data[desc + 0x60 : desc + 0x70]),
                "idf": cstr(data[desc + 0x70 : desc + 0x90]),
            }
        )
    return info


def strings_in(data: bytes, minimum: int = 4):
    return [
        m.group().decode("ascii", "replace")
        for m in re.finditer(rb"[\x20-\x7e]{%d,}" % minimum, data)
    ]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path)
    ap.add_argument(
        "--strings",
        action="store_true",
        help="dump every matching keyword string instead of only unique snippets",
    )
    args = ap.parse_args()

    data = args.image.read_bytes()
    print(f"image: {args.image} ({len(data)} bytes, {len(data) / 1024 / 1024:.1f} MB)")

    parts = parse_partitions(data)
    if not parts:
        print("no partition table found at 0x8000")
        return

    print("\npartition table:")
    print(f"  {'label':16s} {'type':>4s} {'sub':>4s} {'offset':>10s} {'size':>10s}")
    for p in parts:
        print(
            f"  {p['label']:16s} {p['type']:4d} {p['subtype']:4d} "
            f"0x{p['offset']:08x} 0x{p['size']:08x}"
        )

    apps = [p for p in parts if p["type"] == 0 and p["offset"] + 0x100 < len(data)]
    for p in apps:
        info = parse_app(data, p["offset"])
        if not info:
            continue
        print(f"\napplication at {p['label']} (0x{p['offset']:x}):")
        for key, value in info.items():
            print(f"  {key}: {value}")

    print("\nkeyword scan:")
    found = False
    for p in parts:
        if p["size"] == 0 or p["offset"] >= len(data):
            continue
        chunk = data[p["offset"] : min(p["offset"] + p["size"], len(data))]
        seen = set()
        for s in strings_in(chunk):
            low = s.lower()
            for keyword in KEYWORDS:
                if keyword.lower() in low:
                    key = (keyword.lower(), s)
                    if key in seen and not args.strings:
                        continue
                    seen.add(key)
                    print(f"  [{p['label']}] {s}")
                    found = True
                    break
    if not found:
        print("  no known keywords found")


if __name__ == "__main__":
    main()

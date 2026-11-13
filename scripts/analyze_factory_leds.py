#!/usr/bin/env python3
"""Extract LED-driver evidence from the exact, untouched ColoPLAY factory dump."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_SHA256 = "9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e"
DEFAULT_IMAGE = ROOT / "backups/coloplay-factory-20261113-131459.bin"


def analyze(path):
    flash = Path(path).read_bytes()
    digest = hashlib.sha256(flash).hexdigest()
    if len(flash) != 0x1000000 or digest != EXPECTED_SHA256:
        raise ValueError("these addresses apply only to the original 16 MiB dump with SHA-256 " + EXPECTED_SHA256)
    app = 0x20000
    if flash[app] != 0xE9:
        raise ValueError("factory application image header is missing")
    segments = []
    offset = app + 24
    for _ in range(flash[app + 1]):
        address, size = struct.unpack_from("<II", flash, offset)
        offset += 8
        segments.append((address, size, offset))
        offset += size

    def location(address, size=1):
        for start, length, file_offset in segments:
            if start <= address and address + size <= start + length:
                return file_offset + address - start
        raise ValueError("address is outside initialized application segments: %x" % address)

    def memory(address, size):
        start = location(address, size)
        return flash[start:start + size]

    def u32(address):
        return struct.unpack("<I", memory(address, 4))[0]

    def record(address, value):
        return {"vma": hex(address), "flash_offset": hex(location(address)), "value": value}

    # The constructor byte stream and selected native instructions are pinned
    # so this script cannot silently attach another meaning to nearby data.
    for address, expected in {
        0x3C267C14: b"_G.buffer=qt.new(14)",
        0x3C26988F: b"qt.init(48)",
        0x4201E33C: bytes.fromhex("36 a1 00"),  # entry a1,80
        0x4201E351: bytes.fromhex("c2 a1 e0"),  # movi a12,480
        0x4201E361: bytes.fromhex("0c ec"),     # movi.n a12,14
        0x4201E422: bytes.fromhex("62 6b 12"),  # s32i a6,a11,72
    }.items():
        if memory(address, len(expected)) != expected:
            raise ValueError("factory instruction/source fingerprint differs at " + hex(address))

    lookup_address = 0x3FCA0A58
    lookup = struct.unpack("<16H", memory(lookup_address, 32))
    clock = u32(0x3FCA0A08)
    zero = lookup[0].to_bytes(2, "little")[0] >> 4
    one = lookup[15].to_bytes(2, "little")[0] >> 4
    bit_ns = 1e9 / clock
    for nibble, word in enumerate(lookup):
        symbols = "".join("1100" if nibble & (1 << bit) else "1000" for bit in range(3, -1, -1))
        if word.to_bytes(2, "little") != int(symbols, 2).to_bytes(2, "big"):
            raise ValueError("unexpected symbol lookup table")

    return {
        "image_sha256": digest,
        "application_offset": hex(app),
        "embedded_lua": {
            "init": record(0x3C26988F, "qt.init(48)"),
            "buffer": record(0x3C267C14, "_G.buffer=qt.new(14)"),
        },
        "native_functions": {
            "lua_init_binding": record(0x3C264128, hex(u32(0x3C264128))),
            "spi_init": "0x4201e2e8",
            "lua_write_binding": record(0x3C264228, hex(u32(0x3C264228))),
            "write_wrapper": "0x4201e434",
            "encode_and_transmit": "0x4201e33c",
        },
        "spi": {
            "host": record(0x3FCA09EC, u32(0x3FCA09EC)),
            "host_name": "SPI3_HOST",
            "dma_channel": record(0x3FCA09F4, u32(0x3FCA09F4)),
            "clock_hz": record(0x3FCA0A08, clock),
            "mode": record(0x3FCA09FB, memory(0x3FCA09FB, 1)[0]),
            "mosi_gpio": record(0x3FCA0A24, u32(0x3FCA0A24)),
            "miso_gpio": record(0x3FCA0A28, struct.unpack("<i", memory(0x3FCA0A28, 4))[0]),
            "sclk_gpio": record(0x3FCA0A2C, struct.unpack("<i", memory(0x3FCA0A2C, 4))[0]),
        },
        "encoding": {
            "lookup_vma": hex(lookup_address),
            "lookup_flash_offset": hex(location(lookup_address)),
            "lookup_u16_le": ["0x%04x" % word for word in lookup],
            "zero_symbol": format(zero, "04b"),
            "one_symbol": format(one, "04b"),
            "zero_high_ns": zero.bit_count() * bit_ns,
            "zero_low_ns": (4 - zero.bit_count()) * bit_ns,
            "one_high_ns": one.bit_count() * bit_ns,
            "one_low_ns": (4 - one.bit_count()) * bit_ns,
            "color_order": "GRB",
            "encoded_bytes_per_pixel": 12,
            "software_pixel_capacity": 14,
            "transfer_bits": u32(0x42001E10),
            "transfer_bytes": u32(0x42001E10) // 8,
            "prefix_zero_bytes": 72,
            "suffix_zero_bytes_at_capacity": 240,
            "initial_brightness_byte": record(0x3FCA0A78, memory(0x3FCA0A78, 1)[0]),
        },
        "limits": "14 is the factory software frame capacity, not proof of 14 populated LEDs. Physical index mapping is checked on the device.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", nargs="?", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = analyze(args.image)
    except (OSError, ValueError, struct.error) as error:
        parser.error(str(error))
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
#
# Build the bring-up firmware and merge bootloader + partition table +
# OTA data + application into one image that can be written at 0x0.

set -euo pipefail

cd "$(dirname "$0")/../firmware"

pio run

BOOT_APP0="$(find "$HOME/.platformio/packages" -path '*/tools/partitions/boot_app0.bin' | head -n 1)"
if [[ -z "$BOOT_APP0" ]]; then
  echo "error: boot_app0.bin not found in ~/.platformio/packages" >&2
  exit 1
fi

esptool --chip esp32s3 merge-bin \
  -o firmware-merged.bin \
  --flash-mode qio \
  --flash-size 16MB \
  0x0     .pio/build/coloplay/bootloader.bin \
  0x8000  .pio/build/coloplay/partitions.bin \
  0xe000  "$BOOT_APP0" \
  0x10000 .pio/build/coloplay/firmware.bin

echo
echo "merged image: firmware/firmware-merged.bin"
echo "flash with:"
echo "  esptool --port /dev/cu.usbmodemXXXX --baud 460800 write-flash 0x0 firmware/firmware-merged.bin"

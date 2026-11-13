#!/usr/bin/env bash
#
# Upload firmware to a running ColoPLAY over ArduinoOTA.
#
# Usage: ./scripts/ota_upload.sh <device-ip-or-host> [firmware.bin]

set -euo pipefail

cd "$(dirname "$0")/.."

TARGET="${1:-}"
if [[ -z "$TARGET" ]]; then
  echo "usage: $0 <device-ip-or-host> [firmware.bin]" >&2
  exit 1
fi

BIN="${2:-firmware/.pio/build/coloplay/firmware.bin}"
if [[ ! -f "$BIN" ]]; then
  echo "error: firmware image not found: $BIN" >&2
  echo "run: ./scripts/build_firmware.sh" >&2
  exit 1
fi

SECRETS="firmware/include/secrets.h"
if [[ ! -f "$SECRETS" ]]; then
  echo "error: $SECRETS not found (copy secrets.example.h first)" >&2
  exit 1
fi

OTA_PASS="$(sed -n 's/.*OTA_PASSWORD[[:space:]]*"\([^"]*\)".*/\1/p' "$SECRETS" | head -n 1)"
if [[ -z "$OTA_PASS" ]]; then
  echo "error: OTA_PASSWORD not found in $SECRETS" >&2
  exit 1
fi

ESPOTA="$(find "$HOME/.platformio/packages/framework-arduinoespressif32/tools" -name espota.py | head -n 1)"
if [[ -z "$ESPOTA" ]]; then
  echo "error: espota.py not found in the PlatformIO Arduino package" >&2
  exit 1
fi

echo "OTA upload to $TARGET"
python3 "$ESPOTA" -i "$TARGET" -p 3232 -a "$OTA_PASS" -f "$BIN"

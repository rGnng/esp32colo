#!/usr/bin/env bash
#
# Build the ColoPLAY hardware demo application. The factory bootloader and
# partition table are kept; scripts/flash_bootstrap.py performs the first
# USB flash of the application itself, and scripts/ota_upload.sh updates it
# over WiFi afterwards.

set -euo pipefail

cd "$(dirname "$0")/.."

if [[ ! -f firmware/include/secrets.h ]]; then
  echo 'error: firmware/include/secrets.h is missing; copy secrets.example.h and set credentials first' >&2
  exit 1
fi

if command -v pio >/dev/null 2>&1; then
  pio run --project-dir firmware
elif [[ -x .venv/bin/pio ]]; then
  .venv/bin/pio run --project-dir firmware
else
  echo 'error: PlatformIO is missing. Install it with:' >&2
  echo '  python3 -m venv .venv && .venv/bin/python -m pip install platformio' >&2
  exit 1
fi

APP="$(pwd)/firmware/.pio/build/coloplay/firmware.bin"
echo
echo "application: $APP"
echo
echo "first USB flash (factory bootloader stays in place):"
echo "  python3 scripts/flash_bootstrap.py"
echo
echo "OTA update:"
echo "  ./scripts/ota_upload.sh <device-ip>  # first update from the device LAN"
echo "  ./scripts/ota_upload.sh http://127.0.0.1:10081  # after HTTP OTA is installed"

#!/usr/bin/env bash
#
# Build the ColoPLAY bring-up application.  The factory bootloader and
# partition table are kept; scripts/flash_bootstrap.py performs the first
# USB flash of the application itself, and scripts/ota_upload.sh updates it
# over WiFi afterwards.

set -euo pipefail

cd "$(dirname "$0")/../firmware"

pio run

APP="$(pwd)/.pio/build/coloplay/firmware.bin"
echo
echo "application: $APP"
echo
echo "first USB flash (factory bootloader stays in place):"
echo "  ../scripts/flash_bootstrap.py"
echo
echo "OTA update:"
echo "  ../scripts/ota_upload.sh <device-ip>"

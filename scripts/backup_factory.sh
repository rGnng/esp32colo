#!/usr/bin/env bash
#
# Read the complete factory flash from a ColoPLAY into backups/.
#
# The board must already be in ROM download mode:
#   1. unplug USB (and battery, if fitted)
#   2. hold REC_KEY + press the rotary knob
#   3. plug USB back in while holding both
#   4. release after ~1 second
#
# Usage: ./scripts/backup_factory.sh [serial-port]

set -euo pipefail

cd "$(dirname "$0")/.."

PORT="${1:-}"
if [[ -z "$PORT" ]]; then
  PORT="$(ls /dev/cu.usbmodem* /dev/tty.usbmodem* 2>/dev/null | head -n 1 || true)"
fi
if [[ -z "$PORT" ]]; then
  echo "error: no /dev/cu.usbmodem* port found. Enter download mode first." >&2
  exit 1
fi

mkdir -p backups
STAMP="$(date +%Y%m%d-%H%M%S)"
BASE="backups/coloplay-factory-${STAMP}"

echo "== port: ${PORT}"
echo "== collecting chip, MAC and security information"

esptool --port "$PORT" flash-id | tee "${BASE}-flash-id.txt"
esptool --port "$PORT" read-mac | tee "${BASE}-mac.txt"
# Read-only; the command may not be supported on every ROM revision.
esptool --port "$PORT" get-security-info | tee "${BASE}-security.txt" || true

SIZE_MB="$(sed -n 's/.*Detected flash size: *\([0-9][0-9]*\)MB.*/\1/p' "${BASE}-flash-id.txt" | head -n 1)"
if [[ -z "$SIZE_MB" ]]; then
  echo "error: could not determine flash size; inspect ${BASE}-flash-id.txt" >&2
  exit 1
fi

BYTES=$((SIZE_MB * 1024 * 1024))
echo "== reading ${SIZE_MB} MB (${BYTES} bytes) to ${BASE}.bin"
echo "   keep the board connected and powered until this finishes"

esptool --port "$PORT" --baud 460800 read-flash 0 "$BYTES" "${BASE}.bin"
shasum -a 256 "${BASE}.bin" | tee "${BASE}.bin.sha256"

echo
echo "== backup complete"
echo "   image:    ${BASE}.bin"
echo "   checksum: ${BASE}.bin.sha256"
echo
echo "Restore with:"
echo "   esptool --port ${PORT} --baud 460800 write-flash 0 ${BASE}.bin"

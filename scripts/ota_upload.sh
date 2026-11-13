#!/usr/bin/env bash
# Host/IP: ArduinoOTA from the device LAN. HTTP URL: OTA through a web relay.
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 scripts/ota_upload.py "$@"

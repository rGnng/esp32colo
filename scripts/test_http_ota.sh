#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TEST_BIN="$(mktemp "${TMPDIR:-/tmp}/coloplay-http-ota.XXXXXX")"
trap 'rm -f "$TEST_BIN"' EXIT
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Itests/ota_fakes -Ifirmware/include \
  firmware/src/http_ota.cpp tests/http_ota_test.cpp -o "$TEST_BIN"
for scenario in unauthorized wrong-chip bootloader partition empty truncated abort timeout invalid-md5 multiple-files verification-failure; do
  "$TEST_BIN" "$scenario"
done
for chunk in 1 7 35 36 37 128 1436; do
  "$TEST_BIN" success "$chunk"
done
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/demo_levels_test.cpp -o "$TEST_BIN"
"$TEST_BIN"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/lighting_settings_test.cpp -o "$TEST_BIN"
"$TEST_BIN"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/factory_led_frame_test.cpp -o "$TEST_BIN"
"$TEST_BIN"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/rotary_detents_test.cpp -o "$TEST_BIN"
"$TEST_BIN"
python3 -m unittest discover -s tests -p 'test_ota_upload.py' -v
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/clock_settings_test.cpp -o "$TEST_BIN"
"$TEST_BIN"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include tests/usb_dhcp_test.cpp -o "$TEST_BIN"
"$TEST_BIN"

for fixture in scene_settings gesture_engine action_feedback log_buffer chord_engine scene_cue idle_timer; do
  "${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -Ifirmware/include "tests/${fixture}_test.cpp" -o "$TEST_BIN"
  "$TEST_BIN"
done
python3 -m unittest discover -s tests -p 'test_usb_*.py' -v

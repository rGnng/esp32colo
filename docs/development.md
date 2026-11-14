# Development baseline

Hardware bring-up is complete. The current code is a working foundation for
feature development; this document describes its implementation and evidence
limits. User-facing behavior belongs in the [firmware guide](firmware.md),
requests in the [API](api.md), and build/update procedures in [Flashing](flash.md).
Bluetooth controls, pairing and the bidirectional media-state contract are in the
[Tab5 integration guide](tab5-integration.md). Serial companion work remains deferred.

## Current verified release

The current production firmware is **0.6.19-scene-order**, source
[`18f3a79`](https://github.com/rGnng/esp32colo/commit/18f3a793fcc9749a0afcc0eb6c98f860f96f8659). It includes persistent scene order,
source-key action lighting with brightness overrides, symmetric ring fills,
manual brightness and a compact header. Ambient-light sensing/calibration/Auto
and the Sound/Bluetooth help paragraphs are removed.

Application-only OTA on **2026-11-14** redeployed the committed image after both
builds passed. The running MD5 matched; a changed OTA slot and lower uptime
confirmed a new boot even though the binary was identical to the prior
candidate. Hardware was ready, boot attempts cleared, recovery off, BLE feedback
fresh and input/audio/LED error counters zero. Saved scenes, their order, active
selection and General settings matched the pre-update snapshot.

The host suite and browser/device checks cover scene append/reuse/reorder and
restart persistence, Next/Previous wrap, stable explicit targets, per-key/ring
feedback, manual General/scene brightness, searchable HID keys, removed sensor
UI/telemetry, and 320/375/768/1440 px layouts without JavaScript errors. The
latest frozen application/ELF, manifest and before/after evidence are in ignored
`backups/releases/18f3a79/`; feature checks remain under the release-specific
folders listed in the history below. Tab5's earlier integration baseline is
recorded alongside ColoPlay; it was unchanged during this deployment.

| Item | Recorded value |
|---|---|
| ColoPlay firmware | `0.6.19-scene-order` |
| ColoPlay source | [`18f3a79`](https://github.com/rGnng/esp32colo/commit/18f3a793fcc9749a0afcc0eb6c98f860f96f8659) |
| ColoPlay application bytes | 4,089,856 |
| Running image MD5 | `e67b64991746ba5279aa23834d801985` |
| Application SHA-256 | `6f1c46f0b76c3a843d59c78d54907a57ddb944ee145936ebff1ff623246a48a5` |
| Latest verified OTA slot | `ota_app0` |
| Tab5 BLE source | [`79c5dac`](https://github.com/rGnng/mpdtab5/commit/79c5dacd1f38a955f3ea2a8abff5b719d5d616d3) |
| Tab5 base | `a34fa8a2c7547573f128b47061d7359f3f504a81` |
| Tab5 application bytes | 2,842,400 |
| Tab5 running ELF SHA-256 | `46a7807e5e8e483d0e8b8e2f5bab53e585a0448b1a74f476739be17db28e59c9` |

## Earlier Bluetooth integration validation

The following records qualify the earlier Tab5/BLE integration and bring-up.
Later feature-specific checks are in [Release history](#release-history).

Tab5 focused MPD/volume/BLE tests pass, as do 21 native UI CTests. Its untouched
`a34fa8a` full suite has eight failures and six fixture errors; the integration
has the same failures and five errors after fixing the D100H lifecycle fixture's
missing SD-readiness stub. Comparing failure identities found no new cases.

Live checks confirmed encrypted pairing, persisted-bond reconnect after OTA,
raw MPD volume and metadata feedback, 64-step/coarse-16 volume round trips,
pause/resume, both seek directions and release, and Page Up/Down acknowledgments.
These run saved bindings through HTTP; physical button-to-screen latency was
not measured. Sleep through the existing runtime control turns off screen and
LEDs while BLE state remains fresh. An automatic timeout qualification could
not complete during active physical testing; the prior idle-timer tests pass.
The custom timer was restored to 20 minutes. Tab5 SD/MPD remain connected and
its entire `/api/config` matches the pre-update snapshot.

For 0.6.1, v4→v5 scene and v2→v3 General migrations were checked against the
live pre-OTA snapshot: all existing scene definitions, bindings, active selection,
scene-cue enable and timeout were preserved. Only the requested Tab5 BLE profile
was then reconfigured to the eight navigation bindings. Other scene definitions
were compared before/after and remained unchanged. Live API dispatch verified
Up/Down, Back, both ordered Page Up/Down bindings and the two ordered bracket
bindings with Applied results. Enter is verified against Tab5's existing focus
activation code; it was not invoked blindly on an unknown focused live item.
Sound gain 0/35 was captured by the audio task, including scene cues, then
restored to the saved gain. Ring progress reported valid playback fractions. A controlled restart confirmed
all scene/General settings and ordered bindings persisted, boot confirmation
completed, and encrypted feedback reconnected without a new panic. Both opposite
Button 2/4 bindings were then executed independently after reboot. Tab5
application hash and complete configuration stayed unchanged, with MPD/SD healthy.
Physical ordering is covered by the recognizer tests; API Run dispatch bypasses
physical recognition. The production/USB builds, host suite (including the
GATT initialization regression) and browser form checks pass.

Scene v3→v4 migration preserved existing definitions and settings at first boot.
The separate Tab5 BLE test profile was added later; existing Global controls
remain available. Subsequent physical/web edits are user configuration, not a
reason to restore an old snapshot. General, clock, WiFi, USB and lighting NVS
remain independent. No bootloader, partition or Tab5 C6 image was changed.

Initialization testing exposed Wi-Fi/BLE coexistence startup failures.
Enabling Wi-Fi modem sleep before staged controller/host initialization resolved
that startup failure. A subsequent GATT crash was captured in the 0.6.1 test:
`BLECharacteristic::handleGATTServerEvent` handled CONF through the unstarted
battery characteristic implicitly created by `BLEHIDDevice`, leading to
`BLEServer::getConnId()` / LoadProhibited. Explicitly constructing only the used
HID and device-info services removes that uninitialized characteristic. The
regression fixture checks that all published characteristics have initialized
service ownership before confirmation dispatch, without advertising a fictitious
battery. Retained panic records now identify the exact crashing ELF hash.

This is a first working BLE integration, not long-duration reliability
qualification. Generic PC HID and the physical ring direction still
require hands-on verification. Later checks confirmed the key LED order and
ring origin; see [verified baseline and open items](#verified-baseline-and-open-items).

Ignored local evidence is under `backups/ble-first/`: frozen pre-test recovery
images, candidate-1 Tab5, the earlier candidate-4 ColoPlay, before/after JSON, live command
results and sleep snapshot. Tab5 was developed in an isolated checkout, then
its tested source/docs/tests were copied into the original main checkout with
an assertion that it was still clean at `a34fa8a`. The lock retains baseline
component hashes, and USB is pinned to 1.6.0 to prevent an incidental upgrade.
The 0.6.1 evidence is in `backups/ordered-modifiers/`: initial failing image/ELF
and captured panic in `final/`, corrected image/ELF and manifest in `hid-fixed/`,
plus preserved configuration snapshots, mapping and validation results.
The preceding committed ColoPlay release was `0.5.4-idle-sleep` at `06b4e8c`,
MD5 `e9a9c17bf4b88ed4e3adfe53a58f1d71`; its evidence remains under
`backups/releases/06b4e8c/`.

## Source layout

| File / area | Responsibility |
|---|---|
| `firmware/src/bluetooth_service.cpp` | Bonded HID/custom GATT transport, bounded queues, preview and returned remote state |
| `firmware/include/coloplay_ble_protocol.h`, `volume_grid.h` | Shared 20-byte wire contract and Tab5 volume-grid arithmetic |
| `firmware/src/panic_capture.cpp` | Retained bounded panic diagnosis and crashing ELF identity across reset |
| `firmware/src/main.cpp` | Startup, WiFi/NVS, shared HTTP routes, serial commands, OTA and recovery orchestration |
| `firmware/src/hardware_demo.cpp` | Clock/Bluetooth rendering and scene overlays, inactivity sleep, local action dispatch, manual lighting and supply telemetry |
| `firmware/src/scene_service.cpp` | Versioned NVS scenes, atomic CRUD/bindings, activation, ordered dispatch and temporary effects |
| `firmware/src/input_worker.cpp`, `input_trace.cpp` | Dedicated GPIO/PCNT gesture task, bounded event queue and Button 2 timing trace |
| `firmware/include/scene_order.h` | Stable-ID scene permutation, append/move and ordered Next/Previous navigation |
| `firmware/include/action_feedback.h`, `action_lighting.h` | Independent per-key/ring overlays, brightness and timed screen feedback |
| `firmware/include/ring_fill.h`, `playback_clock.h` | Clockwise/symmetric coverage and shared smoothed playback position |
| `firmware/include/bitmap_font.h`, `playback_assets.h` | Flash-backed Japanese playback font and local connection icon |
| `firmware/include/scene_settings.h` | Bounded scene/action schema, validation, protected Default and reference retargeting |
| `firmware/include/gesture_engine.h` | Independent nonblocking click/hold/turn recognizers with cancellation |
| `firmware/include/gesture_policy.h`, `chord_engine.h` | Binding-aware recognition timing and reusable held combination members |
| `firmware/src/general_service.cpp`, `firmware/include/scene_cue.h` | Persistent General preferences and direction-specific scene sounds |
| `firmware/include/idle_timer.h` | Nonblocking inactivity countdown and rollover-safe elapsed time |
| `firmware/src/clock_service.cpp` | NTP, supported timezones, atomic clock settings and time snapshots |
| `firmware/src/usb_network.cpp` | Selectable CDC-ECM, lwIP netif, one-client DHCP and USB stages/telemetry |
| `firmware/src/usb_phy_mode.c` | Restore serial/JTAG PHY selection after warm resets |
| `firmware/src/led_strip.cpp` | Factory-format SPI3 LED transmission |
| `firmware/src/audio_demo.cpp` | ES8311 setup, full-duplex I2S, microphone telemetry and tones |
| `firmware/src/http_ota.cpp` | Authenticated application-only upload lifecycle |
| `firmware/web/` | Editable HTML, CSS, JavaScript and vendored Bootstrap Sass sources |
| `firmware/include/web_ui.h`, `web_assets.h` | Generated HTML and compressed static assets in flash |
| `scripts/build_web_assets.py` | Standard-library asset packer; optional Sass rebuild on the development computer |
| `firmware/include/board_pins.h` | Working custom GPIO configuration; not a complete factory-derived hardware map |
| `firmware/include/*_settings.h` | Versioned settings records and validation |
| `firmware/include/rotary_detents.h` | Two-edge detent decoder retaining partial movement |
| `firmware/include/factory_led_frame.h` | Recovered GRB encoding and reset padding |
| `firmware/lib/` | Vendored ES8311 and TinyUSB ECM code with licenses |
| `scripts/`, `tests/` | Build/update/backup/evidence tooling and host fixtures |
| `firmware/lcd_probe/` | Separate, optional ESP-IDF RAM diagnostic |

The `HardwareDemo` source namespace remains to limit driver churn; `/api/demo`
is a compatibility lighting alias. Normal input dispatch and screen selection
now flow through scenes, not the retired diagnostic page/tour handlers.

## Runtime and persistence

The main loop services HTTP, OTA, NTP, queued input actions and peripheral work.
A priority-3 task samples inputs/recognizes gestures every 1 ms and hands events
to the main loop through a 64-entry queue. Clock rendering targets 5 fps;
Bluetooth screens target 20 fps. Overlays use the current screen cadence.
Blank/sleep suppress display updates.
LED frames target about 30 Hz, supply ADC1 sampling runs every 500 ms and browser
status/log polling runs every two seconds while visible. There is no ambient
ADC2 sampling. Audio has its own task. OTA pauses hardware/audio and clears
queued input while writing flash.
Startup and the initial 30-second run window participate in the failed-start
recovery guard described in [Flashing](flash.md#recovery).

| NVS namespace | Record | Behavior |
|---|---|---|
| `wifi` | `ssid`, `pass` | Seeded from `secrets.h` only when no stored SSID exists; web/serial changes restart |
| `clock` | Versioned `settings` blob, 804 bytes (v2; migrates 296-byte v1) | Three NTP servers and supported zone committed together; apply and sync immediately |
| `lighting` | Versioned `settings` blob, 44 bytes | Manual levels and mode/color; retired fields reserved; web saves before apply; knob writes debounced 1.5 s |
| `usbnet` | Versioned `settings` blob, 12 bytes | Mode, prefix, device/host addresses saved together; restart to apply |
| `general` | Version-3 `settings` blob, 16 bytes | Scene-change sound, sound gain and sleep timeout in minutes (default 60); save before apply |
| `scenes` | Version-7 `settings` blob, 3304 bytes; version-1 `order`, 12 bytes | 8 scenes, 64 gesture bindings, 3 actions each; active selection and stable IDs preserved; display/navigation order saved independently |
| `demo` | Image MD5 and failed-start attempts | Recovery state, not user configuration |

Invalid/missing settings blobs use their declared defaults. Normal OTA preserves
NVS. A full factory-image restore overwrites it. Awake state, speaker volume,
last event and temporary feedback are runtime state. The active scene and all action definitions/effects are saved settings.
Ambient-light sampling and automatic dimming are removed as of 0.6.17. The
lighting record retains its old 44-byte layout, reserving the retired fields;
scene levels similarly reserve the old Auto byte. Neither byte affects output.
Manual percentages and scene/General inheritance directly determine brightness.
GPIO16 is no longer configured or sampled. Supply ADC1 telemetry remains; its
voltage conversion and physical scaling are custom implementation choices,
not recovered analog topology or factory findings.

## Build and tests

Follow [tool setup](flash.md#tool-setup-and-build), then run:

```bash
./scripts/build_firmware.sh
./scripts/test_http_ota.sh
```

The historical name `test_http_ota.sh` now runs the broader host regression suite:

| Fixture | Coverage |
|---|---|
| `http_ota_test.cpp` | Authentication, chip/app/partition validation, abort/timeout, digest/finalization failures and chunked success |
| `test_ota_upload.py` | Real image validation, secrets escaping, multipart/digest/auth and reboot/image verification |
| `demo_levels_test.cpp` | Codec gain bounds and RGB scaling |
| `lighting_settings_test.cpp` | Manual defaults/bounds and legacy byte-layout compatibility with retired fields ignored |
| `factory_led_frame_test.cpp` | All channel bytes against the extracted factory table, GRB order, slot capacity and reset padding |
| `rotary_detents_test.cpp` | Detents, partial turns, reversals, bounce and batched edges |
| `clock_settings_test.cpp` | Hostname/fallback validation, legacy migration, timezone offsets and daylight saving |
| `scene_settings_test.cpp` | Protected Default, ordered bindings, deletion/retargeting, corrupt storage and capacity |
| `scene_order_test.cpp` | Moves, sparse append/reuse, wraparound, stable IDs, protected Default and invalid permutations/positions |
| `action_feedback_test.cpp` | Independent key/ring/screen targets, brightness, expiry, combinations, rollover and rendered colors |
| `action_light_settings_test.cpp` | Brightness/RGB/sound packing, record sizes and atomic legacy migration validation |
| `action_sound_test.cpp` | Per-action cue gating, directional scene tones, tone gating and old sound-flag migration |
| `playback_ring_test.cpp` | Clockwise/symmetric bounds, monotonicity, pairs, endpoints and partial coverage |
| `playback_clock_test.cpp` | Shared position interpolation, stale samples, pause/resume/seek and bounded extrapolation |
| `bitmap_font_test.cpp` | Flash font parsing, UTF-8 text measurement, fit and glyph rendering |
| `bluetooth_protocol_test.cpp`, `bluetooth_hid_test.cpp` | BLE wire/usage validation and initialized GATT service ownership |
| `log_buffer_test.cpp` | Bounded log storage, cursors and wrap/reset behavior |
| `test_input_worker.py` | Production task boundary and timing traces, generation/overflow handling, encoder and held modifiers |
| `gesture_engine_test.cpp` | Click counts, hold/release, modified turns, click suppression, debounce, independent inputs, cancellation and timer wrap |
| `chord_engine_test.cpp` | Exact groups, repeated held-key combinations, binding-aware timing, superset arbitration, boot-held inputs and cancellation |
| `idle_timer_test.cpp` | One-hour default, disable, timeout boundaries/reset, millis wrap, General v1 migration and numeric validation |
| `scene_cue_test.cpp` | General preference validation, directional tones, wraparound, unchanged/muted transitions |
| `usb_dhcp_test.cpp` | Subnet validation, DHCP wire replies, sole-lease ownership, renewal/release/decline/expiry, timer wrap and malformed packets |

C++ fixtures use the host compiler; they do not prove physical output. The Python
image test also validates a built application when present. Browser and live
hardware checks were performed separately during bring-up; they are not bundled
as a browser runner in this repository. Font regeneration and RAM-probe building
are separate optional workflows.

For runtime changes, verify the affected feature over the shared UI/API and
inspect its counters/errors. Preserve existing user settings after temporary
checks. A framebuffer validates the render result; visible LEDs/audio require
physical observations rather than transfer counters alone.

## Driver constraints and provenance

The current firmware uses Espressif32 6.12.0 / Arduino 2.0.17. It preserves the
factory bootloader and partition layout, uses `qio_opi` with enabled PSRAM and
runs the LCD and LEDs on separate SPI buses. These are established build choices.
Do not infer board/module connector wiring from them. Recovered hardware values
are limited to the [factory GPIO map](gpio-map.md).

TFT_eSPI must use the S3 register index 2 path (`USE_FSPI_PORT=1`); compile-time
assertions guard the register base. The current LCD bus is 40 MHz; the factory
configuration recovered from the image is 80 MHz. Generic NeoPixel transmission
was replaced by the factory SPI3 encoder; Adafruit NeoPixel supplies color/gamma
helpers only. Gamma correction precedes brightness scaling.

The ES8311 legacy-I2C driver is vendored from Espressif at revision
`901d9481034e2095a2faf8a72f4ecf05680ddd71`; see its
[upstream record](../firmware/lib/es8311/UPSTREAM.txt) and
[Apache license](../firmware/lib/es8311/LICENSE).

Arduino's precompiled TinyUSB core omits the built-in network class. The pinned
TinyUSB 0.16 ECM driver is registered through the application-class callback;
see [vendored code](../firmware/lib/tinyusb_ecm) and its
[license](../firmware/lib/tinyusb_ecm/LICENSE). Only ECM is advertised; retained
RNDIS support files belong to the upstream combined driver. USB transmit frames
are bounded/queued to the USB task. DHCP uses static bounded packet buffers to
fit the lwIP task stack. Neither the Arduino SDK nor the factory bootloader is
patched for these features.

### Fonts

The plain clock uses Inter 4.1, supplied with its [OFL-1.1 license](../assets/fonts/inter/LICENSE.txt).
The unmodified variable font is from the [Inter 4.1 release](https://github.com/rsms/inter/releases/tag/v4.1),
SHA-256 `4989b125924991b90d05b2d16e0e388c48f7d5bb8b30539bbf9c755278d0ccaf`.
Generated subsets are compiled into flash without a filesystem upload:

```bash
.venv/bin/python -m pip install Pillow==12.0.0
.venv/bin/python scripts/generate_clock_fonts.py
```

The generator also retains the smaller subsets from the earlier clock layout;
the current face draws only the 64 px hours/minutes and sync-dependent colon.

## Scene architecture

InputWorker owns GPIO/PCNT sampling and binding-aware gesture recognition.
Generation-tagged events cross its bounded queue to the main loop, where
SceneService resolves the active scene's binding or a matching Global Default
binding. Up to three actions execute in order through the hardware/Bluetooth
handlers. Scene switching ends the list, advances the epoch and cancels pending
gestures. Held combination modifiers can anchor repeated trigger presses.

Each key and the knob ring have independent feedback deadlines and brightness
selection. Combinations target every member key. Percentage feedback scales a
button's own LED or fills the knob ring using the scene's clockwise/symmetric
style. Screen overrides have a separate lifetime. BluetoothService supplies
fresh returned MPD volume, playback state, metadata and position; the shared
playback clock drives both display and ring. Serial companion work is deferred.

Scene definitions/actions stay in the fixed 3304-byte settings record; a separate
12-byte permutation controls display/Next/Previous order without changing IDs.
New scenes append, Default stays protected and explicit targets remain IDs.
Configuration saves before application; rapid brightness adjustments debounce
writes for 1.5 seconds. Scene selection and reordered tabs save immediately.
The USB exact-old-default migration changes .21/24 + .30 to
.1/30 + .2 without replacing custom networks. Clock migration retains the old
primary/timezone and adds empty fallback slots.

## Verified baseline and open items

Bring-up observations from **2026-11-13**, updated with the **2026-11-14** checks:

- User-confirmed visible display, speaker, ring and four key LEDs; key/encoder
  events observed. Physical notch-by-notch confirmation after the decoder change
  remains distinct from its host tests.
- NTP clock framebuffer checked with/without the sync colon, and without date,
  timezone, seconds, status text or outer ring.
- Independent lighting levels, static/rainbow selection,
  invalid-request handling and settings persistence exercised over the live API.
  Shared web UI checked at mobile width without JavaScript errors.
- 256 KiB PSRAM pattern test passes; ES8311 ID `0x8311`, revision 1, responds at
  I2C `0x18`; microphone data streams without reported I/O errors.
- Earlier ADC2 samples did not establish an externally exposed ambient sensor.
  Sampling, calibration and Auto were removed in 0.6.17. Hardware ready, passing
  PSRAM, audio I/O counters and active USB Ethernet/DHCP were checked separately.
- HTTP OTA verified the running application's MD5; USB enumeration, ECM link and
  the single DHCP lease were observed alongside display/audio/NTP operation.

These observations apply to the tested unit/image, not every C169 variant or a
long-duration qualification. Visual checks confirmed key slots 9/10/11/12 as
Button 1/2/3/4 and ring slot 0 at 12 o’clock. Ring direction/color calibration,
exact panel variant/readback, charge transitions and battery state of charge
remain unestablished. The supply estimate is not an independently calibrated battery
measurement. Full factory restore boot is untested.

Factory analysis has **not** established connector contacts, module-pad wiring,
analog resistor/component identities or all peripheral GPIO roles. Hardware
reference documents record only the recovered binary findings. Remaining custom
pin names and scaling constants must not be treated as proof of board topology.

## Release history

These notes describe each release at the time it was tested. Current behavior,
source identity and deployment evidence are summarized above; older references
to Auto, former layouts and storage versions are historical.

### Scene release validation (0.4.0)

The host suite covers model corruption/capacity and gesture timing, including
rotation during the knob debounce window and scene changes during a pending
press. Live API checks exercised protected Default, CRUD, atomic rejection,
ordered local actions, percentage ring feedback, and scene/config persistence
through reboot. Clock primary/timezone migration and the USB /30 migration were
checked against saved pre-update settings. Browser checks exercised both tabs,
scene creation, 5/9 gesture rows, action/effect editing and ordering, save/run/
activate/reload, and mobile width without JavaScript errors. These checks are
separate from companion-app integration, which has not been implemented.

### Table editor update (0.4.1)

Scenes are tabs, with Idle screen and a single configured-binding table. Row
editors support add, duplicate, delete and key/event moves. `editBinding` checks
source/destination conflicts before candidate mutation, permits moves at full
capacity, and commits the result once. Optional create-only requests prevent
accidental replacement when another editor has already assigned the pair. The
scene storage schema, recognizers and USB behavior are unchanged.

USB capability note: ESP32-S3 USB-OTG supports composite CDC/HID functions. Its
fixed Serial/JTAG controller shares the internal PHY with USB-OTG, so it cannot
run alongside them on the existing USB connection. A second external PHY/USB
path is required to run both controllers simultaneously. This is a SoC limit,
not a recovered board pinout. See [Espressif's USB device documentation](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html).
The current firmware still provides CDC-ECM or Serial/JTAG modes; HID has not
been added by this UI update.

The opt-in [USB concurrency experiment](usb-concurrency-test.md) provides a
separate CDC-ECM/vendor-HID image, host JTAG checks and two-slot local recovery.
It does not change the production USB modes.

### Per-scene lighting (0.4.2)

The scene record is now v2 (2920 bytes), with independent ring and key LED
modes/colors. v1 (2792-byte) records validate and migrate without replacing
scene IDs, names, bindings, ordered actions or selection; both new modes inherit
General. Migration retains the loaded configuration in RAM and reports an error
if persistence fails. Static scratch storage keeps the legacy record off the
setup task stack. Metadata-only saves retain colors.

The renderer resolves General or scene lighting per LED group, then applies
action feedback, gamma correction and the group's existing effective brightness.
Proportional feedback changes only the ring, leaving button scene lighting in
place. LED test colors and sleep behavior retain their existing precedence.

Validation: the host suite covers independent lighting validation/resolution,
v1 migration at full binding capacity, corruption rejection, and deletion reset.
The production firmware builds. Chromium checks with a mocked device API cover
desktop/mobile layouts, independent mode/color saves, scene switches, reload and
unsaved draft retention without JavaScript errors.

OTA on 2026-11-13 verified running MD5 `ec04fae0b6114e1dfe04040d88413d9a`.
The device migrated its existing scene catalog without changing names, IDs,
bindings or active selection; General brightness/Auto, clock and USB network
settings matched their pre-update snapshots. Live API checks saved distinct
ring/button colors, changed just one mode, rejected an invalid color atomically,
and removed the inactive test scene afterward. Hardware continued rendering
past boot confirmation with no LED/audio I/O errors, active USB Ethernet and
NTP sync. These are telemetry/API checks; physical color appearance still
requires observation on the unit.

### Screen capture disabled (0.4.3)

Removed the web preview image and automatic screenshot refresh, together with
`/api/screenshot.bmp` and its row-by-row BMP encoder. Old browser tabs receive
404 for capture requests rather than triggering image encoding. The display's
render framebuffer remains in use for the physical clock. Scene settings and
storage format are unchanged.

OTA verified running MD5 `a7e80c4934ae872ae946be25e6443c16`. Live Chromium
checks observed multiple status refresh intervals with no image requests or
JavaScript errors at desktop/mobile widths. The removed capture route returns
404; scene definitions and lighting settings matched the pre-update snapshot.
Hardware passed boot confirmation and its physical display frame counter kept
advancing. No CPU-load benchmark was taken.

### Compact Bootstrap configuration (0.4.4)

General, Scenes and Device use a compact Bootstrap dark layout with a desktop
navigation rail, mobile tabs, smaller form rows and a dense binding table.
Diagnostics and raw telemetry moved to Device. Scene/general APIs and NVS
formats are unchanged. HTML/CSS/JS are editable under `firmware/web/`, with
PlatformIO generating the embedded headers before each build.

The pinned MIT-licensed Bootstrap 5.3.8 subset includes only forms, buttons,
navs, tables and required foundations. It is 7441 gzip bytes. App CSS and JS add
3619 and 8231 gzip bytes, respectively (19291 bytes total). The normal build
packs checked-in compiled CSS using Python's standard library; only changes to
the Bootstrap Sass subset need the optional host Sass compiler. See the
[web source guide](../firmware/web/README.md).

The gadget serves precompressed bytes from flash with content-hashed URLs and
immutable browser caching. No CDN, external fonts, icon library, jQuery or
Bootstrap JS is loaded. No runtime compression/decompression occurs on ESP32.
Status polling runs at two seconds, skips hidden pages and resumes on return;
raw JSON is formatted only while visible telemetry is expanded. Screen capture
remains disabled.

The production build uses 79780 bytes of static RAM, unchanged from 0.4.3.
The application image is 1063056 bytes, 9680 bytes smaller than 0.4.3. These
are build sizes, not a CPU benchmark. Browser checks cover a real local gzip
server with mocked configuration APIs, native scene/binding dialogs, ordered
commands, General requests and layouts from 320 to 1440 pixels.

OTA verified running MD5 `011a81b4a522c608212c3a00267cf3aa`. Live-device
checks confirmed all three gzip asset responses, zero asset-transfer bytes on
browser reload from cache, working desktop/mobile navigation and native binding
dialogs, hidden-tab polling suppression, and no external/capture requests or
JavaScript errors. Scenes, active selection, lighting/Auto, clock and USB
settings matched the pre-update snapshot. The device passed boot confirmation
and continued physical display rendering with USB Ethernet active.

### Header notifications, UTC choices and RAM log (0.4.5)

Save/error/dirty messages occupy a reserved title-bar slot; removed the duplicate
inline scene-save status. Desktop and 390/320-pixel browser checks measured
identical panel positions/header heights for empty, short and long messages.

City timezone labels show UTC offsets, with both standard/daylight offsets for
seasonal rules. Explicit fixed offsets use new supported IDs, with the existing
UTC+00:00 choice still stored as Etc/UTC. POSIX sign reversal is covered by
winter/summer tests for every fixed choice; the storage layout stays unchanged.

`DeviceLog` mirrors application boot/hardware/WiFi/clock/configuration events to
a 4096-byte RAM ring and the existing serial output. Records are bounded to 383
text bytes plus timestamp/truncation markers; no logging occurs per display/LED
frame. Serial status dumps and OTA progress bypass the ring. The independent
HTTP OTA module's messages remain serial-only. SDK/ROM/panic messages are not
captured. A short critical section protects appends and 2048-byte snapshots;
JSON encoding and HTTP output happen outside it.

The web log uses cursor-based reads, reset detection, text-only rendering,
manual refresh, pause and a browser-created download. Reads stop when the
viewer/panel/browser tab is hidden. Tests exercise ring overwrite, chunking and
stale cursors. No new NVS record or background flash write is introduced.
Static RAM is 83892 bytes (+4112 over 0.4.4); application size is 1068960 bytes.

OTA verified running MD5 `385e1bf614d08f79495bf9b93b49d928`. Live device checks
validated UTC-12:00, UTC+14:00 and UTC+05:30 offsets, then restored the original
timezone. Boot/hardware/config messages were retrieved with cursor and invalid
cursor checks. Live Chromium verified stable header geometry and the log's
pause/refresh/download without JavaScript errors; existing scenes, active
selection, General settings and clock servers matched their pre-update values.
The boot confirmed successfully and the physical display kept rendering.

### Readable typography (0.4.6)

Raised input/table text to 16 px, labels/buttons/log text to 14 px, and enlarged
headings, navigation and secondary text. Brightness columns and narrow-screen
scene fields allow for the larger text; notifications retain fixed header space.
Only presentation and version metadata changed; APIs and NVS are unchanged.

OTA verified running MD5 `95a2dfc4932b36aa3e27d1b16884cde7`. Live Chromium
checks confirmed the computed text sizes, overflow-free General/Scenes/Device
layouts from 320 to 1440 px, and stable panel positions during status changes.
No JavaScript errors were observed. The device passed boot confirmation with
scene definitions, active selection, lighting and clock settings preserved.

### Scene combinations, overrides, icons and Global bindings (0.5.0)

The v3 record is 3304 bytes, adding per-channel brightness/Auto overrides,
scene icons and a Default-only Global flag per binding. Separate v1/v2 structs
validate/migrate old records; one legacy scratch union avoids retaining two
copies. Existing settings retain their behavior. Combination IDs use an encoded
four-button mask while keeping physical input IDs and recovery controls intact.

ChordEngine wraps the existing single-input recognizer with an 80 ms bounded
assembly window, exact groups, consumed-member suppression and per-group click
state. It cancels on scene/binding changes, handles boot-held inputs, timer wrap,
release order and pending click sequences, and leaves knob gestures unchanged.
The active/global combination mask is rebuilt only on configuration changes.
Tests cover fallback, overlapping pairs/triples, holds and multi-click timing.

Global lookup prefers the current scene, then marked Default bindings. Custom
scene editors show a nonblocking warning for a matching Global pair. Brightness
resolves per channel from General or saved scene override. Override adjustment
actions increment a separate UI revision, preserving ordered actions and held
inputs; saves debounce for 1.5 seconds. Configuration commits flush pending
levels atomically. The original global lighting record is unchanged.

32 common Bootstrap Icons 1.13.1 SVGs (MIT) are vendored with their license.
There is no official usage ranking claimed. A local SVG sprite serves the web
picker; host-generated 64×64 monochrome bitmaps use 16 KiB flash for the display.
The application renders icon/name for 1600 ms only when the scene ID changes,
then resumes the unchanged clock. No screen capture endpoint is restored.

Live OTA MD5 `070c513701b27f82f71d8d85fa7c99bf` verified v2 migration, separate
channel output, Use General restoration, ordered local brightness adjustments,
Global fallback/local precedence and invalid-input rejection. New scene icon,
levels, bindings, flags and active selection survived a reboot. The test scene
was removed and the previous active scene restored. A Global 1+2 Single → Next
scene example remains for the user's physical test; GPIO timing was tested in
the host recognizer, not injected into the physical unit. Overlay telemetry
and continuing display frames were verified; appearance needs observation.

### WiFi summary and SSID editor (0.5.1)

WiFi shows SSID, connection status, RSSI and IP via the existing status poll.
Disconnected/unreachable states clear RSSI/IP. Credentials move into a native
modal opened by Edit and restart; Save and restart keeps the existing WiFi API.
SSID is used consistently in the UI. Cancel/Escape clear the password draft and
send no request; duplicate submissions are blocked, and failed saves keep the
editor open. Dialog checks used a mocked POST, preserving actual credentials.
Both firmware environments build; the production application uses 85100 bytes
static RAM and is 1103424 bytes. Assets remain locally compressed and cached.

OTA verified running MD5 `eb178aacaded1d88aa4f8a1d9d8131dc`. Live SSID/status/
RSSI/IP and modal open/cancel checks passed from 320 to 1440 px without JavaScript
errors or configuration writes. Scene/global settings and WiFi SSID matched the
pre-update snapshot, and the device passed boot confirmation.


### Scene sound and responsive inputs (0.5.2–0.5.3)

General has a persisted Scene change sound switch, initially on. Successful
active-ID changes queue a 90 ms tone after the scene NVS commit. Next uses
880 Hz and Previous 440 Hz, including sparse scenes and wraparound; direct
activation uses numeric ID direction. Boot, unchanged and failed changes are
silent. Audio telemetry records the last tone actually written to I2S.

0.5.2 OTA MD5 `2f057c2696e28f676bb7bba76c21b1c9` passed live direction,
wraparound, no-op, mute and failed-change checks. Mute survived reboot. Browser
checks covered switch save/reload/error rollback and 320–1440 px layouts.
Temporary test bindings were removed and the original active scene restored.
These checks verified playback telemetry; audible tone distinction needs the
user's observation.

0.5.3 rebuilds per-input gesture masks from active and Global Default bindings
only when configuration changes. Unmapped Hold/Hold-release never start hold
recognition; unmapped Double/Triple add no click-window wait. Single-only fires
after debounce except individual combination members and a knob with modified
turn bindings, which defer until release to avoid conflicting actions. With a
mapped hold, short singles resolve on release. Double without Triple resolves
on its second release.

An exact combination resolves immediately unless a configured superset could
still join inside 80 ms. Releasing any member rearms it; remaining held members
can anchor another combination, including across scene changes. Cancelled raw
presses cannot become fresh edges, and boot-held suppression remains intact.
Host tests cover repeated 4+2 / 4+3 commands, reconfiguration while held,
configured/unconfigured holds, multi-click masks, superset arbitration,
modified knob turns, cancellation and timer wrap. Physical GPIO timing cannot
be injected through the HTTP relay and still needs hands-on testing.

The web scene-icon selector shows only its SVG; its tooltip/accessibility label
retains the icon name. The on-device transition still shows the scene name.


0.5.3 OTA MD5 `35e8a98143a5fff4a7e32415a4da7415` passed exact-image,
hardware-ready and boot-confirmation checks. Scene definitions, General sound,
WiFi, selected brightness and NTP settings matched the pre-OTA snapshot.
Physical combination events changed the active selection during verification;
that selection was left untouched. Live browser checks verified the icon-only
selector, all 32 locally rendered icons and 320–1440 px layouts without
JavaScript errors or configuration writes. Both production and USB-experiment
builds and the complete host suite passed. Evidence is kept locally under
`backups/responsive-inputs/`.


### Blank idle screen and inactivity sleep (0.5.4)

Clock/Blank extend the existing scene-screen enum without changing the 3304-byte
v3 record layout or existing enum values. Scene JSON now reports the stored and
resolved screen choices. Blank skips frame updates and sets backlight PWM to
zero, while preserving scene LED output. Clock/Blank temporary screen effects
expire independently of LED effects; scene-switch overlays remain visible.

General v2 adds a sleep timeout (0 disables, 1–1440 minutes, default 60).
Migration preserves the v1 scene sound flag. Partial General saves validate the
whole request before writing either field. An unsigned inactivity timer handles
millis wrap. Debounced press edges and knob detents wake the outputs; held keys
refresh the timer even without bindings. Wake preserves input action dispatch.
Scene configuration changes and explicit web wake reset the countdown. Status
polling does not. A timeout sets backlight and both LED groups to zero; HTTP,
USB, inputs and saved lighting remain available. The sleep runtime and inactivity
counter are not written to NVS.


OTA MD5 `8ece9463961993d07bf367e5a092d3b8` passed exact-image and
boot-confirmation checks. Both build environments, the host suite and browser
checks passed. Live tests verified General v1 migration, strict/atomic partial
updates, timer save/reload and 320–1440 px layouts without JavaScript errors.
Blank kept scene LEDs on; temporary Clock/Blank effects expired correctly.
Blank and a custom timer survived reboot. With a one-minute test timeout,
telemetry reported the display/ring/keys all at zero while HTTP remained
reachable; explicit web wake restored the scene's selected levels. Physical
button/knob wake still needs hands-on observation. The timer was restored to
60 minutes and the temporary scene removed; original scene definitions and
sound preferences were preserved. Evidence: `backups/idle-sleep/`.

### Bluetooth scenes and storage (0.6.0)

Scene version 4 keeps the v3 3304-byte layout, adding enum values for Bluetooth
volume/media/key actions and volume/playback screens. Migration validates v3's
old enum bounds before changing its version; invalid records are not partially
migrated. Native settings and protected Default behavior remain unchanged.
Older firmware cannot read v4 records; disable BLE rather than downgrading for
radio recovery. `bluetooth/settings` is a separate 16-byte NVS record storing
enabled state and known public peer address; SDK bonding keys remain separate.

The pinned built-in Arduino BLE stack is initialized lazily after hardware is
ready. Wi-Fi modem sleep is enabled before controller/host startup for coexistence.
Initialization errors are reported and do not cause an unbounded retry loop.
Disabling radio advertising/disconnecting leaves the allocated stack resident,
avoiding unsupported deinit/reinit churn. No new library dependency was added.

`PanicCapture` wraps the SDK panic handler without replacing its reset behavior.
It retains bounded scalar fields, reason and up to 16 PC addresses in RTC RAM;
it performs no allocation, network or flash writes in the panic path. Addresses
must be decoded against the exact frozen ELF of the crashing image. This uses
the pinned SDK's private panic ABI and must be reviewed on SDK upgrades.

### Ordered modifiers, effects gain and playback layout (0.6.1)

Scene schema 5 preserves the 3304-byte layout. A combination input keeps its
four-member mask in bits 0–3; bits 4–6 encode modifier index + 1 (zero is Any
order), and bit 7 remains the combination flag. Reserved scene byte 0 now stores
ring display. Strict v4 migration rejects new input encodings/ring modes in a
legacy record, then preserves old fields while upgrading the version. General
schema 3 adds a saved 0–100 sound-effects gain, default 70, to a 16-byte record;
v1/v2 migrations preserve scene-cue enable and inactivity timeout.

ChordEngine tracks debounced presses with their raw-edge timestamps. Bounces
on an already-held key do not change its ordering. Modifier-first groups have
independent gesture/click state from each other and unordered groups; exact
ordered matches take priority. Simultaneous edges have no order. The modifier
can remain held while targets release/repress or switch, without duplicate
standalone actions. Tests cover reversed orders, ties, wrong order, reused
modifiers, held actions, bounce and timer rollover alongside existing gestures.

General effects gain scales generated tone samples before the existing codec
speaker level. It includes scene cues and local tone actions; remote MPD volume
and brightness stay independent. The audio task captures gain with each tone
request and exposes `last_tone_volume`. Zero generates silent samples.

Scene ring progress uses fresh returned Bluetooth playback time, extrapolates
only while playing without seeking, freezes on pause, clears on stop/stale data,
and clamps to duration. Partial segment coverage smooths the nine LEDs. Scene
colors/brightness remain the source; explicit action effects temporarily take
priority. The LCD playback screen draws play/pause/stop/seek shapes above title
and artist, measures long lines and ellipsizes at UTF-8 boundaries, and removes
the LCD progress bar. No emoji font or raster asset is added.

### Japanese playback font and compact layout (0.6.2)

The playback screen uses an offline 16 px Noto Sans JP font, covering all 16,732
characters mapped by the pinned Regular 2.004 Japanese subset OTF. The upstream
unmodified source/license live in `assets/fonts/noto-sans-jp/`; its SHA-256 is
`dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073`.
The optional `scripts/generate_playback_font.py` uses Pillow 12.0.0, fontTools
4.60.1 and CairoSVG 2.8.2 to generate checked-in `playback_assets.h`. Normal
firmware builds do not need these packages or network access.

The CPF1 font contains a sorted 16-byte metric record per codepoint and packed
4-bit antialiased glyph coverage, totaling 2,353,610 flash bytes. Binary-search
lookup reads only required metrics/bitmaps from mapped flash. There is no font
cache/all-glyph allocation in internal RAM or PSRAM. The renderer fits each line
by measured advance, preserves UTF-8 boundaries, supports supplementary-plane
codepoints, skips incomplete trailing sequences and substitutes `?` for missing
glyphs. All glyph offsets/bounds, Japanese/Latin coverage, malformed UTF-8,
measured ellipsis and circular metadata layout have host tests.

A small status icon/label sits at y=54, with title/artist baselines 115/139 around
vertical center and a slim progress/time area below. The state icon is drawn,
not emoji. Unavailable Bluetooth feedback on both volume/playback screens shows
only the bundled Iconify `andrsrxn:bluetooth-connecting` 32 px duotone icon.
The original SVG/MIT license are in `assets/icons/andrsrxn/`; its 4-bit bitmap is
512 bytes. No Iconify runtime, CDN, filesystem font loader or settings schema
change is introduced. Ring progress, bindings and sound gain remain unchanged.

Both production and USB-experiment builds fit the existing 4.5 MiB application
slots; partitions/bootloader stay unchanged. The corrected 0.6.1 app remains a
compatible local recovery image because scene/General schemas are unchanged.
Evidence and frozen images are under ignored `backups/japanese-playback/`.
`layout-preview.png` is a host rendering reference, not a physical LCD capture.

Live 0.6.2 OTA verification confirmed the running image fingerprint and boot
confirmation, unchanged scenes/bindings/active selection and General settings,
continued MPD/SD on the unchanged Tab5 firmware, and fresh BLE metadata. Playback
refresh measured **19.6 fps**, with **84,376 bytes free internal heap** and
**8,255,003 bytes free PSRAM** in the captured sample. Japanese/Latin rendering
and circular bounds are verified by host font tests and the layout preview;
the live track at verification was Latin, so this is not a physical Japanese
LCD photograph. Existing partitions and settings schemas were preserved.

A bounded disable/re-enable check exercised the icon-only unavailable branch:
frames continued updating without a panic, then the saved bonded peer and fresh
playback feedback reconnected. Bluetooth enabled state was restored afterward.

### Icon-only playback state (0.6.3)

Removed Playing/Paused/Stopped/Fast Forward/Rewind text from the playback
header, leaving only its small centered state icon. Removed playback's footer
status words and the volume screen's Playing/Paused line. Title/artist stay
centered with the 16 px Japanese font, and progress/time remain below. API/web
diagnostics retain state and command outcomes. No settings/schema changes.

### Continuous shared playback clock (0.6.4)

The periodic Tab5 GATT state copies cached MPD time, typically refreshed once
per second, while BLE state runs about twice per second and command results may
send additional copies. Previously every packet reset `receivedAt` and rendering
used `elapsedMs + now - receivedAt`; repeated old positions therefore rolled
both indicators backward. Live samples confirmed repeated elapsed positions.

`PlaybackClock` now separates packet freshness from the display anchor. Updates
and snapshot projection use the Bluetooth state lock, capturing `millis()` inside
the lock to avoid a newer anchor paired with an older query time. One projected
`positionMs` feeds LCD progress/time, LED progress and telemetry. Raw `elapsedMs`
remains unchanged for reporting. No allocations, polling increase, protocol
change, NVS migration or Tab5 change is needed.

Duplicate samples only refresh packet freshness. Ordinary new samples preserve
position at receipt and repay phase error at a bounded ±12.5% clock rate; small
older cached positions are ignored. Seek flags, playback transitions, track
identity changes, reconnect/stale recovery and inferred >=2-second position
jumps reanchor immediately. Repeated source stalls cap extrapolation at three
seconds; connection freshness expires at five seconds. Same-track metadata
refreshes do not reset time. Track changes await the next position sample.

Tests replay 1 Hz MPD / 2 Hz BLE plus extra ACKs at 25 ms render intervals and
assert nondecreasing position and ring coverage through ordinary playback.
They also cover positive/negative corrections without receipt jumps, uneven and
delayed samples, pause/resume, bidirectional seeks, same-track repeat, metadata
refresh/new tracks, buffering/stalls, unavailable state, end clamping, reconnect
and millis rollover. Frozen deployment/sampling evidence is under ignored
`backups/smooth-progress/release/`.

Live 0.6.4 verification confirmed boot, encrypted feedback, unchanged scene/General
settings and unchanged Tab5 application hash with MPD/SD healthy. Sampling made
**35 ordinary-playback comparisons** without a backward display step, including
**3 repeated raw positions** while the display clock continued forward. One
captured pair retained raw `60066` ms while display position advanced from
`59927` to `60973` ms. Refresh remained **19.4 fps**; no panic was recorded.
These are software position samples, not a physical high-speed LCD/LED recording.

### Button 2 missed double presses (0.6.5 diagnostic / 0.6.6 fix)

The physical GPIO trace showed stable presses/release gaps of about 46–106 ms,
with Button 2 sampling gaps commonly 37–40 ms and up to 60.6 ms during LCD
refreshes. Some real 50–65 ms presses were sampled down once and released before
a later observation could confirm 25 ms debounce. They disappeared before click
classification, explaining Single or no event despite two physical presses.
Captured switch chatter was sub-millisecond; lowering debounce was unnecessary.

A separate simulation reproduced a click-deadline bug: a second press starting
inside the 300 ms window could finish debounce after it and produce two singles.
The recognizer now retains a timely candidate through debounce/bounce and uses
its onset time; a rejected glitch clears the candidate after stable release.

`InputWorker` owns GPIO/PCNT sampling, ChordEngine and RotaryDetents in a 4096-byte
priority-3 task on the Arduino core, targeting 1 ms intervals. Rendering, BLE,
HTTP, NVS and action dispatch remain outside that task. A 64-entry RTOS queue
contains generation-tagged gestures; the main loop rejects stale generations
on settings/scene changes. Worker-owned press/activity counters preserve short
cycles even while main is busy. Sleep/wake, encoder detents, ordered modifiers,
Global bindings and conditional gesture delays retain their existing policies.
OTA pause disables recognition bindings and discards pending actions; queue
saturation reports a counter and cancels pending remote holds before resync.

`InputTrace` retains 256 Button 2 raw IRQ/sample/debounced/gesture/gap records in
RAM, without serial formatting or flash writes in the capture path. Reading it
copies a bounded snapshot. Clearing it resets worker sampling timing statistics.
The diagnostic 0.6.5 app deliberately kept the original timing/behavior.

The production worker test replays all eight complete double-press pairs from
the physical capture through its actual run loop at 1 ms while main consumes no
events; all eight become Double and all 16 presses are counted. Additional RTOS
boundary cases cover generation changes, held modifiers across reconfiguration,
OTA-style pause/resume without replay, queue saturation/discard, encoder
press-turns, and allocation/task-start failures. Gesture tests cover timely,
bouncing, rejected and genuinely late second presses at the 300 ms deadline.
Both firmware environments and the complete host suite pass. Evidence is under
ignored `backups/button-double-investigation/`; the sanitized GPIO capture is
checked in as `tests/fixtures/key2_double_trace.json`.

### Per-action sound (0.6.7)

Every ordered binding action now has an independent Sound switch in its editor
header and a Sound on summary in the binding table. It survives editing,
duplication and reordering. The API returns a boolean `sound` and accepts
`aN_sound=0|1`; omission on a complete binding replacement means off. Choosing
Play tone in the editor enables its switch; other new actions begin off.

Scene schema 6 retains the 3304-byte record and 12-byte action layout by using
bit 7 of the action's LED byte for audio. LED validation, rendering feedback and
JSON use the masked LED effect. Reserved bits remain invalid. The v5 migration
validates the entire old record before applying any flags, preserves every
other field, enables existing Tone actions, and transfers General's current
scene-cue preference to existing scene actions once. The v1–v4 migrations use
the same rules. Recovery images must support scene schema 6 to preserve scenes.
General's record and the Bluetooth protocol are unchanged.

SceneService gates audio once per action, including Tone and scene changes.
Ordinary actions use 660 Hz for 50 ms; Tone uses its selected frequency for
200 ms. Scene actions retain 880/440 Hz for 90 ms and suppress the separate
General scene cue to avoid duplicate audio or an Off action still beeping.
Unchanged/failed scene changes are silent. General's scene-cue switch applies
to web/API selection; its sound volume controls all generated cues. The existing
nonblocking audio task handles requests, without delaying the input worker.

Both firmware environments and the host suite pass, including sound gating,
wraparound direction, LED isolation, strict flag validation and legacy
migration. An offline browser check covered independent switches in three
operations (edit, duplicate and reorder), save/reopen/reload, and 320–1440 px
layouts with no JavaScript errors or external asset requests. Live saved-binding
execution verified ordinary, Tone, Next, Previous and direct scene actions both
on and off, correct frequency/duration/volume, silent unchanged selection, and
rejection of invalid flags without modifying saved settings. Evidence and the
frozen application are under ignored `backups/action-sound/release/`.

A reboot retained three independent on/off/off action flags and unchanged General
settings. The temporary scene was removed and the original active scene restored.
The application MD5 matched the frozen image, boot attempts cleared to zero,
Bluetooth feedback reconnected, and audio I/O errors and input queue drops stayed
at zero. Display refresh remained 19.7 fps.


### Modifier multi-select (0.6.8)

The Bluetooth keyboard editor replaces the decimal modifier input with a local
multi-select dropdown. Eight left/right Control, Shift, Alt/Option and GUI
choices toggle independently. Selected values appear as removable tags; an
empty selection displays None. Binding summaries use modifier names. The same
`target` byte is saved, preserving all existing masks and NVS schema 6.

The control uses native buttons and checkboxes, closes on outside click/focus
or Escape, and supports arrows/Home/End plus Space for keyboard selection.
Escape closes an open dropdown before the binding dialog. No Bootstrap JS,
CDN or additional library is required. Independent action editors keep their
own selection; opening one closes the other menus. Browser checks cover all
eight modifier bits combined, clear to zero, tag removal, keyboard controls,
save/reopen/reload, duplicate/reorder and offline 320–1440 px layouts.

Both firmware builds passed. Application-only OTA matched the frozen image MD5,
cleared boot attempts, and reconnected Bluetooth feedback. All saved scenes,
bindings and General settings matched the pre-update snapshot. The device-served
dropdown was exercised on desktop and mobile without submitting any settings
writes. Evidence: ignored `backups/modifier-select/release/`.


### Title-side notifications (0.6.9)

Settings panels, the scene editor, binding/WiFi/scene-name dialogs, log view and
firmware updater reserve notification space to the right of their title. Empty,
short and long messages keep the same dimensions. Long messages scroll within
the reserved area instead of resizing the panel. The binding's conflict and
Global-binding notices share its title area; all source text and save-blocking
rules are retained. Local operation results route to the corresponding panel or
open dialog instead of appearing behind the modal. No settings schema changes.

Offline browser checks at 320/375/768/1440 px compare dialog bounds, form fields,
action-list position, and save-button bounds through empty/unsaved/error and
warning states. They also compare panel/updater geometry through long messages
and exercise real save handlers with mocked success/error responses. No changes
in geometry, JavaScript errors or external asset requests were observed.

Both firmware environments built successfully. Application-only OTA matched the
frozen MD5, cleared boot attempts and reconnected Bluetooth feedback. Scenes,
bindings and General settings matched their pre-update snapshots. A browser
using the actual device compared desktop/mobile dialog and control geometry
through unsaved and long-error states, then cancelled without settings writes.
Evidence: ignored `backups/title-notices/release/`.


### Scene tab borders (0.6.10)

The scene strip overlaps the panel border by one pixel. The selected tab's
bottom border matches the panel background, joining the tab to its content;
inactive tabs retain the shared border. Horizontal scrolling remains available
without a scrollbar separating the tabs from the panel. A browser check covers
first/later selected tabs at 320/375/768/1440 px, matching border/background colors,
one-pixel overlap and no page overflow. No settings or action behavior changed.

Both firmware builds passed. Application-only OTA matched the frozen MD5;
boot attempts cleared and Bluetooth feedback reconnected. Saved scenes,
bindings and General settings matched their snapshots. The device-served page
confirmed the selected-tab join on desktop/mobile without settings writes.
Evidence: ignored `backups/scene-tabs/release/`.


### Modifier row selection (0.6.11)

Clicking modifier text could focus the containing dialog during pointer-down.
The generic focus-out handler immediately closed the menu, so pointer-up landed
on the form underneath and no checkbox toggle occurred. Focus on the dialog
itself is now ignored by that handler; actual outside controls and clicks still
close the menu. Labels explicitly forward text/padding clicks to the checkbox,
with direct checkbox clicks excluded to prevent double toggles. Selected rows
are highlighted and remain synchronized with removable tags and the saved mask.
No settings schema or BLE protocol change is required.

Browser checks click each modifier’s text, row padding and checkbox, covering
all eight bits and verifying one toggle per click, selected highlights, keyboard
Space, saving and reopening. Mouse at 375/1440 px and touch text taps at 375 px
pass. Both firmware environments build successfully.

Application-only OTA matched the frozen MD5; hardware was ready, boot attempts
cleared and Bluetooth feedback reconnected. Existing scenes/bindings and General
settings matched their snapshots. A browser using the device-served page clicked
modifier text, checked highlights/tags and cancelled without any settings writes.
Evidence: ignored `backups/modifier-rows/release/`.


### Custom dropdowns throughout configuration (0.6.12)

A local browser component now draws every visible single-select dropdown:
USB mode, timezone, General lighting, scene screens/lighting/brightness,
binding input/event/modifier, commands and command-specific choices. The
existing modifier multi-select and scene icon picker remain custom controls.
Hidden native selects preserve FormData, saved values and existing input/change
handlers. Programmatic value assignment, rebuilt options/optgroups and disabled
fieldsets synchronize the visible trigger; action rebuilds restore its focus.

Menus live in their owning dialog or the document and use viewport positioning,
with automatic above/below placement. Keyboard focus scrolls only the option
list, preserving page position. Outside focus/click and Escape close menus;
Tab resumes form navigation. Longer lists include local search and retain group
headings. Rendering, search and event handling run entirely in the browser.
The extra JavaScript is about 3.4 KB gzip in flash; no library/CDN, new device
polling, NVS schema or Bluetooth protocol change is introduced.

Offline browser checks at 320/375/768/1440 px cover every static/dynamic select,
timezone and command groups/search, no-match results, value synchronization,
disabled fieldsets, keyboard/Escape/Tab, popup bounds, modifier-menu coexistence,
scene saves, ordered binding serialization and reopen. A mobile touch context
is included. No JavaScript errors or external resource requests were observed.

Both firmware environments built successfully. Application-only OTA matched
the frozen MD5, cleared boot attempts and reconnected Bluetooth feedback.
Saved scenes/bindings, General, NTP/timezone and USB settings matched their
pre-update snapshots. The device-served controls passed timezone search, grouped
commands, dynamic Bluetooth parameters and desktop/mobile popup checks; edits
were cancelled with no settings writes. Evidence: ignored
`backups/custom-dropdowns/release/`.


### Plain interface labels (0.6.13)

Removed middle-dot separators from all authored interface strings: page titles,
labels, action groups, step choices, summaries, tooltips, status/diagnostic text
and the updater. Wording uses plain labels, contextual descriptions and normal
sentences. The connection footer displays its existing values on separate lines.
Related guide labels and the web wording policy are updated. User-provided
scene names and track metadata remain unchanged; no settings migration occurs.

Both builds and desktop/mobile dropdown checks passed. Application-only OTA
matched the frozen image, cleared boot attempts and reconnected Bluetooth.
The device-served script and page title contain no middle-dot separators.
Scenes/bindings and General settings matched their pre-update snapshots; live
UI edits were cancelled without settings writes. Evidence: ignored
`backups/plain-labels/release/`.


### Scene editor layout and HID key search (0.6.14)

Scene identity moves into the heading beside the title. Idle screen/ring display
share a desktop row, with ring/button color controls on the next row at equal
width. On narrow screens they stack. This removes the old 70 px icon grid track
that the fifth (button-color) setting could wrap into and collapse its dropdown.
The existing icon picker, status area, brightness channels and form IDs remain.

Keyboard HID usage is a custom searchable dropdown with 141 named keyboard-page
keys, grouped by letters, numbers, navigation/editing, symbols, functions,
numpad, system and international keys. Values follow the bundled TinyUSB HID
constants. Labels include decimal usage; search also matches aliases and hex.
Multi-word search matches all words. Binding summaries show the key name.
A Custom HID code option keeps the existing 1–231 range and displays unlisted
saved usages automatically. Its temporary UI mode uses a WeakSet, leaving the
saved action shape unchanged; key choice preserves the modifier mask.

Browser checks cover static ring/button widths and the relocated icon at
320/375/768/1440 px; the 32-icon picker still updates the scene draft. HID checks
cover named/alias/decimal/hex searches, modifiers, custom bounds, rerenders and
save/reopen on mouse and touch. Existing custom-dropdown regression checks
pass for General, scenes and ordered actions. No settings schema change.

Both firmware builds passed. Application-only OTA matched the frozen MD5,
cleared boot attempts and reconnected Bluetooth feedback. Existing scenes,
bindings and General settings matched the pre-update snapshots. The served page
passed static-color width/icon checks and HID name/alias searches, custom-code
retention and desktop/mobile popup checks. All live edits were cancelled with
no settings writes. Evidence: ignored `backups/scene-editor/release/`.


### Source-key action lighting and header status (0.6.15)

Action light feedback now targets only the binding input. Button 1–4 use their
own LED, combinations use all participating buttons, and every knob gesture
uses the ring. OPEN/REC have no associated LED. The four key overlays and ring
have separate deadlines, so unrelated effects can overlap and expire without
resetting one another. Screen feedback remains independently timed.

The editor offers On using scene lighting, Off, static color, rainbow and
percentage feedback for commands returning a percentage. Brightness either uses
the resolved scene/General level, including Auto, or an absolute 0–100%
override. The existing 250–10000 ms duration controls the feedback lifetime.
Proportional ring feedback fills segments; a button scales only its own light.
Sleep still forces all outputs off. Explicit commands changing the device's
lighting settings keep their existing group behavior.

Scene schema 7 packs action brightness in the unused high byte of RGB: 0–100
are explicit percentages and 101 means inherit. The sound flag stays in the
LED byte; API RGB stays 24-bit. Action and Stored remain 12 and 3304 bytes.
Version 6 migrates to inherited brightness without changing RGB, sound,
commands, bindings, scenes or active selection; older migrations chain through
it. Recovery firmware must support schema 7.

Connection, standby screen and last input now appear together in the top
header. Fixed-height cells reserve space before polling; long text can scroll
and has a tooltip. The mobile header uses two columns with last input below.
The old sidebar and key-binding heading placements are removed.

Host checks cover source masks, overlapping expiry, combinations, rollover,
scene/override brightness, all light modes, remote percentage availability and
legacy migration validation. Both production and USB-experiment builds pass.
Browser fixtures verify source labels, brightness/color/duration save/reopen,
mouse/touch controls and fixed header geometry at 320/375/768/1440 px.
Frozen artifacts and live evidence are in ignored `backups/action-lights/release/`.

Live API checks confirmed 80% red on Key 1 while other keys/ring stayed at
scene levels, independent Key 2 off/expiry, a simultaneous 60% green knob
ring, and cyan feedback on both members of Key 1+3 only. On at 0/100%,
inherited scene brightness, invalid-input rejection and duration expiry passed.
The complete saved scene/action definitions and General matched after a
controlled restart. Physical inputs switched scenes during the post-restart
output check, so that check did not qualify a steady idle baseline.
The 500 ms fixture was too short for reliable HTTP-relay sampling; output
checks used 3-second cues instead. The portable resolver tests cover short
lifetimes independently of network latency.

The device-served page passed header placement and fixed-geometry checks at
320/375/768/1440 px, plus source labels, brightness/duration retention and
cancelling edits with no settings writes or JavaScript errors. A visible
red/green/blue/white pattern on Key 1/2/3/4 was confirmed by the user on
2026-11-14, validating the custom firmware's slots 9/10/11/12 respectively.
This does not establish the ring's physical direction/origin.

Final verification matched the frozen running MD5 with hardware ready, boot
attempts cleared, fresh Bluetooth feedback and zero input-queue drops, audio
I/O errors or LED SPI errors. Both temporary scenes were removed; all original
scene definitions, active selection and General settings matched the pre-OTA
snapshot, with inherited brightness as the only added action field.


### Symmetric ring fills (0.6.16)

Scene Ring fill style selects Clockwise or Bottom to top (symmetric), applying
to Bluetooth playback progress and knob proportional feedback, including
Bluetooth volume. Existing scenes use Clockwise. The shared RingFill helper
keeps playback and action coverage consistent, without changing timing or BLE
volume/position state. Per-button proportional effects retain their intensity
behavior. Sleep and stale feedback handling remain in the existing paths.

A single red slot-0 check was observed at 12 o'clock by the user on 2026-11-14.
For the nine-pixel ring, symmetric fill starts with slots 4/5 at the bottom,
then 3/6, 2/7 and 1/8, reaching top slot 0 last. Paired pixels share partial
intensity equally. At 0% all are dark; at 100% all are full. Coverage is monotonic
and total intensity follows the percentage to within one permille-pixel of
integer rounding. No floating point or frame buffer is added to firmware.

ring_style uses a formerly zero reserved scene byte; schema stays 7 and the
record stays 3304 bytes. The API accepts clockwise/bottom_to_top, validates
the enum atomically and reports it in scene and state objects. The earlier
0.6.15 implementation ignores this reserved byte if used as a recovery image.
The scene page places idle screen/ring display/style across a desktop row,
with ring/button colors at equal widths below; narrow layouts wrap or stack.

Host tests sweep all 1001 progress levels for symmetry, bounds, monotonicity,
total intensity and 0/100% endpoints, alongside unchanged clockwise coverage,
rotated origins and even-count geometry. The production action resolver tests
cover Bluetooth-returned percentages with the same helper. Scene validation
checks style defaults, allowed values and unchanged storage size. Both builds
and the host suite pass. Browser fixtures pass style save/reload, per-scene
isolation, clockwise round trips and layout at 320/375/768/1440 px, including
touch input. Frozen application/ELF and evidence: ignored
`backups/ring-styles/release/`.

Application-only OTA matched the frozen MD5, with hardware ready and the boot
guard cleared. Hardware frame checks at 0/10/50/90/100% matched symmetric
coverage exactly, leaving all key LEDs unchanged; clockwise 50% remained
unchanged. A live BLE playback sample at 624 permille matched the symmetric
fill, including its partially lit boundary pair. Invalid styles were rejected
without changing settings. A controlled restart retained the style and every
scene/action/General field. The temporary scene was removed and the original
active scene restored, with zero input-queue drops, audio I/O errors or LED
SPI errors and fresh BLE feedback.

The device-served page also passed 320/375/768/1440 px header/control checks,
custom style-menu placement and dirty/reload behavior with no settings writes
or JavaScript errors.


### Remove ambient-light feature (0.6.17)

The unit has no established usable ambient sensor exposed outside its case.
ADC2/GPIO16 configuration and sampling, sample smoothing, calibration conversion,
automatic scaling and light-related counters/state are removed from production
firmware. The board header no longer assigns GPIO16 a light-sensor role.
ADC1 supply estimation and charge telemetry remain separate.

General and scene Auto checkboxes, sensor readings/range/capture/calibration,
associated browser state and polling are removed. Status publishes no light
or Auto/calibration fields. Retired form parameters return HTTP 400 before
applying any settings. Manual screen/ring/key levels, per-scene Use General
or Override, action brightness, ring styles, blank/sleep and wake behavior
remain. The Bluetooth switch keeps its inline checkbox styling.

No NVS migration or reset is needed: lighting v1 stays 44 bytes with retired
words reserved, and scene v7 stays 3304 bytes with the former Auto byte reserved
in each four-byte level. Validation/resolution ignores these bytes, so previously
enabled Auto flags cannot dim output. Manual values, mode/color, scene/action
definitions and all unrelated records are preserved. The old code paths in
earlier release sections above are historical; Auto is no longer available.

Host tests check manual defaults/bounds, actual legacy word offsets with Auto
enabled, ignored obsolete bytes and scene-level inheritance/override with the
legacy Auto byte set. Both builds and the host suite pass. Browser fixtures
pass at 320/375/768/1440 px, including touch, with every ambient/Auto status
field absent. General slider requests and scene save/reopen contain only manual
brightness fields; Bluetooth and ring-style controls remain usable.

Application-only OTA matched the frozen MD5, with hardware ready and cleared
boot attempts. Live API tests rejected every retired General/scene parameter
atomically, exercised independent General levels 37/52/68% and scene overrides
19/31/47% against exact composed LED frames, and checked Use General restores
selected levels. A restart retained manual selections, overrides and all
scene/action/General fields without light telemetry returning. Original levels,
active scene and definitions were restored and the temporary scene removed,
with zero input-queue, LED SPI or audio I/O errors. Frozen artifacts/evidence:
ignored `backups/manual-brightness/release/`.

The device-served page passed General/scene manual-control, sensor-UI absence,
Bluetooth/header and reload checks at 320/375/768/1440 px with zero settings
writes or JavaScript errors. Final status confirmed the expected running MD5,
cleared boot guard, hardware ready and fresh BLE feedback.


### Connection footer restored (0.6.18)

Ready/version and IP return to the bottom left of the sticky desktop sidebar,
using the existing connection ID and status polling. Standby screen and last
input remain in the top header, now with two columns. The mobile header stacks
these two fields and retains the earlier compact navigation behavior with the
sidebar footer hidden. Connection text has a fixed two-line scrolling area,
so long unavailable/error text cannot shift the navigation. Header status also
keeps its reserved height. No API, lighting or NVS behavior changes.

The production build and browser layout checks pass at 320/375/768/1440 px,
including live version/IP rendering, footer position and unchanged geometry
with very long status strings. Evidence: ignored `backups/connection-footer/`.

Application-only OTA matched the frozen MD5 and reached ready with cleared
boot attempts, fresh BLE feedback and zero input/audio/LED errors. The served
page passed footer/header placement and live version/IP checks at all four
widths, without JavaScript errors or settings writes. Ring color/display/style
were edited externally during the test window; the latest values were retained.
Release evidence is in `backups/connection-footer/release/`.


### Scene order and compact header (0.6.19)

The arrow controls beside + Scene move the selected tab without activating it
or discarding its unsaved settings draft. New scenes append even when they
reuse a lower numeric ID. Default can move while its name/delete protection
remains. API scenes are returned in saved order, driving tab navigation and
target menus. Next/Previous follow this order with wraparound. Explicit switch
targets and all bindings keep stable scene IDs. Direct-selection tones use
displayed positions; explicit Next/Previous retain their wraparound direction.

SceneOrder stores a versioned 12-byte permutation of all eight IDs in the
existing scenes namespace's order key. Missing/corrupt order falls back to
numeric IDs without resetting scene definitions. Unused IDs are filtered out
at display/navigation time. Creation moves the unused ID to the tail and saves
order before saving the new scene; a failed scene write therefore cannot
reorder existing visible scenes. Deletion needs only the normal scene write.
The 3304-byte settings schema and other NVS records are unchanged. Reorder
validates the ID/position and saves before applying. order_stored reports the
order record separately from settings. Reorder updates epoch/revision without
activating a scene or producing a scene-change tone.

The desktop header now aligns page title, a two-line active-scene block and a
two-line standby/input block. Active scene has a subtle divider instead of a
boxed badge. Fixed heights and ellipsis/tooltips retain compact geometry.
Mobile keeps title/active scene together and moves device status below.
Connection/version/IP stay in the desktop sidebar footer. The Sound and
Bluetooth explanatory paragraphs were removed without changing controls.

Both builds and the host suite pass, including scene-order moves, invalid
permutations/positions, wraparound, sparse IDs, append/reuse, Default protection,
unchanged settings/binding bytes and cue direction. Browser fixtures pass at
320/375/768/1440 px (including touch), checking ordering, retained drafts, save
failures, append/delete, keyboard navigation, header alignment, long status
strings and removed paragraphs without JS errors or horizontal overflow.

Application-only OTA matched the frozen MD5 and reached hardware ready with
cleared boot attempts. Temporary scenes verified append/reuse, reorder without
activation, invalid-position rejection, Default delete rejection, Next/Previous
wrap and stable explicit targets. Restart retained the order and temporary
bindings. Cleanup deleted only temporary scenes, retaining original scenes'
relative order and any current user edits, with zero input/audio/LED errors.
Frozen artifacts/evidence: ignored `backups/scene-order/release/`.

The device-served page passed order-control/tab-order, inline two-line header,
footer and removed-help checks at 320/375/768/1440 px without JavaScript errors
or settings writes. Final status confirmed the expected MD5, hardware ready,
cleared boot guard, fresh BLE feedback and saved scene order.

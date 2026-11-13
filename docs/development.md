# Development baseline

Hardware bring-up is complete. The current code is a working foundation for
feature development; this document describes its implementation and evidence
limits. User-facing behavior belongs in the [firmware guide](firmware.md),
requests in the [API](api.md), and build/update procedures in [Flashing](flash.md).
The proposed Serial/Bluetooth action categories, Tab5 source review, pairing
and bidirectional media-state contract are in the
[Tab5 integration plan](tab5-integration.md).

## Source layout

| File / area | Responsibility |
|---|---|
| `firmware/src/main.cpp` | Startup, WiFi/NVS, shared HTTP routes, serial commands, OTA and recovery orchestration |
| `firmware/src/hardware_demo.cpp` | GPIO/encoder sampling, Clock rendering, local action dispatch, lighting/ADC and hardware telemetry |
| `firmware/src/scene_service.cpp` | Versioned NVS scenes, atomic CRUD/bindings, activation, ordered dispatch and temporary effects |
| `firmware/include/scene_settings.h` | Bounded scene/action schema, validation, protected Default and reference retargeting |
| `firmware/include/gesture_engine.h` | Independent nonblocking click/hold/turn recognizers with cancellation |
| `firmware/include/gesture_policy.h`, `chord_engine.h` | Binding-aware recognition timing and reusable held combination members |
| `firmware/src/general_service.cpp`, `firmware/include/scene_cue.h` | Persistent General preferences and direction-specific scene sounds |
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

The main loop services HTTP, OTA, NTP and input/peripheral work. Display targets
are 5 fps for Clock; LED frames target about 30 Hz, ADC sampling every 500 ms and browser polling every second. Audio uses
its own task. Updates pause hardware/audio processing while writing flash.
Startup and the initial 30-second run window participate in the failed-start
recovery guard described in [Flashing](flash.md#recovery).

| NVS namespace | Record | Behavior |
|---|---|---|
| `wifi` | `ssid`, `pass` | Seeded from `secrets.h` only when no stored SSID exists; web/serial changes restart |
| `clock` | Versioned `settings` blob, 804 bytes (v2; migrates 296-byte v1) | Three NTP servers and supported zone committed together; apply and sync immediately |
| `lighting` | Versioned `settings` blob, 44 bytes | Independent levels/Auto flags, mode/color and calibration; web saves before apply; knob writes debounced 1.5 s |
| `usbnet` | Versioned `settings` blob, 12 bytes | Mode, prefix, device/host addresses saved together; restart to apply |
| `general` | Version-2 `settings` blob, 12 bytes (migrates 8-byte v1) | Scene-change sound and sleep timeout in minutes (default 60); save before apply |
| `scenes` | Version-3 `settings` blob, 3304 bytes | 8 scenes, 64 sparse gesture bindings, 3 actions each; active selection committed with config |
| `demo` | Image MD5 and failed-start attempts | Recovery state, not user configuration |

Invalid/missing settings blobs use their declared defaults. Normal OTA preserves
NVS. A full factory-image restore overwrites it. Awake state, speaker volume,
last event and temporary feedback are runtime state. The active scene and all action definitions/effects are saved settings.
`auto_screen`, `auto_ring` and `auto_keys` control automatic brightness; the
old page tour is retired.

The light controller smooths successful raw samples and uses calibrated endpoints
of either polarity. It scales each selected level independently and falls back
to selected manual levels after five seconds without a good sample. Voltage
conversion, supply scaling and remaining GPIO roles in custom code are working
implementation choices; unrecovered analog topology is not documented as a
factory finding.

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
| `lighting_settings_test.cpp` | Defaults, settings validation, calibration polarity/clamps, independent levels and missing-sensor fallback |
| `factory_led_frame_test.cpp` | All channel bytes against the extracted factory table, GRB order, slot capacity and reset padding |
| `rotary_detents_test.cpp` | Detents, partial turns, reversals, bounce and batched edges |
| `clock_settings_test.cpp` | Hostname/fallback validation, legacy migration, timezone offsets and daylight saving |
| `scene_settings_test.cpp` | Protected Default, ordered bindings, deletion/retargeting, corrupt storage and capacity |
| `action_feedback_test.cpp` | Independent screen/LED lifetimes, replacement, inheritance, clear and timer wrap |
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

The gesture engine takes raw GPIO states and decoded rotary detents; it queues
recognized events without blocking the HTTP/audio loop. The hardware layer
passes those to SceneService, which resolves the active scene's saved binding
and executes ordered actions through a local hardware handler. Scene switching
ends that list, advances the configuration epoch and cancels pending gestures.
The recognizer preserves held members as combination anchors; a fresh trigger
press is required in the next scene.
No input mappings are hardcoded in the polling loop beyond boot recovery.

Percentage-producing actions return values to the feedback controller. Clock
screen overrides and LED effects have independent, bounded lifetimes and fall
back to the scene standby/General lighting. Proportional ring output uses a
fractional final segment and respects ring brightness; key LEDs retain their
General output for this effect. No computer volume or external data source is
fabricated: the future companion app will supply those integrations.

NVS writes are bounded by one fixed scene record and use save-before-apply for
configuration. Scene selection is saved immediately; rapid lighting adjustments
are debounced. The USB exact-old-default migration changes .21/24 + .30 to
.1/30 + .2 without replacing custom networks. Clock migration retains the old
primary/timezone and adds empty fallback slots.

## Verified baseline and open items

Recorded on **2026-11-13** for this unit:

- User-confirmed visible display, speaker, ring and four key LEDs; key/encoder
  events observed. Physical notch-by-notch confirmation after the decoder change
  remains distinct from its host tests.
- NTP clock framebuffer checked with/without the sync colon, and without date,
  timezone, seconds, status text or outer ring.
- Independent lighting levels, static/rainbow selection, calibration-driven Auto,
  invalid-request handling and settings persistence exercised over the live API.
  Shared web UI checked at mobile width without JavaScript errors.
- 256 KiB PSRAM pattern test passes; ES8311 ID `0x8311`, revision 1, responds at
  I2C `0x18`; microphone data streams without reported I/O errors.
- Light readings change and have successful/error counters. At consolidation,
  the live device reported hardware ready, a passing PSRAM test, no accumulated
  light/audio I/O errors, active USB Ethernet/link and a DHCP ACK.
- HTTP OTA verified the running application's MD5; USB enumeration, ECM link and
  the single DHCP lease were observed alongside display/audio/NTP operation.

These observations apply to the tested unit/image, not every C169 variant or a
long-duration qualification. Exact panel variant/readback, physical LED spatial
order/color calibration, charge transitions and battery state of charge remain
unestablished. The supply estimate is not an independently calibrated battery
measurement, and light values are not lux. Full factory restore boot is untested.

Factory analysis has **not** established connector contacts, module-pad wiring,
analog resistor/component identities or all peripheral GPIO roles. Hardware
reference documents record only the recovered binary findings. Remaining custom
pin names and scaling constants must not be treated as proof of board topology.

## Scene release validation (0.4.0)

The host suite covers model corruption/capacity and gesture timing, including
rotation during the knob debounce window and scene changes during a pending
press. Live API checks exercised protected Default, CRUD, atomic rejection,
ordered local actions, percentage ring feedback, and scene/config persistence
through reboot. Clock primary/timezone migration and the USB /30 migration were
checked against saved pre-update settings. Browser checks exercised both tabs,
scene creation, 5/9 gesture rows, action/effect editing and ordering, save/run/
activate/reload, and mobile width without JavaScript errors. These checks are
separate from companion-app integration, which has not been implemented.

## Table editor update (0.4.1)

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

## Per-scene lighting (0.4.2)

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

## Screen capture disabled (0.4.3)

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

## Compact Bootstrap configuration (0.4.4)

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

## Header notifications, UTC choices and RAM log (0.4.5)

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

## Readable typography (0.4.6)

Raised input/table text to 16 px, labels/buttons/log text to 14 px, and enlarged
headings, navigation and secondary text. Brightness columns and narrow-screen
scene fields allow for the larger text; notifications retain fixed header space.
Only presentation and version metadata changed; APIs and NVS are unchanged.

OTA verified running MD5 `95a2dfc4932b36aa3e27d1b16884cde7`. Live Chromium
checks confirmed the computed text sizes, overflow-free General/Scenes/Device
layouts from 320 to 1440 px, and stable panel positions during status changes.
No JavaScript errors were observed. The device passed boot confirmation with
scene definitions, active selection, lighting and clock settings preserved.

## Scene combinations, overrides, icons and Global bindings (0.5.0)

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

## WiFi summary and SSID editor (0.5.1)

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


## Scene sound and responsive inputs (0.5.2–0.5.3)

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


## Blank idle screen and inactivity sleep (0.5.4)

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

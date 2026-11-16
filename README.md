# LifeSmart/ColoLight ColoPlay (C169)

<img src="firmware/web/coloplay.svg" width="96" height="96" alt="ColoPlay project icon">

Custom ESP32-S3 firmware and hardware reference for the **LifeSmart/ColoLight
ColoPlay C169**. Factory firmware uses the name **ColoPLAY V1**. Hardware
bring-up is complete; this repository is the working baseline for feature
development. The build target and default hostname remain `coloplay`.

> **Compatibility and warranty:** This is a rewritten replacement firmware.
> It is not compatible with the factory firmware or factory software in any way.
> **Installing this firmware will void your device's warranty.**

## Current baseline

Current source: **`0.6.33-navigation-nvs`**. Tab5 owns the selected volume scale:
**1/16 Squared Curve**, **1/64 Squared Curve** (default), **2% Integer**, or
**5% Integer**. ColoPlay synchronizes it over BLE and predicts immediate
screen/ring response. Small curved hardware readback differences map back to
the same displayed scale position. Commands remain Up/Down only.
The [macOS volume findings](docs/tab5-integration.md#observed-macos-volume-scale)
explain raw mixer quantization and the squared control profiles.
The volume screen shows a speaker icon and bar; command outcomes
stay in Device diagnostics and the web log. General combines WiFi and USB
status in one USB & Network panel, with separate modal editors and grouped controls.
The dedicated [Bluetooth playback page](docs/firmware.md#bluetooth-playback)
configures separate Playing, Paused and Stopped/Disconnected light plans. Ring
and Buttons independently follow the scene or use custom colors, fills and
brightness. Screen elements, text scrolling and screen state fallbacks are
configured separately. Its settings
save independently to NVS; action feedback and sleep retain priority.
Saved scene order, manual brightness, symmetric ring fills, source-key
lighting and searchable HID keys remain available.
Both devices use application-only OTA. Source/build identity, validation results
and image fingerprints are in the
[release record](docs/development.md#current-verified-release).
The deployed 0.6.33 production image occupies **3.10 MiB**, leaving **1.40 MiB** of
headroom in either 4.5 MiB OTA slot. The embedded playback font uses **1.41 MiB**
of that image; see the [application storage budget](docs/flash.md#application-slots-and-storage-budget).
The working firmware provides:

- A plain Inter clock showing hours and minutes, with a colon only after NTP sync.
  Web screen capture and automatic preview refresh are disabled.
- Rainbow or static-color LEDs; separate screen, ring and key brightness controls.
  All three default to **70%** and save to NVS.
- Compact Bootstrap dark configuration: General, Scenes, Bluetooth playback and Device pages,
  served locally over WiFi or USB CDC-ECM. USB offers **CDC-ACM Serial + USB
  JTAG** (default) or **CDC-ACM Serial + CDC-ECM Network**, with no network-only
  mode. The active serial console provides commands and application logs.
  Includes a read-only WiFi/USB Network overview, modal SSID/password and USB
  mode/address editors, plus a web log viewer.
  Connection/version and IP sit at the bottom of the desktop sidebar; standby
  screen and last input share a compact header row with the page title and active scene.
  All dropdowns use local custom controls, with search for long timezone/action lists.
- Application-only HTTP OTA and ArduinoOTA through the preserved factory bootloader.
- Persistent scenes with customizable button/knob gestures, simultaneous button
  combinations, Global Default bindings and ordered actions. Unused multi-click/hold
  gestures add no delay; a held common key can anchor repeated combinations.
- Per-scene brightness overrides and a local 32-icon picker; scene switches
  briefly show the icon and name on the display, with optional direction-specific sound cues.
  Every binding action has a saved Sound switch using the General sound-effects volume.
  Action lighting targets its own key, all keys in a combination, or the knob ring,
  with on/off, color, brightness and independent feedback durations.
  Bluetooth keyboard modifiers use a dropdown with multiple selections and removable tags.
  Scene tabs can be moved left/right; new scenes append and Next/Previous follow
  the saved order. Default remains protected; idle screens can show Clock or Blank.
- Inactivity sleep turns off the display and LEDs after one hour by default,
  with a saved timer and wake on any button press or knob turn. Set the timer
  to `0` to disable it; existing custom timeouts are preserved during OTA.

The display, speaker, ring and four key LEDs have been confirmed on the unit.
Live checks have verified input events, PSRAM, microphone streaming,
saved settings, clock rendering, OTA and USB Ethernet DHCP. Evidence
and remaining limits are recorded in the [development baseline](docs/development.md#verified-baseline-and-open-items).
These are bring-up results, not a long-duration reliability qualification.
Ambient-light sensing and automatic dimming have been removed: a usable sensor
exposed to light outside the case has not been established.

## Bluetooth and Tab5

Enable **General → Bluetooth**, start pairing, then scan/select **ColoPlay**
on Tab5. A bonded BLE connection carries controls and authoritative MPD volume,
Playing/Paused/Stopped, track metadata, seek direction and position feedback.
The action editor groups Device and Bluetooth commands; volume offers
Up/Down commands using Tab5's configured grid, with separate volume/playback screens and
proportional ring feedback. See [pairing, actions and limits](docs/tab5-integration.md).

For direct navigation, choose **Scenes → + Tab5 navigation**, then **Activate scene**.
The new scene uses knob turn/press for item selection, Button 1 for Back,
Buttons 2/3 for previous/next view (hold to page repeatedly), and Button 4 for
the context menu. Its idle screen shows Tab5's current view and focused item
over BLE, returning to playback on Now Playing. Existing scenes remain intact.
Both updated applications are required for navigation feedback; the last
verified OTA is still recorded separately above.

The earlier **Tab5 BLE** scene supplies knob item navigation/Enter, Back and
modifier-first Page Up/Down and previous/next-menu combinations. The same two
buttons can perform different actions in reverse order. Playback uses a small
playback-state icon and centered title/artist with an offline 16 px font covering
common Japanese, 1,500 frequency-ranked GB2312 Chinese characters and 2,350 common
Korean syllables plus Jamo. Shared Han characters use the Japanese glyph;
unsupported characters use `?`. The LED-ring progress fill can run clockwise or
symmetrically from bottom to top. Connecting uses the
locally bundled Iconify `andrsrxn:bluetooth-connecting` icon without text.
General includes saved sound-effects volume for local tones and scene cues. Existing scenes and General settings remain intact.
Serial companion actions and a dedicated long-sequence editor remain future work.

## Documentation

Each current topic has one primary guide:

| Need | Guide |
|---|---|
| Use the clock, lighting, controls, WiFi and USB | [Firmware guide](docs/firmware.md) |
| Set up tools, back up, build, update or recover | [Flashing and recovery](docs/flash.md) |
| Integrate controls or read telemetry | [HTTP API](docs/api.md) |
| Use Bluetooth controls, Tab5 pairing and media feedback | [Tab5 integration guide](docs/tab5-integration.md) |
| Understand source, settings, tests and constraints | [Development guide](docs/development.md) |
| Inspect playback language coverage, font sources or regeneration | [Playback font guide](assets/fonts/noto-sans-jp/README.md) |
| Inspect recovered hardware assignments | [Factory-derived GPIO map](docs/gpio-map.md) |
| Inspect the original backup and factory restore procedure | [Factory firmware](docs/factory-firmware.md) |
| Reproduce recovered factory configurations | [GPIO evidence](docs/firmware-gpio-evidence.md), [LED transmitter evidence](docs/firmware-led-evidence.md) |
| Understand earlier bring-up decisions | [Bring-up history](docs/history/bring-up.md) |

The old [hardware-demo](docs/hardware-demo.md) and [clock-and-usb](docs/clock-and-usb.md)
paths remain as pointers. The [RAM-only LCD probe](firmware/lcd_probe/README.md)
is optional research material.

`coloplay` is the only firmware build target. The earlier
[USB experiment record](docs/usb-concurrency-test.md) is archived; its target,
HID hooks and experiment routes have been removed. Production USB modes remain
CDC-ACM Serial + USB JTAG and CDC-ACM Serial + CDC-ECM Network.

## Connect and build

For an already provisioned device, open `http://coloplay.local/` over WiFi, or
`http://172.32.29.1/` over USB Ethernet with its default address configuration.
The web home page contains all settings, diagnostics and the OTA link.

In a checkout with `firmware/include/secrets.h` configured:

```bash
./scripts/build_firmware.sh
./scripts/test_http_ota.sh
./scripts/ota_upload.sh http://coloplay.local
```

For a fresh checkout or first device flash, follow [Flashing and recovery](docs/flash.md).
Preserve the factory bootloader; the normal PlatformIO full-upload path is
incompatible with this unit's established flash layout. Raw backups and
`secrets.h` are local, ignored files and are not distributed with the repository.

## Hardware evidence

Hardware findings come from the captured factory image and its backup logs.
The detailed [GPIO evidence](docs/firmware-gpio-evidence.md) records executable
call paths, data addresses and dump offsets; the [GPIO map](docs/gpio-map.md)
is a compact index of those recovered values.

| Finding | Evidence |
|---|---|
| ESP32-S3, 16 MiB flash, 8 MiB PSRAM | Captured device logs; PSRAM also exercised by custom firmware |
| GC9A01 family display driver | Recovered factory initialization call/configuration |
| ES8311 audio path | Factory codec routines, I2C and I2S configuration |
| Six-key scan and rotary encoder | Recovered factory scan table and PCNT configurations |
| GPIO48 LED transmitter | Embedded Lua and native SPI3 encoder; fourteen software slots |
| Factory bootloader, partitions and application | Original full-image parsing and integrity checks |

The removed hardware drawings are not used to infer module pads, connector
pinouts, component identities or unrecovered peripheral roles. Custom sensor,
USB and output behavior is documented separately in the firmware/development
guides. The [module datasheet](docs/esp32-s3-wroom-1-datasheet.pdf) remains a
component reference, not evidence of the installed board's wiring or module variant.

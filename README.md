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

Current firmware: **`0.6.19-scene-order`**, committed as
[`18f3a79`](https://github.com/rGnng/esp32colo/commit/18f3a793fcc9749a0afcc0eb6c98f860f96f8659). It includes saved scene ordering,
manual brightness, symmetric ring fills, source-key lighting feedback and
searchable HID keys. Tab5 uses the recorded BLE integration built on
**`a34fa8a`** and its existing MPD volume logic.
Both devices use application-only OTA. The latest ColoPlay deployment,
validation results and image fingerprints are in the
[release record](docs/development.md#current-verified-release).
The working firmware provides:

- A plain Inter clock showing hours and minutes, with a colon only after NTP sync.
  Web screen capture and automatic preview refresh are disabled.
- Rainbow or static-color LEDs; separate screen, ring and key brightness controls.
  All three default to **70%** and save to NVS.
- Compact Bootstrap dark configuration: General, Scenes and Device pages,
  served locally over WiFi or USB CDC-ECM, plus selectable USB serial/JTAG.
  Includes SSID/status/RSSI/IP, an SSID/password editor and a web log viewer.
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
The action editor groups Device and Bluetooth commands; volume supports host
steps, **1/64** and **1/16**, with separate Bluetooth volume/playback screens and
proportional ring feedback. See [pairing, actions and limits](docs/tab5-integration.md).

The installed **Tab5 BLE** scene supplies knob item navigation/Enter, Back and
modifier-first Page Up/Down and previous/next-menu combinations. The same two
buttons can perform different actions in reverse order. Playback uses a small playback-state icon, centered title/artist with a 16 px Japanese
font, and a configurable LED-ring progress fill: clockwise or symmetric bottom to top. Connecting uses the
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
| Inspect recovered hardware assignments | [Factory-derived GPIO map](docs/gpio-map.md) |
| Inspect the original backup and factory restore procedure | [Factory firmware](docs/factory-firmware.md) |
| Reproduce recovered factory configurations | [GPIO evidence](docs/firmware-gpio-evidence.md), [LED transmitter evidence](docs/firmware-led-evidence.md) |
| Understand earlier bring-up decisions | [Bring-up history](docs/history/bring-up.md) |

The old [hardware-demo](docs/hardware-demo.md) and [clock-and-usb](docs/clock-and-usb.md)
paths remain as pointers. The [RAM-only LCD probe](firmware/lcd_probe/README.md)
is optional research material.

The opt-in [USB CDC/HID/JTAG experiment](docs/usb-concurrency-test.md) includes
host checks and local recovery; it is separate from the production baseline.

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

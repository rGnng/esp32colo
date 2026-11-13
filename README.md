# LifeSmart/ColoLight ColoPlay (C169)

Custom ESP32-S3 firmware and hardware reference for the **LifeSmart/ColoLight
ColoPlay C169**. Factory firmware uses the name **ColoPLAY V1**. Hardware
bring-up is complete; this repository is the working baseline for feature
development. The build target and default hostname remain `coloplay`.

> **Compatibility and warranty:** This is a rewritten replacement firmware.
> It is not compatible with the factory firmware or factory software in any way.
> **Installing this firmware will void your device's warranty.**

## Current baseline

Source version: **`0.4.6-readable-ui`**. Documentation consolidated on **2026-11-13**.
Per-scene lighting was OTA-verified on this unit, including legacy scene
migration, independent mode/color saves and preserved General settings.
The working firmware provides:

- A plain Inter clock showing hours and minutes, with a colon only after NTP sync.
  Web screen capture and automatic preview refresh are disabled.
- Rainbow or static-color LEDs; separate screen, ring and key brightness controls.
  All three default to **70%**, with independent **Auto** checkboxes initially off.
- Ambient-light readings, calibration and automatic dimming; persistent settings.
- Compact Bootstrap dark configuration: General, Scenes and Device pages,
  served locally over WiFi or USB CDC-ECM, plus selectable USB serial/JTAG.
- Application-only HTTP OTA and ArduinoOTA through the preserved factory bootloader.
- Persistent scenes with customizable button/knob gestures and ordered actions.
  Default is protected; Clock is the first scene standby/action screen behavior.

The display, speaker, ring and four key LEDs have been confirmed on the unit.
Live checks have verified input events, PSRAM, microphone streaming, light
readings, saved settings, clock rendering, OTA and USB Ethernet DHCP. Evidence
and remaining limits are recorded in the [development baseline](docs/development.md#verified-baseline-and-open-items).
These are bring-up results, not a long-duration reliability qualification.

## Documentation

Each current topic has one primary guide:

| Need | Guide |
|---|---|
| Use the clock, lights, sensors, WiFi and USB | [Firmware guide](docs/firmware.md) |
| Set up tools, back up, build, update or recover | [Flashing and recovery](docs/flash.md) |
| Integrate controls or read telemetry | [HTTP API](docs/api.md) |
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

# Build, flashing and recovery

This is the primary procedure for the current firmware. For controls and
configuration see the [firmware guide](firmware.md). Earlier version records
are in [bring-up history](history/bring-up.md).
The latest tested version, source commit and image fingerprints are recorded
in [Current verified release](development.md#current-verified-release).

## Tool setup and build

Run from the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install platformio 'esptool>=5.4,<6'
if [ ! -f firmware/include/secrets.h ]; then
  cp firmware/include/secrets.example.h firmware/include/secrets.h
fi
# Edit secrets.h: WIFI_SSID, WIFI_PASSWORD, OTA_PASSWORD.
./scripts/build_firmware.sh
./scripts/test_http_ota.sh
```

The normal build also regenerates embedded web assets from `firmware/web/`
using Python only; it needs no npm, Sass or network access for those assets.
See the [web source guide](../firmware/web/README.md) for optional Bootstrap
subset changes.

Keep an existing `secrets.h` when updating an already provisioned device. The
installed firmware's OTA password must match the uploader's password. WiFi NVS
settings take precedence over the credentials compiled into a new application.

The application is `firmware/.pio/build/coloplay/firmware.bin`. Dependencies and
memory settings are pinned in [platformio.ini](../firmware/platformio.ini):
Espressif32 6.12.0 / Arduino 2.0.17, TFT_eSPI 2.5.43, Adafruit NeoPixel 1.12.3,
and `qio_opi` memory for N16R8. Building generates other PlatformIO artifacts;
only the application image belongs in the update workflows below.

## Normal updates

HTTP OTA is available on the same web server through WiFi or USB Ethernet:

```bash
./scripts/ota_upload.sh http://coloplay.local
# Or, with the default USB Ethernet address:
./scripts/ota_upload.sh http://172.32.29.1
```

Use the device's actual hostname/IP if changed. A local forwarded HTTP port
also works, for example `./scripts/ota_upload.sh http://127.0.0.1:10081` when
that relay has been set up. That port is not created by the firmware.

In a browser, open `/update`, log in as **admin** using **OTA_PASSWORD**, and
select `firmware.bin`. For a separately provisioned uploader, use
`--secrets /path/to/secrets.h` or the `COLOPLAY_OTA_PASSWORD` environment variable.
The uploader can also take an explicit application image as its second argument.
It does not print the password or pass it in the process argument list.

The script validates the S3 application descriptor, image checksum and appended
SHA-256, uploads with an MD5 header, then waits for the device's `sketch_md5`
to match the uploaded file. Confirm `hardware.ready` after the startup window,
then `recovery_mode=false` and `boot_attempts=0` after the 30-second confirmation
period. Check `running_partition` and a fresh uptime to establish that the
updated image booted, especially when reinstalling an identical image whose
MD5 already matched before upload.

ArduinoOTA remains available for installed firmware without HTTP OTA, including
the earlier `0.1.3-nvs` baseline:

```bash
./scripts/ota_upload.sh coloplay.local
```

ArduinoOTA requires UDP 3232 and a return TCP connection from the device to the
uploader on port 32320. `--host-ip` and `--host-port` override the callback
address/port. A port-80-only TCP relay cannot carry that protocol.

Both methods write the inactive 4.5 MiB application slot, then switch OTA
metadata and reboot. They preserve the factory bootloader, partition tables,
NVS, PHY and vendor data. An aborted HTTP upload leaves the running slot
selected. Successful upload verification is not automatic rollback if the
new application's runtime later crashes.

## First USB flash

Use this section only when establishing the custom firmware for the first time.
The established unit needs its **factory bootloader preserved**: a default
Arduino bootloader repeatedly reset on its flash chip (manufacturer ID `0x46`).
Do not substitute `pio run -t upload`, a merged image or an erase-flash operation.

### Enter ROM download mode

1. Disconnect USB and, if fitted, the battery so the ESP32 fully powers off.
2. Hold **REC / GPIO0** and press the **center knob / GPIO46**.
3. Reconnect USB with a data-capable cable while holding both controls.
4. For the backup waiting script, release when it prints `connected`.

The ROM USB Serial/JTAG interface has VID:PID `303a:1001`. Linux normally uses
`/dev/ttyACM*`; macOS uses `/dev/cu.usbmodem*`. These ROM download straps differ
from KEY1 application recovery. This button sequence was used for the captured
unit; it does not establish a connector pinout for another board.

### Preserve the original flash

Before the first write, start the read-only waiting script, then connect the
device in ROM download mode:

```bash
python scripts/wait_and_backup.py --timeout 600
```

It requires esptool 5.4 or newer and writes the full flash, SHA-256 and metadata
under ignored `backups/`. Keep an untouched copy outside the working checkout.
The original unit's 16 MiB capture and checksum are recorded in
[factory firmware](factory-firmware.md#备份); raw images are not distributed.

Once custom firmware is installed, a new full-flash dump is a **current-state
backup**, not a factory backup. Do not name it `coloplay-factory-*.bin`: the
bootstrap script chooses the newest matching filename automatically.

### Write the application

Ensure the newest matching local factory backup belongs to this device and is
untouched. Re-enter ROM download mode, then run:

```bash
python scripts/flash_bootstrap.py --timeout 600
```

| Flash offset | Bootstrap action |
|---|---|
| `0x0` | Preserve factory bootloader |
| `0x8000` | Write a copy of the factory partition table for Arduino |
| `0x10000` | Preserve original factory partition table |
| `0x11000`, `0x15000`, `0x17000` | Preserve NVS, OTA metadata and PHY |
| `0x20000` | Write the application into `ota_app0` |
| `0x4a0000` | Preserve `ota_app1` |

Each application slot is `0x480000` bytes. The bootstrap assumes the factory OTA
metadata selects app0 and does not reset slot selection. Use OTA for subsequent
updates, especially once the active slot has changed. `0x10000` is the factory
partition table, not an application address. The first application boot may
seed/update NVS even though the bootstrap itself preserves that partition.

## Verify a boot

```bash
curl http://coloplay.local/api/status
```

Check the source version, `sketch_md5`, `running_partition`, `recovery_mode`,
`hardware.ready` and `boot_attempts`. Hardware startup waits five seconds after the web server is
started; WiFi connection and early serial startup add their own time before
that. Successful hardware startup clears `boot_attempts` after 30 seconds.
Check the PSRAM result, peripheral errors and advancing microphone/LED counters.
Display frames advance while Clock or a scene overlay is visible; Blank and
sleep intentionally stop display refresh. Sleep reports zero effective screen,
ring and key brightness while leaving HTTP and USB available. The USB mode may
change after hardware startup.

In serial/JTAG mode the console runs at 115200 baud. In Ethernet mode it is an
early boot console and then yields the USB PHY to the network interface.
A USB link/DHCP check and a visible display/audio check complement telemetry;
a framebuffer or successful I2S transfer alone does not prove physical output.

## Recovery

Application recovery leaves WiFi, web and OTA available while skipping hardware
startup and retaining USB serial/JTAG. Enter it by holding **KEY1 / GPIO6** during
a normal reset. Two failed peripheral starts or failures in the first 30 seconds
of operation on the same image also trigger recovery on the next boot. The
retry counter is scoped to image MD5 and clears after 30 seconds of successful
operation. A different uploaded image gets a fresh retry budget.

After correcting a problem, authenticated `POST /api/hardware/retry` clears the
counter and restarts the same image. See the [API](api.md#recovery-and-reboot).
Retained `hardware.stage` / `hardware.last_boot_stage` and USB stage fields help
locate startup failures across warm resets. Recovery requires the application
to reach startup; it cannot repair an invalid bootloader or pre-setup failure.

For incorrect WiFi settings, use USB Ethernet if reachable. Otherwise hold KEY1
at reset for serial/JTAG recovery and use the 115200-baud commands:

```text
status
wifi <ssid> <password>
wifi-reset
```

`wifi-reset` clears the WiFi namespace and reseeds from `secrets.h` at the next
boot. It does not reset scenes, lighting, clock or USB configuration. There is no generic
reset-all-settings operation in the current firmware.

For the CDC/HID/JTAG experiment, prepare the portable known-good recovery bundle
first. Its [two-slot recovery procedure](usb-concurrency-test.md#recovery-preparation)
preserves settings and works regardless of the selected OTA slot.

## Factory restore and boot warning

The complete original-image write/verify procedure is maintained in
[factory firmware → restore](factory-firmware.md#恢复). It preserves the original
flash-header parameters and verifies bytes while still in ROM download mode.
A full restore overwrites applications, NVS and filesystems; it does not restore
eFuses. Full factory restore boot has **not** been tested on this unit.

The preserved factory bootloader reports `SHA-256 comparison failed` followed
by `Attempting to boot anyway`. Its specific header/hash difference is explained
in the [integrity analysis](factory-firmware.md#完整性与-bootloader-头部差异).
This unit has subsequently booted and completed OTA with that bootloader; that
finding does not generalize to other checksum failures.

## Bluetooth test recovery

Current builds (0.6.15 and later) write scene version 7 and General version 3
records. Use a version-7-capable image to preserve saved scenes during recovery.
Versions 0.6.7–0.6.14 used scene version 6; earlier images cannot read the new
per-action brightness byte.
The 0.6.1–0.6.6 builds used scene version 5 and cannot read the new action sound
flags. Prefer disabling Bluetooth over downgrading firmware.
The existing two-failed-start guard keeps WiFi/HTTP OTA available in recovery.
Send `POST /api/bluetooth` with form `op=disable`, then use the authenticated
`POST /api/hardware/retry` to restart hardware. Keep the same OTA admin password.
Local evidence/recovery images for the first test are under ignored
`backups/ble-first/`; Tab5's saved pre-test app is based on `a34fa8a`.
Only application images were changed; no C6, bootloader or partition update is
required. See [the integration guide](tab5-integration.md) for pairing again.

# LifeSmart/ColoLight ColoPlay (C169)

Hardware notes, factory-firmware analysis and custom ESP32-S3 firmware for the
**LifeSmart/ColoLight ColoPlay, model C169**. Manufacturer schematics and existing
firmware use **ColoPLAY V1**; build targets, scripts and the default network
hostname retain `coloplay`.

## Hardware and firmware status

The identified unit matches the published ColoPLAY V1 design:

| Part | Hardware |
|---|---|
| MCU/module | ESP32-S3-WROOM-1-N16R8 |
| Memory | 16 MiB flash, 8 MiB Octal PSRAM |
| Display | 240×240 round LCD; factory firmware selects the GC9A01 driver |
| Audio | ES8311 codec, CS8120 speaker amplifier, microphone |
| Controls | Four main keys, rotary encoder with push switch, OPEN and REC keys |
| Lighting | Ring and four key LEDs; factory SPI3 encoder sends fourteen RGB slots on GPIO48 |
| Boards | Main board, screen/encoder adapter and USB-C/power board |

A full factory backup was captured before writes. Offline analysis recovered
LCD, encoder, button-scan, I2C and I2S configuration from that image; see the
[factory GPIO evidence](docs/firmware-gpio-evidence.md). Other connections below
come from the schematics and are not all independently measured on the unit.

The earlier `0.1.3-nvs` WiFi/ArduinoOTA firmware is recorded as flashed and
OTA-verified in both application slots. The current source is `0.3.1-clock-usb`, which
adds display pages, buttons/encoder, RGB animation, audio/microphone telemetry,
sensor readings, a PSRAM check, browser controls and authenticated HTTP OTA.
The firmware has been deployed through HTTP OTA and verified by its running-image
MD5. The user confirmed the display, speaker, LED ring and four key LEDs work;
the four keys and knob also produce input events. PSRAM passes its pattern test,
the microphone streams without I/O errors, and supply/light readings update.
The LED transmitter was reconstructed from the original firmware: SPI3 at
3.2 MHz, fourteen RGB slots, GRB order and factory pulse timing. See the
[LED teardown](docs/firmware-led-evidence.md) and [demo guide](docs/hardware-demo.md).
The rotary decoder now consumes two quadrature edges per mechanical notch;
the former four-edge divisor caused one UI step per two notches. Partial
reverse movement is retained until a full notch is completed.

The clock uses embedded Inter fonts and NTP, with server and timezone settings
saved in NVS. Clock, USB and lighting controls share the existing web page.
USB Ethernet uses CDC-ECM for macOS, with device `172.32.29.21/24`, one DHCP
lease at `172.32.29.30`, and no gateway or DNS. USB serial/JTAG is a separate
selectable mode. See the [clock and USB guide](docs/clock-and-usb.md).

## Internal pinout

Numbers in the GPIO column are **ESP32 GPIO numbers**, not module-pad or connector
numbers. The [full GPIO map](docs/gpio-map.md) includes module pads, schematic net
names, boot straps and the factory button-scan order.

### Peripheral connections

| Peripheral / signal | ESP32 GPIO | Notes |
|---|---|---|
| LCD SCLK / MOSI | 14 / 13 | SPI; no separate MISO connection |
| LCD CS / DC (`LCD_RS`) | 12 / 11 | GC9A01 driver |
| LCD reset / backlight / TE | 45 / 18 / 17 | GPIO45 is also a boot strap |
| Main keys KEY1 / KEY2 / KEY3 / KEY4 | 6 / 1 / 4 / 5 | Active low; main-board S1 / S2 / S3 / S4 |
| Encoder A / B | 9 / 10 | `EAS` / `EBS` |
| Encoder push | 46 | `EKEY`; active low, external pull-up, boot strap |
| OPEN key | 15 | Power-board S2; active low |
| REC / download key | 0 | Power-board S1; active low, boot strap |
| RGB LED data | 48 | Factory `qt.init(48)`, fourteen-slot SPI3 frame; ring and key lights work |
| ES8311 I2C SDA / SCL | 42 / 41 | Codec control |
| I2S MCLK / BCLK / LRCK | 40 / 39 / 47 | Audio clocks |
| I2S ESP32 → codec | 21 | `AUD_SDOUT` → ES8311 DSDIN |
| I2S codec → ESP32 | 38 | `AUD_SDIN` ← ES8311 ASDOUT |
| Amplifier control / LED supply enable | 2 / 8 | `AUD_CTRL` / schematic net `MICBIAS`; GPIO8 switches VCCL and biases the light sensor |
| Supply-voltage ADC | 7 | `VOL_ADC`: VDD divided by approximately 3, not a volume knob |
| Ambient-light ADC | 16 | `LIGHTADC`; ADC2 reads can contend with WiFi |
| Charge status | 3 | `CHRG`; also a boot strap |
| Native USB D− / D+ | 19 / 20 | USB-C data path through the power-board ribbon |
| UART0 TX / RX | 43 / 44 | Internal P1 download header |

GPIO26–37 are reserved for the module's flash/Octal PSRAM connections; GPIO22–25
do not exist on ESP32-S3. Do not treat these numbers as spare expansion pins.
The factory six-key scan order is `6, 1, 5, 4, 46, 15`, so its indices 2 and 3
refer to KEY4 and KEY3 respectively.

### P1 download header and J1 USB debug connector

These are **schematic pin numbers** from the [main-board schematic](docs/coloplay-main.pdf),
sheet 1. Physical left/right orientation and pin-1 location must be checked on
the PCB before wiring.

| P1 pin | Signal | Connection |
|---:|---|---|
| 1 | VCC | 3.3 V logic supply rail |
| 2 | UART0_TX | GPIO43; connect to adapter RX |
| 3 | UART0_RX | GPIO44; connect to adapter TX |
| 4 | GND | Adapter ground |
| 5 | IO0 | GPIO0; pull low during reset for ROM download |

Use a **3.3 V TTL UART adapter**, with TX/RX crossed and a common ground. When the
device is powered through USB-C, leave the adapter's power lead disconnected.
P1 has no reset/EN or GPIO46 contact: use a full power cycle and hold the knob
pressed as well as REC to satisfy both download straps. The supplied bootstrap
script detects native USB only; it does not select an external UART adapter.

| J1 pin | Signal |
|---:|---|
| 1 | USB_5V |
| 2 | USB_P / D+ / GPIO20 |
| 3 | USB_N / D− / GPIO19 |
| 4 | GND |

J1 is labelled `DEBUG` in the schematic. The normal flashing connection is the
external USB-C port with a **data-capable cable**; no UART adapter is required.
Power-board J1 pins 7/8 carry D+/D−; main-board J3 pins 10/9 carry D+/D−.
Those ribbon contacts have different numbering on the two boards; consult the
[power-board](docs/coloplay-power.pdf) and [main-board](docs/coloplay-main.pdf)
drawings before probing them.

## Build, back up and flash

Run the following from the repository root. This procedure preserves the
**factory bootloader**: the default Arduino bootloader was recorded as repeatedly
resetting on this unit's flash chip (manufacturer ID `0x46`).

### 1. Prepare the tools and credentials

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install platformio 'esptool>=5.4,<6'
if [ ! -f firmware/include/secrets.h ]; then
  cp firmware/include/secrets.example.h firmware/include/secrets.h
fi
# Edit firmware/include/secrets.h: WIFI_SSID, WIFI_PASSWORD, OTA_PASSWORD.
./scripts/build_firmware.sh
```

`secrets.h` is git-ignored. WiFi credentials seed NVS on first boot, then runtime
configuration takes precedence and survives OTA. The application image is
`firmware/.pio/build/coloplay/firmware.bin`; the build uses `qio_opi` for N16R8
memory and the factory application-slot offsets.

### 2. Enter ROM download mode and save the factory image

1. Disconnect USB and, if fitted, the battery so the ESP32 fully powers off.
2. Hold **REC_KEY (GPIO0)** and press the **centre rotary knob (GPIO46)**.
3. Reconnect USB while holding both. Keep them pressed until the backup script
   prints `connected`, then release them.

Start the waiting script before connecting the device:

```bash
python scripts/wait_and_backup.py --timeout 600
```

The native USB Serial/JTAG interface uses VID:PID `303a:1001`. Linux normally
exposes `/dev/ttyACM*`; macOS uses `/dev/cu.usbmodem*`. The script reads the full
flash and writes an image, SHA-256 file and metadata under git-ignored `backups/`.
Save your own untouched factory image before the first write; local backups are
not distributed with this repository.

The existing capture is `backups/coloplay-factory-20261113-131459.bin`, exactly
16,777,216 bytes, with SHA-256
`9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e`.
Its captured security logs report Secure Boot and Flash Encryption disabled;
that finding applies to the captured unit.

### 3. First application flash over USB

Re-enter ROM download mode using the same two controls, then run:

```bash
python scripts/flash_bootstrap.py --timeout 600
```

The script automatically selects the newest `backups/coloplay-factory-*.bin` by
filename. Ensure that file is the correct, untouched factory backup for this
device; do not save later custom-firmware dumps under that filename pattern.

| Flash address | Bootstrap action |
|---|---|
| `0x0` | Preserve factory bootloader |
| `0x8000` | Write a copy of the factory partition table for the Arduino application |
| `0x10000` | Preserve the original factory partition table |
| `0x11000` / `0x15000` / `0x17000` | Preserve NVS / OTA selection data / PHY data |
| `0x20000` | Write `firmware.bin` into `ota_app0` |
| `0x4a0000` | Leave `ota_app1` untouched |

Each application slot is `0x480000` bytes (4.5 MiB). This bootstrap procedure
assumes the factory OTA metadata selects app0; it does not reset the slot
selection. Use OTA for subsequent updates, especially after the active slot has
changed. **Do not use a default PlatformIO full upload, erase the flash, or write
an old merged image** with this procedure. `0x10000` is the factory partition
table, not the application address. The application may update NVS after boot.

### 4. Verify and update over WiFi

After a normal boot, inspect USB serial at 115200 baud and open
`http://coloplay.local/` (or the IP printed in the serial log):

In USB Ethernet mode the port changes from its early serial console to the
network interface after startup. Open the same page at `http://172.32.29.21/`,
or select USB serial/JTAG in web settings for continuous serial access.

```bash
curl http://coloplay.local/api/status
```

The current source reports version `0.3.1-clock-usb`, `running_partition`,
`sketch_md5`, WiFi status and hardware telemetry. Peripheral startup waits five
seconds after the web server starts. Holding **KEY1 (GPIO6)** during a normal
reset skips the demo and leaves WiFi/web/OTA available; two failed peripheral
starts also trigger this recovery mode. This is separate from ROM download mode.

Build an update, then choose the transport supported by the installed firmware:

```bash
./scripts/build_firmware.sh
# ArduinoOTA: run from the device LAN, including the first update from 0.1.3-nvs.
./scripts/ota_upload.sh coloplay.local
# HTTP OTA: available once firmware with /update support is installed.
./scripts/ota_upload.sh http://coloplay.local
```

The uploader reads `OTA_PASSWORD` from `secrets.h`, validates the application
image and checks the running image's MD5 after reboot. HTTP OTA is also available
in the browser at `/update`, using username `admin` and `OTA_PASSWORD`.
Both OTA methods write the inactive application slot, update OTA metadata and
reboot through the preserved factory bootloader.

WiFi can be changed in the web UI or with the USB serial commands
`wifi <ssid> <password>` and `wifi-reset`; `status` prints JSON. `wifi-reset` clears
the stored WiFi namespace and seeds it from `secrets.h` again on boot.

### Factory restore and known boot warning

Follow the [full factory restore and verification procedure](docs/factory-firmware.md#恢复)
to write the original full image at `0x0` while preserving its flash-header
parameters. A restore overwrites custom firmware, NVS credentials and filesystem
data; full factory restoration has not yet been boot-tested.

The captured factory bootloader prints `SHA-256 comparison failed` and
`Attempting to boot anyway`; repository records show successful application boot
and OTA afterward. Offline analysis traced this specific mismatch to its flash
frequency header. See the [integrity analysis](docs/factory-firmware.md#完整性与-bootloader-头部差异);
this does not establish that other checksum failures are harmless.

## Reference documents

- [GPIO map](docs/gpio-map.md) — complete module-pad/net mapping and unresolved hardware details.
- [Factory GPIO evidence](docs/firmware-gpio-evidence.md) — recovered values, code/data addresses and image identity.
- [Factory LED teardown](docs/firmware-led-evidence.md) — original Lua/native write path, SPI configuration, encoding and reproduction script.
- [Hardware demo guide](docs/hardware-demo.md) — controls, HTTP OTA, test results and recovery.
- [Factory firmware](docs/factory-firmware.md) — backup integrity, complete partition table and restore commands.
- [Detailed flash notes](docs/flash.md) — earlier `0.1.3-nvs` bring-up and OTA verification records.
- [Firmware source](firmware/) — current PlatformIO hardware demo and web/OTA implementation.
- [LCD RAM probe](firmware/lcd_probe/README.md) — experimental ROM-loaded diagnostic; not a persistent flash image.
- Local schematics: [main board](docs/coloplay-main.pdf), [screen adapter](docs/coloplay-screen.pdf), [power board](docs/coloplay-power.pdf).
- [Manufacturer hardware documentation](https://x.cololight.com/mweb/docs/en/opensource/coloplay_hw/) and [Espressif boot-mode requirements](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

The initial [firmware feasibility report](cololight-firmware-report.html) is kept
as background; its initial discovery status predates the backup and bring-up.

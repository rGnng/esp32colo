# ColoPLAY hardware demo 0.2.0

This is the peripheral bring-up stage before feature development. The starting
device at `http://127.0.0.1:10081/api/status` reported `0.1.3-nvs`, 16 MiB flash,
no enabled PSRAM, working WiFi and three released buttons. The new firmware
builds successfully and passes host OTA tests. It has **not yet been confirmed
on the physical unit**; the first update must run from its LAN.

## Pull, build and upload

Run in your existing checkout on a computer that can reach `10.0.0.225` directly.
Keep its existing `firmware/include/secrets.h`: `OTA_PASSWORD` must match the
currently installed firmware for the first upload, and remains the new HTTP
OTA password. WiFi credentials already in NVS take precedence over the header.

```bash
git pull --ff-only
./scripts/build_firmware.sh && ./scripts/ota_upload.sh 10.0.0.225
```

If PlatformIO is missing, install it locally and repeat the build/upload:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install platformio
```

Dependencies are pinned to Espressif32 6.12.0 / Arduino 2.0.17, TFT_eSPI 2.5.43
and Adafruit NeoPixel 1.12.3. The ES8311 legacy-I2C driver is vendored from
Espressif with its Apache license and source revision. This retains the working
Arduino / IDF baseline. `BOARD_HAS_PSRAM` enables the existing `qio_opi`
configuration for the N16R8 module; flash / PSRAM GPIOs are not repurposed.

The uploader validates the chip ID, application descriptor, image checksum and
appended SHA-256 before contacting a device. It reads the password without
printing it or putting it in the process arguments, uploads the application,
then waits for `/api/status` to report its exact MD5. No backup, secrets or
compiled firmware images are committed.

The first update uses UDP 3232 for the ArduinoOTA handshake and a return TCP
connection from the ESP32 to the uploader on port **32320**. Permit that inbound
connection if your local firewall prompts. `--host-ip` and `--host-port` can
override the callback bind address / port. A TCP relay to device port 80 alone
cannot carry this initial protocol.

## HTTP OTA after the first update

The new firmware adds `/update` to the existing port 80 web server, so the same
netcat / TCP relay works without a UDP or return-TCP tunnel:

```bash
./scripts/build_firmware.sh && ./scripts/ota_upload.sh http://127.0.0.1:10081
```

Alternatively open `http://127.0.0.1:10081/update`, log in as `admin` using
`OTA_PASSWORD`, and select `firmware/.pio/build/coloplay/firmware.bin`.
For a separately provisioned uploader, `--secrets /path/to/secrets.h` or the
`COLOPLAY_OTA_PASSWORD` environment variable selects the device's password.

Both update methods preserve the factory bootloader at `0x0`, the partition
tables at `0x10000` / `0x8000`, NVS and vendor data partitions. They target the
inactive 4.5 MiB application slot. HTTP OTA authenticates before flash writes,
checks the S3 app prefix before erasing, supports an `X-MD5` integrity header,
and selects the new slot only after the complete upload verifies. An aborted
upload leaves the running application selected. This is not automatic rollback
after a successfully uploaded application subsequently crashes.

Do not run `pio run -t upload` or upload a merged image: that default USB path
can replace this unit's required factory bootloader.

## What the demo does

The screen cycles through four pages every ten seconds: animated orbit, input
states, microphone waveform / level, and sensor readings. The LED branches run
a dim rainbow; pressing a key lights its pixel white. A brief, low-level test
tone plays after hardware initialization.

| Control | Action |
|---|---|
| KEY1 / GPIO6 | Orbit page |
| KEY2 / GPIO1 | Inputs page |
| KEY3 / GPIO4 | Audio page and short test tone |
| KEY4 / GPIO5 | Sensors page |
| Knob turn / GPIO9,10 | Screen brightness; speaker level on the Audio page |
| Knob press / GPIO46 | Toggle automatic page tour |
| OPEN / GPIO15 | Toggle screen backlight and LEDs; web / OTA remain available |
| REC / GPIO0, while already running | Short test tone |

The web home page provides the same page, brightness, LED and tone controls,
live telemetry, a render-buffer preview, WiFi provisioning and the OTA link.
The preview is generated from the framebuffer; it is not optical verification
or a readback of the physical panel.

| Subsystem | Driver and remotely observable evidence |
|---|---|
| LCD | GC9A01, 240×240, SPI mode 0 at 40 MHz; frame count / FPS, screenshot, TE pulses on GPIO17 |
| Backlight | GPIO18, non-inverted LEDC channel 0, 5 kHz / 10 bits |
| RGB LEDs | GPIO48, GRB at 800 kHz, nine addresses for parallel four- and nine-pixel chains |
| Buttons | Seven active-low inputs, 25 ms debounce, state and press counter per key |
| Encoder | Dual-channel PCNT quadrature, hardware glitch filter, edge / detent counts |
| Audio | ES8311 ID registers must read `0x8311`; I2C 42/41, full duplex I2S at 16 kHz with MCLK 40, BCLK 39, LRCK 47, TX 21, RX 38 |
| Microphone | Analog input with GPIO8 bias enabled, waveform, DC-removed RMS / peak and dBFS |
| Speaker | Short enveloped sine tone; GPIO2 high is the initial amplifier-enable setting |
| Supply | ADC1_CH6 / GPIO7, averaged calibrated samples ×3 for the 2M/1M divider; approximate VDD, not isolated battery voltage |
| Light | ADC2_CH5 / GPIO16, raw / calibrated value, successful-read and error counts |
| Charge | GPIO3 logic level; low is reported explicitly without assuming battery presence |
| PSRAM | Enabled Octal PSRAM; 256 KiB allocation tested with two address-dependent patterns |

LCD SPI runs at 40 MHz initially; the factory binary configured 80 MHz.
The GRB LED protocol, amplifier enable polarity, microphone response, screen
orientation / colors, physical buttons and encoder direction still need live
confirmation. TE activity is panel scan feedback, not proof of correct pixels
or backlight. A successful I2S transfer is not proof of audible output.
ADC2 can contend with WiFi: failed light reads retain the last good value and
publish an error instead of silently reporting zero. No battery state of charge
or lux calibration is claimed.

## API and checks

```bash
curl http://127.0.0.1:10081/api/status
curl -X POST -d 'page=2&auto=0&tone=660&duration=300' http://127.0.0.1:10081/api/demo
curl -X POST -d 'brightness=65&leds=20&volume=65' http://127.0.0.1:10081/api/demo
curl -o screen.bmp http://127.0.0.1:10081/api/screenshot.bmp
```

`/api/status` retains the WiFi / button fields and adds `sketch_md5`,
`running_partition`, `reset_reason`, `recovery_mode` and a `hardware` object.
Check that the new firmware boots, the PSRAM pattern passes, frame and LED
counters advance, TE pulses arrive, codec ID is correct and microphone frames
advance without growing I/O errors. Then verify the visible display / LEDs,
audible tone, each key / knob, light changes and supply readings on the unit.

Local checks:

```bash
./scripts/build_firmware.sh
./scripts/test_http_ota.sh
```

The host tests exercise the real HTTP OTA callbacks against fake flash / HTTP
transport: unauthorized input, wrong chip, bootloader, wrong partition,
empty / truncated / aborted files, a disconnected-client timeout, invalid
digest headers, multiple files, finalization failure,
recovery and seven chunk sizes down to one byte. Python tests check checksum /
SHA integrity, escaped credentials, actual multipart payload / authentication /
MD5 and post-reboot verification. They do not replace tests on ESP32 hardware.

## Recovery

Networking starts before the peripherals, with a five-second OTA window.
Hold **KEY1 / GPIO6** during reset to skip hardware initialization while retaining
WiFi and both OTA methods. Holding REC / GPIO0 through reset is a ROM boot-strap
operation and is not the web recovery method.

If hardware initialization or the first 30 seconds of the demo fails on two
consecutive boots, the next boot automatically skips the peripherals. After a
corrected upload, clear the guard through authenticated `POST /api/hardware/retry`
to restart the demo. The counter is cleared after 30 seconds of successful
operation. Recovery still depends on the application reaching startup; it cannot
fix an invalid image or a failure before `setup()`.

The factory USB ROM-download and full-backup procedures remain documented in
[flash.md](flash.md) and [factory-firmware.md](factory-firmware.md).

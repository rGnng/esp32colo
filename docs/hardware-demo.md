# ColoPLAY hardware demo 0.2.5

This is the peripheral bring-up stage before feature development. The starting
device at `http://127.0.0.1:10081/api/status` reported `0.1.3-nvs`, 16 MiB flash,
no enabled PSRAM, working WiFi and three released buttons. The initial
`0.2.0-demo` upload reached recovery mode after two hardware-startup panics.
`0.2.5-rotary` is now installed through HTTP OTA, with its running-image
MD5 verified. With `0.2.4-factory-led`, the user confirmed the display, speaker,
ring and four key LEDs work. Key presses and knob movement were also observed
in telemetry. The panel
sends approximately 60–66 Hz TE pulses and the UI renders near 20 fps; the
256 KiB PSRAM pattern test passes. ES8311 ID `0x8311` / revision 1 responds at
I2C `0x18` with microphone samples and no reported I/O errors. The supply ADC
reads approximately 5.0–5.2 V on USB and the light ADC returns changing values
without errors. Internal OPEN / REC switch presses and charging transitions
were not separately exercised during this session.

## Rotary notch correction

The user reported one UI increment for every two physical notches. The PCNT
configuration counts all four edges of a quadrature cycle, while this encoder
has two notches per cycle. `0.2.5-rotary` consumes two edges per notch instead
of four. A signed remainder preserves incomplete movement; a single reverse
edge or contact bounce does not undo a completed step. Direction and per-step
brightness / volume changes are unchanged.

`hardware.encoder.edges_per_detent` reports `2` alongside raw `edges` and the
decoded `position`. Host checks cover both directions, partial turns, reversals,
bounce and multiple detents batched between polls. The build and HTTP OTA image
verification pass; physical notch-by-notch behavior still needs user confirmation.

## Display startup fix

TFT_eSPI 2.5.43's default S3 path defines `SPI_PORT` as Arduino's `FSPI`, whose
value is **0** in Arduino 2.0.17. ESP-IDF's `REG_SPI_BASE` expects **2** for GPSPI2
and returns zero for index 0, so `SPI_USER_REG(SPI_PORT)` becomes address `0x10`.
The library writes that address during `init()`. The observed panic reset and
two failed starts are consistent with this fault.

`USE_FSPI_PORT=1` selects the library branch that uses register index 2 while
retaining Arduino's FSPI bus object. The firmware now checks both the index
and actual `SPI_USER_REG` address with compile-time assertions. GPIO assignments
and the GC9A01 controller remain the same. The updated device has passed hardware
initialization and reports live panel scan pulses. Related upstream report:
[TFT_eSPI S3 initialization panic](https://github.com/Bodmer/TFT_eSPI/issues/3332).

The failed-start budget is now scoped to the running image MD5: a new image
automatically gets a fresh attempt, while repeated crashes of the same image
still enter recovery. Startup stages are retained in RTC memory across warm
resets and exposed as `hardware.stage` / `hardware.last_boot_stage`, so a later
failure can be located without relying only on serial logs.

## LED and speaker corrections

The LED transmitter is reconstructed from the [factory binary](firmware-led-evidence.md):
SPI3 at 3.2 MHz, GRB, fourteen software slots, four SPI bits per LED bit, and a
480-byte transfer including reset padding. The original nine-pixel / parallel
branch assumption was incorrect. Generic NeoPixel timing has been replaced by
the factory `1000` / `1100` symbols. The user confirmed all lights work after
this replacement. Adafruit NeoPixel remains a dependency for its color / gamma
helpers; its RMT transmitter is not used.

Gamma correction now applies to full-scale colors before brightness scaling;
the previous order made low brightness values quantize near zero. The default
is 40/255, adjustable from 0 to 64. “Test all LEDs” sends white for five seconds;
API `led_test` values 2/3/4 similarly select red/green/blue.

Speaker percentage now controls linear amplitude with a 0 dB ceiling. ES8311
DAC register `0x32` has a -95.5 dB baseline and 0.5 dB steps, so `0xBF` is 0 dB;
the [Linux ES8311 driver](https://github.com/torvalds/linux/blob/master/sound/soc/codecs/es8311.c)
records the same scale. Default volume 75% maps to -2.5 dB; 100% reaches 0 dB,
and 0% mutes. This avoids the legacy driver's raw 0–255 register-percentage
mapping, which made modest UI settings too quiet and allowed positive gain.

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
states, microphone waveform / level, and sensor readings. The LEDs run
a rainbow; pressing a key lights its pixel white. A brief, low-level test
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
| RGB LEDs | GPIO48, factory SPI3 encoding at 3.2 MHz / 800 kbit/s logical rate, fourteen-slot frame; ring and key LEDs confirmed |
| Buttons | Seven active-low inputs, 25 ms debounce, state and press counter per key |
| Encoder | Dual-channel PCNT quadrature, hardware glitch filter, two edges per notch, edge / detent counts |
| Audio | ES8311 ID registers must read `0x8311`; I2C 42/41, full duplex I2S at 16 kHz with MCLK 40, BCLK 39, LRCK 47, TX 21, RX 38 |
| Microphone | ES8311 analog input / ADCV bias network, waveform, DC-removed RMS / peak and dBFS |
| Speaker | Short enveloped sine tone; audible output confirmed with GPIO2 high |
| LED power | GPIO8 high enables VCCL; the schematic net is called MICBIAS, but it also biases the light sensor |
| Supply | ADC1_CH6 / GPIO7, averaged calibrated samples ×3 for the 2M/1M divider; approximate VDD, not isolated battery voltage |
| Light | ADC2_CH5 / GPIO16, raw / calibrated value, successful-read and error counts |
| Charge | GPIO3 logic level; low is reported explicitly without assuming battery presence |
| PSRAM | Enabled Octal PSRAM; 256 KiB allocation tested with two address-dependent patterns |

LCD SPI runs at 40 MHz initially; the factory binary configured 80 MHz.
The panel, speaker and all visible LEDs have been confirmed by the user.
Exact key-to-LED spatial order, color calibration and encoder direction remain
available for refinement. TE activity alone is panel scan feedback; the separate
user report confirms visible pixels. I2S transfers alone are not proof of sound;
audible output was checked separately.
ADC2 can contend with WiFi: failed light reads retain the last good value and
publish an error instead of silently reporting zero. No battery state of charge
or lux calibration is claimed.

## API and checks

```bash
curl http://127.0.0.1:10081/api/status
curl -X POST -d 'page=2&auto=0&tone=660&duration=300' http://127.0.0.1:10081/api/demo
curl -X POST -d 'brightness=65&leds=40&volume=75' http://127.0.0.1:10081/api/demo
curl -X POST -d 'led_test=1&leds=48' http://127.0.0.1:10081/api/demo
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
MD5 and post-reboot verification. Additional checks cover codec gain bounds,
brightness scaling and every channel value against the factory LED lookup table,
including GRB order, the last slot and reset padding. The suite has 25 host cases;
the hardware observations above were collected separately on the running unit.

## Recovery

Networking starts before the peripherals, with a five-second OTA window.
Hold **KEY1 / GPIO6** during reset to skip hardware initialization while retaining
WiFi and both OTA methods. Holding REC / GPIO0 through reset is a ROM boot-strap
operation and is not the web recovery method.

If hardware initialization or the first 30 seconds of the demo fails on two
consecutive boots of the same image, the next boot automatically skips the
peripherals. A corrected upload gets a fresh retry budget automatically.
Authenticated `POST /api/hardware/retry` can also clear the guard and restart
the same image. The counter is cleared after 30 seconds of successful
operation. Recovery still depends on the application reaching startup; it cannot
fix an invalid image or a failure before `setup()`.

The factory USB ROM-download and full-backup procedures remain documented in
[flash.md](flash.md) and [factory-firmware.md](factory-firmware.md).

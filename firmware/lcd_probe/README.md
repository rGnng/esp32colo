# ColoPLAY LCD probe

Optional RAM-only ESP32-S3 research diagnostic. The main firmware now drives
this unit's display, and the factory image's selected GC9A01 family driver is
[identified from executable code](../../docs/firmware-gpio-evidence.md#lcd-recovered-from-executable-code).
This probe itself has not been exercised on the device and is not needed for
normal operation. It reads possible responses rather than promising an exact
controller variant.

The application is built entirely for RAM, without SPI flash or PSRAM support.
Load it with the ROM downloader's `load_ram` command. It does not rewrite flash;
a normal power cycle starts the application already installed there. ESP-IDF
labels RAM application builds experimental. Device security settings may prevent
RAM downloads.

## Why detection is uncertain

The recovered factory SPI configuration has MISO=-1 and uses MOSI GPIO13.
That is a software configuration, not proof of the physical panel interface
straps or available readback wiring. Some controllers can return register data on the shared data line; others require an unconnected output or disable reads in the selected interface mode. The absence of a response does not identify the controller or prove the display is faulty.

The probe sends common DCS read commands (`04`, `09`, `0A`, `0B`, `0C`, `DA`, `DB`, `DC`) with a slow software SPI clock. GPIO13 is released to input for the response. Each read is repeated with weak pull-up and pull-down bias, alongside captures with CS inactive. All eight captured bytes are printed, including any dummy bits and undriven trailing bits. Different commands/controllers can need different dummy-cycle counts; the program makes no automatic ID-to-driver assignment.

No LCD pins are configured at startup. `probe` and `reset` configure the reference pins and leave the backlight off. No vendor-specific initialization tables, panel voltages, or gamma settings are sent.

## Reference pins

The display assignments below are recovered from the [factory application](../../docs/firmware-gpio-evidence.md#lcd-recovered-from-executable-code). Shared-line readback remains an experiment:

| Signal | GPIO |
|---|---:|
| D/C | 11 |
| CS | 12 |
| MOSI / attempted shared data | 13 |
| Clock | 14 |
| Backlight output | 18 |
| Reset | 45 |

## Build

Use ESP-IDF 5.5.6 with its ESP32-S3 Xtensa toolchain and Python environment activated:

```sh
cd firmware/lcd_probe
idf.py -DIDF_TARGET=esp32s3 build
```

Output: `build/coloplay_lcd_probe.bin`. CMake rejects a different chip target or a configuration that would build a flash application.

ESP-IDF may print generic flash instructions after the build. Use the RAM-loading command below for this diagnostic.

## Run from RAM

Use this only with the captured unit or an independently verified matching device.
The established [ROM download sequence](../../docs/flash.md#first-usb-flash) removes all power, holding **REC_KEY and the centre knob pressed**, then reconnecting USB and releasing both after about a second. They pull GPIO0 and GPIO46 low at reset. See the [Espressif boot-mode requirements](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

With ESP-IDF's **esptool 4.13.0** active, substitute the actual serial port below:

```sh
python -m esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub --after no_reset load_ram build/coloplay_lcd_probe.bin
python -m serial.tools.miniterm /dev/ttyACM0 115200 --dtr 0 --rts 0
```

`--no-stub` loads through ROM directly; `--after no_reset` avoids resetting away the RAM application. This binary is not a flash-installable image: do not use `write_flash` or `idf.py flash` with it.

In the terminal, press Enter for help, then run:

```text
pins
info
probe
probe 3
```

`reset` optionally pulses the LCD reset pin before another probe. Close miniterm with Ctrl+] without toggling reset lines. Power-cycle normally, with REC_KEY and the knob released, to return to the application already installed in flash.

## Interpret the capture

- A response that follows the weak bias (all `00` with pull-down and `FF` with pull-up) usually indicates an undriven line. This is inconclusive.
- Repeated response bits that remain stable under both biases and differ from the CS-inactive control are stronger evidence of panel output. Dummy clocks can shift the meaningful bits; trailing unused bits may still follow the bias.
- A returned ID can be generic or shared between controllers. Confirm it against the matching datasheet or factory initialization sequence before selecting a driver.
- If reads remain inconclusive, inspect the panel flex marking or capture the factory firmware's SPI initialization with a logic analyzer before replacing it.

Reference: [Espressif SPI LCD interface modes](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/spi_lcd.html), [common LCD command definitions](https://github.com/espressif/esp-idf/blob/v5.5.6/components/esp_lcd/include/esp_lcd_panel_commands.h).

## Validation

Built locally with ESP-IDF `v5.5.6-dirty` (an existing SDK checkout) and Xtensa GCC 14.2.0, with warnings treated as errors for the probe source. `esptool image_info` validated the 131,344-byte image and its internal RAM segments, including RTC RAM; there are no flash-mapped segments. An eight-case host GPIO simulation checked command/response bits in modes 0 and 3, data-line release before panel output, and inactive-CS captures with both biases. This checks software behavior only. This was software-only validation; the probe still has no recorded device trial.
Factory-code analysis separately identifies the selected GC9A01 family driver;
physical single-line readback and the exact panel variant remain unresolved.

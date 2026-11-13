# GPIO evidence from the factory firmware

Factory-image evidence only; hardware drawings are not used as a source.

Analysis of the uploaded factory backup on 2026-11-13. The original dump was preserved byte for byte. The initial GPIO pass was offline; the subsequent [LED transmitter teardown](firmware-led-evidence.md) was followed by custom-firmware OTA and confirmation on the device.

## Image and method

- Local image: `backups/coloplay-factory-20261113-131459.bin` (16,777,216 bytes).
- SHA-256: `9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e`, matching the supplied checksum file.
- The partition-table MD5 is valid. OTA metadata selects `ota_app0` at flash offset `0x20000`; its image checksum and appended SHA-256 are valid. `ota_app1` is erased.
- The application was parsed with esptool 4.13.0 and its Xtensa instructions disassembled with the ESP32-S3 objdump tool. Constants were traced through initialization calls and configuration structures; a GPIO number appearing somewhere in a binary was not treated as proof of its purpose.

The following addresses apply only to this exact image. **Code/data addresses below are runtime virtual addresses, not flash-write offsets.** In this image, IROM address `0x42000020` maps to full-dump offset `0x120020`, and DROM address `0x3c230020` maps to full-dump offset `0x20020`.

## LCD: recovered from executable code

| Signal / setting | Factory configuration | Evidence |
|---|---|---|
| SPI MOSI / `LCD_MOSI` | GPIO13 | At `0x420ef8ac–0x420ef8b3`, setup passes SCLK=14 and MOSI=13 to the SPI-bus helper. |
| SPI clock / `LCD_CLK` | GPIO14 | Same call. |
| Separate MISO | `-1` (not configured) | `0x420ef8a1`; the helper argument order matches the linked LVGL SPI helper. |
| Chip select / `LCD_CS` | GPIO12 | `0x420efec8` loads 12; `0x420eff07` places it in the SPI device configuration. |
| D/C / `LCD_RS` | GPIO11 | `0x420efb12–0x420efb21` configures GPIO11 as output; the command/data paths subsequently toggle it. |
| Reset / `LCD_REST` | GPIO45, active low | `0x420efb24–0x420efb4f` configures GPIO45 as output, drives it low, then high. |
| Backlight / `LCD_BLK` | GPIO18, PWM enabled, output inversion disabled | Configuration at `0x3c2f762c`: `01 00 00 00 12 00 00 00 00 00 00 00 00 00 00 00`. The display setup copies this configuration into the backlight constructor. |
| SPI frequency | 80,000,000 Hz | `0x420eff97` loads `0x04c4b400` for the display device's clock argument. |
| SPI mode | 0 | Device setup at `0x420efec3–0x420eff09`; the mode field and corresponding log argument are zero. |
| Controller driver | GC9A01 family | Display setup calls `0x420efae4` from `0x420ef8c3`. That routine uses the `GC9A01` tag at `0x3c2482a4` and the initialization table at `0x3c2f7664`. |

The initialization table begins with `EF`, `EB 14`, `FE`, `EF`, `EB 14`, `84 40`. This is executable driver/configuration evidence, beyond the presence of a controller-name string. It identifies the selected factory driver; it does not distinguish every GC9A01/GC9A01A panel variant or prove that a generic replacement initialization table will work unchanged.

The backlight structure's full-dump offset is `0xe762c`. GPIO17 (`LCD_TE`) use was not established in the recovered factory display configuration. Separately, the custom driver now enables TE output and observes roughly 60–66 Hz pulses on GPIO17; this is a live-device result rather than a recovered factory setting.

The factory driver does not configure a separate MISO pin. This does **not** establish that the panel is physically unable to return data on its shared SDA/MOSI line. Panel interface straps and controller readback support still determine that. The RAM probe remains an optional experiment; identifying the controller family no longer depends on it.

## Buttons: recovered native scan order

The initialized data table at `0x3fca0ad4` (full-dump offset `0x1144e0`) contains six 12-byte entries. Their first 32-bit words are:

```text
6, 1, 5, 4, 46, 15
```

The loop at `0x420264c8–0x4202650f` reads those six entries, builds the GPIO input mask and enables input mode with pull-up requested.

| Table position | GPIO |
|---:|---:|
| 0 | 6 |
| 1 | 1 |
| 2 | 5 |
| 3 | 4 |
| 4 | 46 |
| 5 | 15 |

These are positions in the native scan table, not independently verified Lua
`btn()` IDs. GPIO0 is absent from this six-entry factory scan table. Board
switch positions and connector assignments are not established by these words.

## Rotary encoder

At `0x42026662–0x42026679`, the PCNT channel configuration contains edge GPIO9 and level GPIO10. At `0x420266a3–0x420266af`, the other channel reverses them. The recovered assignments are:

- encoder channel A → GPIO9.
- encoder channel B → GPIO10.

## Audio

The I2S pin configuration at `0x3c264ec4` (full-dump offset `0x54ec4`) is copied into an I2S setup call at `0x42024f18–0x42024f2e`:

```text
40, 39, 47, 21, 38
```

Using the ESP-IDF legacy I2S pin-structure layout, those fields are:

| Function | GPIO |
|---|---:|
| MCLK | 40 |
| BCLK | 39 |
| WS / LRCK | 47 |
| Data out, ESP32 → codec | 21 |
| Data in, codec → ESP32 | 38 |

At `0x4201b0f8–0x4201b11f`, the I2C configuration places SDA=42 and SCL=41 into the structure. The firmware also includes ES8311 register/codec routines.

GPIO8 is explicitly driven low/high at `0x42026bf1–0x42026c14`. Those writes establish a controlled output, but not its board-level role, component connections or active polarity. GPIO2 amplifier control was not established by this factory-code pass.

## LED transmitter: recovered native path

The factory's embedded Lua calls `qt.init(48)` at `0x3c26988f` and creates
`qt.new(14)` at `0x3c267c14`. The `qt:write()` binding at `0x3c264228` points to
`0x4201e434`, which calls the encoder at `0x4201e33c`. Its initialized SPI
configuration selects GPIO48, SPI3, 3.2 MHz and mode 0. The encoder emits GRB
data with four SPI bits per LED bit: zero=`1000`, one=`1100`, in a 480-byte
transfer with fourteen-slot capacity and reset padding. Full addresses,
byte values, reproduction script and device results are in the
[LED evidence](firmware-led-evidence.md).

## Limits of the recovered configuration

Factory configuration or physical roles for GPIO2, GPIO3, GPIO7, GPIO16,
GPIO17, GPIO19/20 and GPIO43/44 were not independently recovered in this pass.
No module-pad/connector mapping, resistor network, sensor component identity,
amplifier identity or physical pull-up arrangement is inferred here.

The factory software's fourteen-slot LED capacity does not establish populated
package count or physical ring/key order. Reproducing its waveform lights the
ring and key LEDs in the custom firmware, which is a separate device result.
See the [development baseline](development.md#verified-baseline-and-open-items)
for custom-firmware observations, and the [compact factory GPIO map](gpio-map.md)
for a summary of the recovered assignments.

## Local evidence and references

`backups/analysis/` contains the extracted application/bootloader, parsed partition records, segment map, GC9A01 initialization table, and disassembly. The archive, raw dump, device logs, and derived binary data are all git-ignored under `backups/`.

For example, these read-only commands show the configuration data directly in the full dump; each key-table row contains a GPIO word followed by two initialized zero words:

```sh
xxd -e -g 4 -s 0x1144e0 -l 72 backups/coloplay-factory-20261113-131459.bin
xxd -e -g 4 -s 0x54ec4 -l 20 backups/coloplay-factory-20261113-131459.bin
xxd -e -g 4 -s 0xe762c -l 16 backups/coloplay-factory-20261113-131459.bin
```

- [ESP32-S3 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/peripherals/gpio.html).
- [LVGL SPI helper argument order](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_helpers.c).
- [LVGL backlight configuration layout](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_tft/esp_lcd_backlight.h) and [SPI device configuration](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_tft/disp_spi.c). These explain the matching structure/code patterns; the exact vendor source revision is unknown.
- [ESP SPI LCD interface modes](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/spi_lcd.html).

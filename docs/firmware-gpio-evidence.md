# GPIO evidence from the factory firmware

Offline analysis of the uploaded factory backup on 2026-11-13. The original dump was preserved byte for byte; no firmware was loaded onto a device during this analysis.

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

The backlight structure's full-dump offset is `0xe762c`. GPIO17 (`LCD_TE`) is connected in the schematic, but its use was not established in the recovered display configuration. Keep it documented as a schematic connection rather than a firmware-confirmed synchronization feature.

The factory driver does not configure a separate MISO pin. This does **not** establish that the panel is physically unable to return data on its shared SDA/MOSI line. Panel interface straps and controller readback support still determine that. The RAM probe remains an optional experiment; identifying the controller family no longer depends on it.

## Buttons: software order differs from schematic naming

The initialized data table at `0x3fca0ad4` (full-dump offset `0x1144e0`) contains six 12-byte entries. Their first 32-bit words are:

```text
6, 1, 5, 4, 46, 15
```

The loop at `0x420264c8–0x4202650f` reads those six entries, builds the GPIO input mask and enables input mode with pull-up requested.

| Table position | GPIO | Schematic signal |
|---:|---:|---|
| 0 | 6 | `KEY1` |
| 1 | 1 | `KEY2` |
| 2 | 5 | `KEY4` |
| 3 | 4 | `KEY3` |
| 4 | 46 | `EKEY`, knob press |
| 5 | 15 | `OPEN_KEY` |

These are positions in the native scan table, not independently verified Lua `btn()` IDs. In particular, do not rename schematic `KEY3` to GPIO5: **the schematic net is still `KEY3` → GPIO4**. It is the software scan order that swaps the third and fourth key nets.

`REC_KEY` / GPIO0 is absent from this six-entry scan table. Its download-mode connection remains established by the schematics and ESP32-S3 boot requirements.

## Rotary encoder

At `0x42026662–0x42026679`, the PCNT channel configuration contains edge GPIO9 and level GPIO10. At `0x420266a3–0x420266af`, the other channel reverses them. This corroborates:

- `EAS` / encoder A → GPIO9.
- `EBS` / encoder B → GPIO10.

The encoder component is **EC1** on the screen adapter; **S1** on that board is the separate knob push switch.

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

At `0x4201b0f8–0x4201b11f`, the I2C configuration places SDA=42 and SCL=41 into the structure. These agree with the ES8311 schematic. The firmware also includes ES8311 register/codec routines.

GPIO8 is explicitly driven low/high at `0x42026bf1–0x42026c14`, corroborating control of the `MICBIAS` net. Those writes alone do not establish the intended on/off semantics of every audio mode. The amplifier control polarity on GPIO2 was not established in this pass.

## Connections supported by the drawings, with firmware use unresolved

The existing schematic mapping remains the source for GPIO2 (`AUD_CTRL`), GPIO3 (`CHRG`), GPIO7 (`VOL_ADC`), GPIO16 (`LIGHTADC`), GPIO17 (`LCD_TE`), GPIO19/20 (USB), GPIO43/44 (UART0), and GPIO48 (`RGB_LED`). This analysis did not independently recover every one of their runtime configurations.

Additional corrections to the GPIO guide:

- ESP32-S3 has no GPIO22–25. GPIO26–32 serve internal memory; GPIO33–37 are also occupied with Octal PSRAM on this module family. These are not spare, unused board GPIOs.
- `VOL_ADC` is connected to **VDD through a 2 MΩ / 1 MΩ divider**, nominally VDD/3. The drawing supports supply-voltage sensing, not a volume potentiometer or board-version selector. It is not an isolated battery-voltage measurement under all power conditions.
- The power-board drawing shows no external 10 kΩ pull-up on `OPEN_KEY`. Factory input initialization requests an internal pull-up; the knob push switch separately has an external 10 kΩ pull-up.
- `RGB_LED` splits through R35 and R36 into the 9-LED and 4-LED branches in the main-board drawing. It is not a documented single 13-LED serial chain. The exact LED protocol and visible mapping still need hardware verification.

## Local evidence and references

`backups/analysis/` contains the extracted application/bootloader, parsed partition records, segment map, GC9A01 initialization table, and disassembly. The archive, raw dump, device logs, and derived binary data are all git-ignored under `backups/`.

For example, these read-only commands show the configuration data directly in the full dump; each key-table row contains a GPIO word followed by two initialized zero words:

```sh
xxd -e -g 4 -s 0x1144e0 -l 72 backups/coloplay-factory-20261113-131459.bin
xxd -e -g 4 -s 0x54ec4 -l 20 backups/coloplay-factory-20261113-131459.bin
xxd -e -g 4 -s 0xe762c -l 16 backups/coloplay-factory-20261113-131459.bin
```

- [Main-board schematic](coloplay-main.pdf), [screen-adapter schematic](coloplay-screen.pdf), [power-board schematic](coloplay-power.pdf).
- [ESP32-S3 GPIO restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/peripherals/gpio.html).
- [LVGL SPI helper argument order](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_helpers.c).
- [LVGL backlight configuration layout](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_tft/esp_lcd_backlight.h) and [SPI device configuration](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_tft/disp_spi.c). These explain the matching structure/code patterns; the exact vendor source revision is unknown.
- [ESP SPI LCD interface modes](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/spi_lcd.html).

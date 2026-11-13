# Factory LED transmitter teardown

This is factory-image evidence. Current controls and custom-firmware behavior
are documented in the [firmware guide](firmware.md#lighting); the compact
[GPIO map](gpio-map.md) contains only recovered factory assignments.

The replacement LED driver in `0.2.4-factory-led` was reconstructed from the
original firmware's embedded Lua, native call path, initialized data and pixel
encoder. This analysis uses the factory binary as evidence. After HTTP OTA,
the user confirmed the display, speaker, ring and four key LEDs work.

## Image and reproduction

Untouched local image: `backups/coloplay-factory-20261113-131459.bin`, 16 MiB,
SHA-256 `9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e`.
The backup and extracted binaries remain ignored by Git.

```bash
python3 scripts/analyze_factory_leds.py --output backups/analysis/factory-led-evidence.json
./scripts/test_http_ota.sh
```

The extraction script checks the whole-image hash, parses application segments,
checks the selected source/instruction bytes, and reports both virtual addresses
and full-dump offsets. It never extracts NVS or user credentials. Addresses here
apply to this exact image.

## Entry points and call path

| Evidence | Runtime address | Full-dump offset / result |
|---|---:|---|
| Embedded `qt.init(48)` | `0x3c26988f` | `0x5988f` |
| Embedded `_G.buffer=qt.new(14)` | `0x3c267c14` | `0x57c14` |
| `init` function pointer in the Qt Lua table | `0x3c264128` | Points to `0x4201e2e8` |
| `write` function pointer in the Qt object table | `0x3c264228` | Points to `0x4201e434` |
| Native init routine | `0x4201e2e8` | Sets bus MOSI from Lua argument, default 48; initializes SPI and adds device |
| `qt:write()` wrapper | `0x4201e434` | Gets the buffer, multiplies its pixel count by 3, calls `0x4201e33c` |
| Encoder / transfer routine | `0x4201e33c` | Scales RGB, encodes GRB, submits SPI transaction |

The SPI API names are inferred from their argument structures and call patterns;
the stripped application does not supply ELF symbol names. The Lua binding
names and pointer values above are present in the binary.

## SPI configuration recovered from initialized DRAM

| Field | Runtime data address | Value |
|---|---:|---|
| Host | `0x3fca09ec` | 2, `SPI3_HOST` |
| DMA selection | `0x3fca09f4` | 3, `SPI_DMA_CH_AUTO` |
| Mode | `0x3fca09fb` | 0 |
| Clock | `0x3fca0a08` | `0x0030d400` = **3,200,000 Hz** |
| MOSI | `0x3fca0a24` | **GPIO48** |
| MISO | `0x3fca0a28` | `-1` |
| SCLK output | `0x3fca0a2c` | `-1` |
| Initial brightness byte | `0x3fca0a78` | 30; the encoder scales channels by this byte / 255 |

The LED protocol is generated on MOSI; no physical SPI clock wire is needed.
SPI3 is separate from the display's SPI bus. This is the actual selected LED
transmit path; the presence of generic RMT library strings elsewhere in the
image is not evidence that Qt LED output uses RMT.

## Encoding and frame length

The sixteen little-endian 16-bit values at `0x3fca0a58` (full-dump offset
`0x114464`) encode one four-bit color nibble into sixteen SPI bits:

```text
8888 8c88 c888 cc88 888c 8c8c c88c cc8c
88c8 8cc8 c8c8 ccc8 88cc 8ccc c8cc cccc
```

Interpret each value as two little-endian bytes transmitted MSB first. For
example, nibble zero emits `88 88`, or `1000 1000 1000 1000`.

| Logical bit | SPI symbol | High time | Low time |
|---|---|---:|---:|
| 0 | `1000` | 312.5 ns | 937.5 ns |
| 1 | `1100` | 625 ns | 625 ns |

Each logical bit lasts four SPI clocks: **1.25 µs / 800 kbit/s**. The routine
reads source channels as RGB but stores encoded output in **G, R, B** order.
Each channel expands to four bytes, so each pixel occupies twelve SPI bytes.

The native routine:

- Clears **480 bytes** at buffer `0x3fca734c` (`0x4201e351`).
- Limits the software pixel payload to **14 RGB slots** (`0x4201e361`).
- Writes encoded G/R/B words starting at offsets **72 / 76 / 80**
  (`0x4201e422–0x4201e428`), advancing twelve bytes per pixel.
- Submits a transaction with **`length=0xf00` bits**, or 480 bytes; the constant
  is at `0x42001e10`, used at `0x4201e376–0x4201e381`.

At capacity, that gives 72 raw zero bytes before the encoded pixels and 240
after them: **180 µs low**, **420 µs pixel data**, **600 µs low**, for a 1.2 ms
transfer. These raw zero regions are latch/reset time, not encoded black pixels.
Fourteen is the factory software frame capacity; it does not independently
establish the number of populated LED packages.

Equivalent recovered behavior:

```text
clear 480-byte transmit buffer
for each of up to 14 source RGB entries:
    scale channels by brightness / 255
    append encoded G, R, B at byte 72 + pixel_index * 12
submit all 480 bytes through SPI3 at 3.2 MHz
```

## Replacement and observed result

[factory_led_frame.h](../firmware/include/factory_led_frame.h) reproduces the
symbols, GRB byte order, fourteen-slot capacity and reset padding.
[led_strip.cpp](../firmware/src/led_strip.cpp) sends that frame through SPI3 on
GPIO48. The clock is generated by hardware rather than software delay loops.

The host fixture compares every possible channel byte against the independently
extracted factory nibble table, checks GRB order, the final slot and both zero
padding regions. The device reports successful SPI transactions without errors.
The user then confirmed **all lights work**, including the four key LEDs.

Earlier code sent only nine pixels and used generic NeoPixel pulse timing.
Both the software capacity and waveform have now been corrected. The earlier
documentation's parallel-branch claim was incorrect and has been removed.
The current custom implementation uses ring slots 0–8 and key slots 9–12;
slot 13 stays off during normal output and temporary color tests. This grouping
is a custom implementation choice rather than a recovered physical layout. Exact key-to-pixel spatial
order and color calibration can be refined separately if needed.

The [broader GPIO teardown](firmware-gpio-evidence.md) records the independently
recovered display, button, encoder and audio configurations. The original image
was never modified or written back to the device during this teardown.

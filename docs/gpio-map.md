# GPIO map recovered from factory firmware

This map contains only GPIO assignments recovered from the captured factory
application. It summarizes the [addressed evidence](firmware-gpio-evidence.md)
and [LED teardown](firmware-led-evidence.md), which retain the image identity,
virtual addresses, dump offsets and extraction methods.

Module-pad numbers, board connector contacts, resistor values, component
reference designators and unrecovered signal roles have been removed.

## Display

| Factory setting | Recovered value |
|---|---|
| Controller driver | GC9A01 family |
| MOSI | GPIO13 |
| Clock | GPIO14 |
| CS | GPIO12 |
| D/C | GPIO11 |
| Reset | GPIO45, active low |
| Backlight | GPIO18, PWM, not inverted |
| Separate MISO | -1, not configured |
| SPI | 80 MHz, mode 0 |

These are factory settings. The custom driver currently uses 40 MHz and has
produced visible output; that does not change the recovered factory value.
Factory GPIO17/TE configuration and physical single-wire display readback
capability were not established by this analysis. The exact GC9A01 variant
remains unresolved.

## Keys and encoder

| Native factory scan index | GPIO |
|---:|---:|
| 0 | 6 |
| 1 | 1 |
| 2 | 5 |
| 3 | 4 |
| 4 | 46 |
| 5 | 15 |

Factory initialization requests input pull-ups for these six entries. These
are native table positions, not established Lua `btn()` IDs or board connector
labels. The custom firmware's KEY1–4/knob/OPEN naming is an application
convention; it is not additional factory evidence. GPIO0 is absent from this
factory scan table.

The two factory PCNT channels use **GPIO9 and GPIO10**, swapping edge/level
roles between channels. The current firmware's two-edges-per-notch decoder is
a later custom implementation, not a recovered factory mechanical specification.

## Audio

| Factory I2S function | GPIO |
|---|---:|
| MCLK | 40 |
| BCLK | 39 |
| WS / LRCK | 47 |
| ESP32 data out | 21 |
| ESP32 data in | 38 |

The recovered I2C configuration uses **SDA42 / SCL41**. The application includes
ES8311 codec/register routines. GPIO8 is explicitly written low/high, but those
writes alone do not establish its board-level purpose or active polarity.

## LEDs

The factory Lua/native path selects **GPIO48**, **SPI3**, **3.2 MHz**, mode 0.
The transmitter sends **GRB** with `1000`/`1100` symbols, fourteen software slots
and a 480-byte frame including reset padding. Fourteen is software capacity;
it does not prove fourteen physical packages or their spatial order.

See the [LED evidence](firmware-led-evidence.md) for the independent nibble-table
fixture and reproduced waveform. Ring/key grouping in the custom application
is described as custom behavior in the [firmware guide](firmware.md#lighting).

## Not recovered

This pass did not establish factory configuration or board roles for GPIO2,
GPIO3, GPIO7, GPIO16, GPIO17, GPIO19/20 or GPIO43/44. It does not establish
connector pinouts, analog divider ratios, light-sensor component type or
amplifier identity/polarity. Do not infer those details from this map.

Custom firmware has separately exercised audio output, ADC changes, TE pulses
and USB networking. Those observations are kept in the
[development baseline](development.md#verified-baseline-and-open-items) and
are not promoted into factory-code findings.

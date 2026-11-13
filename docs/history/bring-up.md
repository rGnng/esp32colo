# Bring-up history

Hardware bring-up is complete. This file records decisions behind the working
baseline; use the [firmware guide](../firmware.md), [API](../api.md) and
[flashing procedure](../flash.md) for current behavior. Superseded feasibility
and hardware-drawing assumptions are not retained as hardware evidence.

## Original image and application-only boot

A complete untouched factory image was preserved before writes. Its identity,
partition table and bootloader-header/hash analysis are maintained in
[factory firmware](../factory-firmware.md). The parsed table places applications
at `0x20000` and `0x4a0000`, with the factory table at `0x10000`.

A default Arduino bootloader repeatedly reset on this unit's flash chip. The
working approach preserves the factory bootloader and adds a partition-table
copy at `0x8000` for the Arduino application. `0.1.3-nvs` established WiFi/NVS
and ArduinoOTA, with both OTA application slots exercised. It did not initialize
the display/audio/LED peripherals. Its old status examples are not current defaults.

## Display startup and recovery

The early `0.2.0-demo` hardware startup panicked. TFT_eSPI 2.5.43's S3 default
path used Arduino FSPI=0 as an IDF SPI register index, producing register address
`0x10`. `USE_FSPI_PORT=1` selects index 2; compile-time assertions now guard the
index and register base. See the [factory LCD configuration](../firmware-gpio-evidence.md#lcd-recovered-from-executable-code)
for recovered values, and the [development guide](../development.md) for custom settings.

Startup stages are retained across warm resets. The failed-start counter is
scoped to image MD5, so a corrected application gets a new attempt budget while
repeated crashes of one image still enter recovery. This provides a web/OTA
recovery window rather than bootloader rollback.

## LED, speaker and encoder corrections

`0.2.4-factory-led` reproduced the original image's SPI3 3.2 MHz, GRB,
fourteen-slot, 480-byte LED frame. The [factory LED teardown](../firmware-led-evidence.md)
retains the native call path, lookup table and waveform evidence. Generic LED
pulse timing and the earlier nine-slot-only assumption were superseded. Software
slot capacity does not establish physical package count or parallel branches.

Gamma correction moved before brightness scaling to retain low-level color
resolution. The former shared byte-scale brightness control was later replaced
by independent 0–100% screen/ring/key controls with 70% defaults, persistent
settings, static/rainbow selection and sensor-driven Auto. The legacy `leds`
API field remains only for compatibility.

Speaker percentage now represents linear signal amplitude with a 0 dB ceiling,
using ES8311 register 0x32's 0.5 dB steps. The default 75% selection maps to
approximately -2.5 dB, replacing the earlier raw register-percentage behavior.

The user reported one UI increment per two encoder notches. `0.2.5-rotary`
changed decoding to two quadrature edges per notch and preserved partial signed
movement. Host tests cover reversals/bounce; physical confirmation is recorded
separately from those tests.

## Clock, lighting and USB baseline

`0.3.0-clock` introduced the Inter/NTP clock and saved server/timezone. The face
was subsequently simplified to hours/minutes with a sync-dependent colon; the
weekday/date/timezone/seconds/status text and decorative ring were removed.

`0.3.1-clock-usb` combines clock, lighting, diagnostics, WiFi, USB and OTA on one
web page. USB CDC-ECM uses its own lwIP interface and a one-client DHCP server,
without advertising a gateway or DNS. Serial/JTAG remains a selectable mode.
An early USB build panicked with DHCP packet buffers on the small lwIP task
stack; static bounded buffers corrected that failure. USB stages are retained
for diagnosis. Enumeration, link activation, DHCP and concurrent peripheral/NTP
operation were subsequently observed.

The dated [development baseline](../development.md#verified-baseline-and-open-items)
separates those observations from the factory-code findings and lists open items.

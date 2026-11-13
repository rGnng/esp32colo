# esp32colo

Cololight / ColoPLAY ESP32-S3 firmware exploration.

The initial [firmware feasibility report](cololight-firmware-report.html) covers the published ColoPLAY V1 hardware, USB data wiring, flash-mode requirements, and the proposed build workflow. Open the HTML file locally in a browser to read it.

The physical unit matches the published design: **ESP32-S3-WROOM-1-N16R8**, **GC9A01** 240×240 round LCD, and **ES8311** audio codec. A full 16 MB factory backup was taken before any writes.

A minimal WiFi/OTA bring-up firmware is flashed and OTA-verified (`0.1.3-nvs`). WiFi credentials are stored in NVS (seeded from `secrets.h` on first boot) and can be changed over HTTP or the USB serial console. Display, audio and input drivers are not implemented yet.

An experimental [LCD probe](firmware/lcd_probe/README.md) can still be loaded from RAM through the ROM downloader for controller experiments, without overwriting factory flash.

## Working documents

- [GPIO map](docs/gpio-map.md) — display, audio, buttons, LEDs and power signals read from the published schematics.
- [Factory firmware](docs/factory-firmware.md) — full 16 MB backup, partition table and firmware component analysis.
- [Flash guide](docs/flash.md) — backup, build and flash procedure.
- [Firmware](firmware/) — minimal PlatformIO bring-up image (WiFi + status page + OTA).

Manufacturer documentation: [ColoPLAY hardware](https://x.cololight.com/mweb/docs/en/opensource/coloplay_hw/).

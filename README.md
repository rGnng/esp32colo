# esp32colo: ColoPlay firmware proof of concept

<img src="firmware/web/coloplay.svg" width="96" height="96" alt="ColoPlay project icon">

esp32colo began as a proof of concept for rebuilding the firmware of the
**LifeSmart/ColoLight ColoPlay C169**, an ESP32-S3 desktop controller. Its code
and development history have now been merged into
[MPD Tab5](https://github.com/rGnng/mpdtab5). This repository is archived and
retained as a reference for that integration and other ColoPlay projects.

Active development continues in
[MPD Tab5's ColoPlay directory](https://github.com/rGnng/mpdtab5/tree/main/coloplay),
with common protocol and volume definitions in
[shared/](https://github.com/rGnng/mpdtab5/tree/main/shared). See the
[migration record](https://github.com/rGnng/mpdtab5/blob/main/docs/coloplay-migration.md)
for the repository layout, preserved history and validation.

## Reference material

- [Factory firmware findings and backup/restore procedure](docs/factory-firmware.md)
- [Factory-derived GPIO map](docs/gpio-map.md)
- [GPIO evidence](docs/firmware-gpio-evidence.md) and [LED transmitter evidence](docs/firmware-led-evidence.md)
- [Firmware behavior and configuration](docs/firmware.md)
- [Build, flashing and recovery](docs/flash.md)
- [Development findings and verified releases](docs/development.md)
- [Tab5 integration and macOS volume findings](docs/tab5-integration.md)

The last standalone baseline is **0.6.33-navigation-nvs** at
[`0c741ec`](https://github.com/rGnng/esp32colo/commit/0c741ec2b0ca11095859999460a1cdcd99778949).
The [README at that revision](https://github.com/rGnng/esp32colo/blob/0c741ec2b0ca11095859999460a1cdcd99778949/README.md)
preserves the full feature overview. Documentation in this archive describes
that baseline; use MPD Tab5 for current source, builds and updates.

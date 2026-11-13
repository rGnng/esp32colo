# esp32colo

Cololight / ColoPLAY ESP32-S3 firmware exploration.

The initial [firmware feasibility report](cololight-firmware-report.html) covers the published ColoPLAY V1 hardware, USB data wiring, flash-mode requirements, and the proposed build workflow. Open the HTML file locally in a browser to read it.

The physical board variant remains unverified. No firmware has been run on the device.

An experimental [LCD probe](firmware/lcd_probe/README.md) attempts controller ID/status reads using the reference wiring. It is designed to run from RAM through the ROM downloader, without overwriting factory flash. Panel readback and exact controller identification are not guaranteed.

Manufacturer documentation: [ColoPLAY hardware](https://x.cololight.com/mweb/docs/en/opensource/coloplay_hw/).

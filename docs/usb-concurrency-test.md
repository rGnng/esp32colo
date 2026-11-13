# USB CDC + HID + JTAG experiment

This is an opt-in experiment, separate from the `coloplay` production build.
The host checks must run on the computer physically connected to the gadget;
WiFi/HTTP telemetry alone cannot prove USB enumeration or a JTAG TAP scan.

Here CDC means the existing **CDC-ECM network** function. The fixed USB
Serial/JTAG controller also exposes CDC-ACM serial, but is a separate controller.
The test adds a vendor HID with a harmless feature-report echo. It sends no
keyboard, mouse or consumer-control input.

ESP32-S3 USB-OTG supports composite classes. The fixed Serial/JTAG controller
shares its internal PHY with USB-OTG. Espressif documents an external PHY and
another USB connection for simultaneous operation of both controllers; simply
initializing both drivers does not supply another PHY. See the
[USB device guide](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html).
This experiment measures the behavior on this unit without changing eFuses.

## Recovery preparation

Before replacing firmware, capture `/api/status`, and copy the exact running,
validated application into an ignored, private recovery bundle. Its manifest
records version, length, SHA-256, MD5 and WiFi control URL. Do not substitute a
newly compiled image with the same version string. Firmware contains compiled
credentials, so keep the bundle private.

The portable bundle contains `known-good.bin`, `manifest.json`,
`recover_usb.py`, `check_usb_concurrency.py` and `requirements-host.txt`.
On the USB host, extract it and prepare tools:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-host.txt
.venv/bin/python -m platformio pkg install --global --tool platformio/tool-openocd-esp32
.venv/bin/python check_usb_concurrency.py --preflight
```

The preflight checks the local recovery image, Python dependencies and OpenOCD.
It does not change the device. `--openocd /path/to/openocd` overrides discovery;
use Espressif OpenOCD with `board/esp32s3-builtin.cfg` available.

For a bootloop: remove all power, including the battery if fitted; hold REC and
the center knob, reconnect USB in ROM download mode, then run from the bundle:

```bash
.venv/bin/python recover_usb.py --timeout 120
# If several ESP devices are attached:
.venv/bin/python recover_usb.py --port /dev/cu.usbmodemYOUR_PORT
```

The script validates the manifest and complete ESP32-S3 application before any
flash command. It writes and verifies the same known-good application at
`0x20000` and `0x4a0000`, with flash parameters kept. Both slots are restored
because either may be selected after OTA. It preserves bootloader, both
partition tables, NVS and OTA metadata, and leaves the chip in ROM mode. Release
the controls and power-cycle normally afterward. `--dry-run` prints the exact
esptool commands. No erase-flash or first-install bootstrap is needed.

## Build and run

From the development checkout:

```bash
.venv/bin/pio run -d firmware -e usb-concurrency
./scripts/ota_upload.sh http://DEVICE_WIFI_IP firmware/.pio/build/usb-concurrency/firmware.bin
```

The image identifies as `0.4.1-usb-concurrency-test`. Its initial phase retains
fixed Serial/JTAG, even if the stored production USB mode is Ethernet. All
experimental mode choices remain in RAM. The normal `coloplay` environment
omits the vendor HID, experiment routes and mux-switching test code.

After hardware startup, run on the attached USB host:

```bash
.venv/bin/python check_usb_concurrency.py --url http://DEVICE_WIFI_IP
```

Use WiFi for control; the USB network will disconnect during the experiment.
Close serial monitors/debuggers first. On macOS, allow the accessory connection
if the OS prompts. The script never opens the serial port or toggles its DTR.
Automatic target selection requires one native `303a:1001` serial port matching
the gadget's WiFi MAC. If the host reports a different serial format, inspect
its inventory and pass `--jtag-serial EXACT_SERIAL` explicitly.

1. **Baseline:** inventory native USB and serial devices; use OpenOCD to scan
   the exact target's JTAG TAP. If it fails, save the report without arming the
   CDC/HID phase. The scan does not request reset or flash; it resumes the CPU
   if target examination halted it.
2. **Coexist attempt:** keep fixed Serial/JTAG initialized while starting
   CDC-ECM and vendor HID on USB-OTG (`303a:4003`). Perform SET/GET feature echo
   (report ID 6, 63-byte payload), obtain the DHCP lease and fetch status over
   USB HTTP, verifying image and MAC, then repeat the exact-target JTAG scan.
   Record actual PHY mux registers before and after the checks.
3. **Mux back:** retain the initialized OTG stack but route the internal PHY
   back to Serial/JTAG. Repeat JTAG, USB inventory and HID checks. Skip USB HTTP
   when the OTG PHY route is absent, avoiding traffic over an unrelated route.

The test restores fixed Serial/JTAG at the end. A 180-second volatile watchdog
also switches back if the host stops during the coexist phase; a reboot returns
to baseline. This is separate from the application's image-scoped recovery
counter. It is not an automatic known-good firmware rollback.

The script writes private `usb-concurrency-report.json` and copies it to RAM on
the gadget. `all_three_same_phase` requires successful USB HTTP, HID echo and
JTAG TAP scan while the phase and OTG PHY ownership remain unchanged. Missing
tools, API flags or drivers merely being initialized cannot constitute a pass.

Experiment-only routes:

- GET/POST `/api/usb-experiment`: read actual PHY ownership/stack counters or
  schedule `mode=coexist` / `mode=jtag` after the HTTP response.
- GET/POST `/api/usb-experiment/report`: retrieve/store a bounded host report
  in RAM; it is lost on reboot and does not write NVS.

Collect the report before restoring the exact known-good image over WiFi OTA.
Check its original MD5, hardware readiness and saved settings after reboot.
Host evidence is pending until the attached computer runs these checks; the
expected PHY limitation is documented separately from measured results.

## Status of the staged run

On 2026-11-13 the experiment image booted in Serial/JTAG baseline mode. No host
report was received before the user requested replacing it with production
`0.4.2-scene-lighting` over OTA. Therefore this run has **no measured concurrent
CDC/HID/JTAG result**. The host test requires staging the experiment again;
its original recovery bundle contains v1 scene firmware and is not a rollback
of the newer v2 scene configuration.

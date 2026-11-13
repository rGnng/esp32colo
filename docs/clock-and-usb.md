# Clock and USB networking

The clock, hardware pages, USB configuration and HTTP OTA share one firmware
and one web page. WiFi continues to provide the device's Internet connection.

## Clock

The clock opens at boot and stays visible until a key or web control changes
the page. KEY1 returns to the clock. KEY2–4 select Inputs, Audio and Sensors;
Orbit remains available in the web page. Pressing the knob enables or disables
the automatic tour across all five pages.

The screen uses Inter 4.1, with antialiased 64 px tabular digits, 18 px weekday
and date, and 14 px timezone / seconds / sync status. The clock refreshes at
5 fps; the hardware pages retain their 20 fps target. Clock updates do not
block the controls, audio, LEDs, web server or OTA.

In the web page's **Clock** section, enter an NTP server hostname or IPv4
address, choose a timezone, then select **Save and sync**. Defaults are
`pool.ntp.org` and UTC. The device stores both values atomically in NVS and
applies changes immediately. **Sync now** requests another synchronization
without changing settings. An unsupported timezone or a URL containing a
scheme, path or port is rejected before saving.

SNTP refreshes hourly and restarts after WiFi reconnects. Time synchronization
uses WiFi, including while the Mac is connected by USB. The display shows
`--:--` until system time is valid, and distinguishes WiFi offline / awaiting
NTP / synchronized. A running clock continues through a network interruption.
No time is fabricated when an NTP response is unavailable.

Supported zones include UTC, Hong Kong, Shanghai, Taipei, Tokyo, Singapore,
Kolkata, Los Angeles, New York, London, Berlin, Sydney and Auckland. The listed
daylight-saving zones use embedded POSIX rules, not a complete IANA database.

```bash
curl http://coloplay.local/api/clock
curl -X POST http://coloplay.local/api/clock \
  --data-urlencode ntp_server=pool.ntp.org \
  --data-urlencode timezone=Asia/Shanghai
curl -X POST http://coloplay.local/api/clock/sync
```

`GET /api/clock` includes the available timezone choices. The `clock` object in
`GET /api/status` includes current time, last NTP synchronization, synchronization
count, stored settings and SNTP state. Settings use the `clock` NVS namespace;
WiFi credentials and other settings remain in their own namespaces.

The unmodified font and its OFL-1.1 license are in
[`assets/fonts/inter`](../assets/fonts/inter). The source is the
[official Inter 4.1 release](https://github.com/rsms/inter/releases/tag/v4.1).
`InterVariable.ttf` SHA-256:
`4989b125924991b90d05b2d16e0e388c48f7d5bb8b30539bbf9c755278d0ccaf`.
Generated subsets are compiled into flash; no filesystem upload is required.
To regenerate them:

```bash
.venv/bin/python -m pip install Pillow==12.0.0
.venv/bin/python scripts/generate_clock_fonts.py
```

## USB on macOS

In **USB** on the same web page, select **USB Ethernet (macOS / Linux)**.
Connect the USB-C data port to the Mac and open `http://172.32.29.21/`.
The native macOS CDC-ECM driver provides the network interface.

| Setting | Default |
|---|---|
| Device address | `172.32.29.21/24` |
| Host DHCP lease | `172.32.29.30` only |
| Gateway | None |
| DNS | None |
| Lease lifetime | 3600 seconds |

The device address, prefix and host lease address are configurable. The two
addresses must be distinct usable hosts in the same subnet; prefixes 1–30 are
supported. Saving commits one NVS record in namespace `usbnet` and restarts
the device. The server offers exactly one address to one client, supports
renewal/release/expiry and omits router, DNS and classless-route DHCP options.
DHCP is bound to the USB interface. USB does not replace the WiFi default route.

USB Ethernet and the fixed USB serial/JTAG controller share the S3's internal
PHY. They are separate selectable modes on this connector. Choose **USB serial
+ JTAG** in the same web page and save to restore the hardware serial/debug
device after reboot. No eFuse is changed. Holding KEY1 at boot or triggering
the existing failed-start recovery also keeps serial/JTAG and WiFi OTA available.
See [Espressif's USB device documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html).

`GET /api/usb` and the `usb` object in `/api/status` report configured / active
mode, link state, RX/TX counters, DHCP offers/ACKs, lease ownership and startup
errors. The web page reports the same state.

```bash
curl http://coloplay.local/api/usb
# This saves settings and restarts the device:
curl -X POST http://coloplay.local/api/usb \
  -d 'mode=ethernet&address=172.32.29.21&prefix=24&dhcp_address=172.32.29.30'
```

On the Mac, identify the USB interface with `networksetup -listallhardwareports`.
For its `enX` device, `ipconfig getifaddr enX` should print `172.32.29.30` and
`ipconfig getpacket enX` should contain the lease and subnet mask without a
router option. `curl http://172.32.29.21/api/status` should return the same
firmware and clock seen over WiFi. Use a data cable if the interface does not
appear; select DHCP for that network service if it has an existing manual setup.

## Driver and checks

Arduino 2.0.17's precompiled TinyUSB core omits its built-in network class.
The firmware registers the pinned TinyUSB 0.16 ECM driver through the supported
application-class callback and supplies ECM descriptors. Ethernet frames enter
the existing lwIP stack through a separate netif. Transmit packets are bounded
and queued to the USB task; endpoint operations never run on the TCP/IP task.
No Arduino SDK or factory bootloader changes are needed.

The vendored network driver and its original copyright notices are in
[`firmware/lib/tinyusb_ecm`](../firmware/lib/tinyusb_ecm). Only ECM is advertised;
RNDIS support files are retained to build the upstream combined driver unchanged.

The DHCP callback uses static bounded packet buffers because lwIP's network task
has a small stack. An earlier build panicked after USB startup with stack-local
buffers; the updated build has passed USB enumeration, ECM link activation,
DHCP offer / ACK and concurrent NTP / display / audio operation. USB startup
stages are retained across warm resets and exposed in telemetry. Clock settings
(`pool.ntp.org`, `Asia/Shanghai` on this unit) survived the OTA / reboot sequence.

`./scripts/test_http_ota.sh` covers clock setting validation and timezone rules,
USB subnet validation, DHCP packet contents, sole lease ownership, renewal,
release, decline, expiry, timer wrap and malformed/truncated packets, in addition
to the existing firmware / OTA / hardware checks. The DHCP codec also passes
AddressSanitizer and UndefinedBehaviorSanitizer checks on the host.

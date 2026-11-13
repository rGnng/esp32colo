# TinyUSB ECM driver

Pinned upstream: [TinyUSB 0.16.0](https://github.com/hathach/tinyusb/tree/0.16.0).
The upstream source files are unmodified:

| Local file | Upstream path | SHA-256 |
|---|---|---|
| `src/ecm_rndis_device.c` | `src/class/net/ecm_rndis_device.c` | `ab38db2cfdc05e90f88e58ce0890fe589579ec1eea594a946900068352d7f89c` |
| `src/rndis_reports.c` | `lib/networking/rndis_reports.c` | `3942dc8a1e8c4bab8f92e2710cc9ff42ddc947c512b3e611eaf2339204f40ce4` |
| `src/rndis_protocol.h` | `lib/networking/rndis_protocol.h` | `498e1f5e85836cb54e7a9165a04a387378b4af54d8054d8ce40f548eff61f01b` |
| `src/ndis.h` | `lib/networking/ndis.h` | `13cec2d64dd04e026ffe9763fc4445422586b7646903c9e99103d962de8879a7` |

The two small local headers route includes to the Arduino SDK's matching
TinyUSB 0.16 headers and make the driver discoverable by PlatformIO.
`usb_network.cpp` registers the driver through `usbd_app_driver_get_cb`; the
precompiled SDK has no built-in ECM class enabled. Only ECM is advertised.

The C implementation files use the MIT license, reproduced in `LICENSE` and
their source headers. `rndis_protocol.h` retains its BSD-3-Clause notice;
`ndis.h` retains its upstream public-domain dedication. Keep these notices with
source or binary distributions. The combined driver's RNDIS dependencies are
included for building the unmodified upstream source, not as an advertised USB
function in this firmware.

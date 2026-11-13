# HTTP API

This reference describes the current implementation in
[main.cpp](../firmware/src/main.cpp),
[hardware_demo.cpp](../firmware/src/hardware_demo.cpp),
[clock_service.cpp](../firmware/src/clock_service.cpp) and
[usb_network.cpp](../firmware/src/usb_network.cpp).
The same server is reachable over WiFi and USB Ethernet; substitute the actual
address in these examples. Requests use port 80 and form-encoded POST fields,
not JSON request bodies. Successful configuration responses are JSON.

## Routes and authentication

| Method / path | Purpose | Authentication / result |
|---|---|---|
| `GET /` | Shared settings and diagnostics page | None; HTML |
| `GET /assets/<name>-<hash>.css` / `.js` | Local static styles/scripts | None; gzip, immutable cache |
| `GET /api/status` | Full device telemetry | None; JSON |
| `GET /api/log` | Bounded application log; optional `after` byte cursor | None; JSON, no-store |
| `GET /api/scenes` | Saved scene catalog, bindings, limits and state | None; JSON |
| `POST /api/scenes` | Scene CRUD, binding saves, activation and saved-gesture execution | None; scene config JSON |
| `POST /api/lighting` | General lighting, audio test and sleep controls | None; hardware JSON |
| `POST /api/demo` | Compatibility alias for lighting controls | None; hardware JSON |
| `GET /api/clock` | Clock status/settings, supported zones | None; JSON |
| `POST /api/clock` | Save NTP server/timezone, request sync | None; clock JSON |
| `POST /api/clock/sync` | Request sync with stored settings | None; clock JSON |
| `GET /api/usb` | USB settings and network state | None; JSON |
| `POST /api/usb` | Save USB settings and restart | None; JSON with `rebooting` |
| `GET /api/wifi` | Stored SSID and source | None; never returns password |
| `POST /api/wifi` | Save WiFi credentials and restart | None; JSON with `rebooting` |
| `GET /update` | Firmware upload page | Basic auth: `admin` / `OTA_PASSWORD` |
| `POST /update` | Application-only multipart upload | Same Basic auth; JSON with `updated`, `rebooting` |
| `POST /api/hardware/retry` | Clear failed-start counter and restart | Same Basic auth; JSON with `retrying`, `rebooting` |
| `POST /reboot` | Restart without resetting settings | None; plain text |

This table reflects the current access model: configuration/reboot routes do
not inherit OTA authentication. HTTP is not encrypted. Configuration validation
errors return 400 with an `error` field; WiFi persistence failure returns 500.
Clock, USB and hardware configuration currently report persistence errors as
400 as well. An unknown route returns 404.

## Status and telemetry

```bash
curl http://coloplay.local/api/status
```

| Object / field | Meaning |
|---|---|
| `firmware`, `sketch_md5`, `running_partition` | Version, exact running image identity and OTA slot |
| `uptime_ms`, `reset_reason`, `boot_attempts`, `recovery_mode` | Runtime and failed-start recovery state |
| `flash_bytes`, `psram_bytes`, `free_heap` | Detected/enabled memory and free internal heap |
| `wifi` | Connection, stored/current SSID, IP, RSSI, MAC; no password |
| `buttons` | Debounced names, pins, pressed state and counters after hardware startup; early raw inputs during startup/recovery |
| `clock` | Primary/fallback servers, valid local/Unix time, sync history and SNTP state |
| `scene` | Active scene, standby/current screen, NVS state, last event/action/result and temporary feedback |
| `usb` | Configured/active mode, stages, link, counters, DHCP and lease state |
| `hardware` | Startup stage, page/tour, awake/paused state and peripheral objects |
| `hardware.display` | Framebuffer, frame/FPS/TE counters, selected/automatic/effective screen brightness |
| `hardware.encoder` | Raw edges, decoded position, pin levels and `edges_per_detent=2` |
| `hardware.leds` | Ring/key brightness and Auto flags, effective levels, mode/color and SPI transfer errors/counters |
| `hardware.lighting` | NVS `stored`, `auto_dark_raw`, `auto_bright_raw`, `sensor_usable` |
| `hardware.sensors` | `vdd_raw`, approximate `vdd_mv_approx`, `light_raw`, `light_mv`, light result/counts and charge logic |
| `hardware.psram` | Pattern-test result/size and free PSRAM |
| `hardware.audio` | Codec/I2S readiness, volume, microphone levels, frame/error counters and tone state |

Screen fields are `brightness`, `auto_brightness`, `effective_brightness`.
LED fields are `ring_brightness`, `keys_brightness`, `auto_ring`, `auto_keys`,
`ring_effective`, `keys_effective`, `mode`, `color`. These brightness values are
percentages except the legacy `hardware.leds.brightness`, which reports the
ring's selected 0–255 byte level.

A failed light sample retains the last good raw/mV values. Check `light_error`,
`light_readings` and `light_errors` rather than interpreting a retained value as
a fresh measurement. Before a successful read, `light_raw` is -1 and the read
count is zero. `sensor_usable` is the Auto controller's availability flag: the
last good sample is retained for up to five seconds. Values are not lux.
`vdd_mv_approx` is the divided supply rail estimate, not battery state of charge.

Screen capture is disabled in 0.4.3: `/api/screenshot.bmp` returns 404 and the
web interface no longer requests display images.

## Scenes and gesture bindings

`GET /api/scenes` returns `state`, `limits`, recognizer `timing`, the input/
gesture/action/effect `catalog`, and `scenes`. Each scene has a stable numeric
`id`, `name`, `protected`, `standby_screen`, `ring_mode`, `ring_color`,
`keys_mode`, `keys_color`, and sparse `bindings` list. Each
binding contains `input`, `gesture`, `global` and an ordered `actions` array. NVS stores
one version-3, 3304-byte record: 8 scenes, 64 configured gestures and up to 3
actions per gesture. The active scene is persistent.

Ring and key LED modes independently accept `general`, `rainbow` or `static`;
colors use `#RRGGBB`. `catalog.lighting_modes` lists these choices. Omitted
lighting fields retain their saved values, including during rename requests.
`state` and `/api/status.scene` include the active scene's same lighting and icon fields.
`hardware.display.scene_overlay` is true while the switch icon/name is displayed.
Brightness/Auto can inherit General or be overridden per scene; action LED
feedback overlays scene lighting, then expires back to it. Legacy v1 scenes migrate with both groups in
`general`, retaining all names, IDs, bindings and the active selection.

`POST /api/scenes` uses these form operations:

| `op` | Fields | Result |
|---|---|---|
| `create` | `name` | Create an empty scene in a free slot |
| `update` | `scene_id`, `name`, optional screen/color, `icon` and brightness fields below | Save scene settings atomically |
| `delete` | `scene_id` | Delete; active/scene-switch targets fall back to Default |
| `activate` | `scene_id` | Persist active selection and clear pending input/effects |
| `binding` | `scene_id`, `input`, `gesture`, `count`, optional `global=0/1`, action fields below | Atomically replace the complete ordered binding; count 0 clears |
| `run` | `scene_id`, `input`, `gesture` | Execute the saved binding in the active scene; requires hardware ready |

Scene 0 is protected: it must exist, remains named **Default**, and cannot be
deleted. Names are distinct, nonempty, at most 24 UTF-8 bytes, with no control
characters. New scenes have no local bindings; marked Global Default bindings
remain available.
Deleting a scene clears its bindings and retargets references to scene 0 before
saving. Invalid requests or NVS write failures do not apply a partial update.

Additional scene fields are `icon` (one of `catalog.icons[].id`) and, for each
`screen`, `ring`, `keys` channel: `<channel>_brightness_mode=general|override`,
`<channel>_brightness` (0–100), `auto_<channel>` (form 0/1; JSON boolean).
Omitted fields are retained. Icon names are fixed to the 32 locally bundled
Bootstrap choices. Brightness actions update the enabled scene override or the
General setting; override actions debounce storage for 1.5 seconds. `state.revision`
changes for brightness edits without cancelling gestures; `state.epoch` changes
for configuration/scene switches. Polling clients should observe both.

Combination input strings are canonical `key1+key2`, `key1+key3+key4`, etc.,
using 2–4 distinct front buttons. Input order is accepted in either direction;
responses use button order. Knob/OPEN/REC combinations and rotation gestures on
combinations are rejected. `catalog.combo_inputs` lists supported members and
`timing.combo_window_ms` is 80. There is no GPIO-injection endpoint; `op=run`
dispatches a saved logical gesture without testing physical recognition.

`global=1` is allowed only for scene 0. Lookup first checks the active scene,
then Global Default. `last_event.source_scene` and `last_event.global` identify
the resolved binding. Global and local bindings count toward the same 64-row
limit; each scene may override a Global pair. The web editor warns rather than
rejecting that deliberate shadowing. Legacy v1/v2 records migrate with preserved
settings, Use General brightness, House icons and Global off.

For table editing, `binding` also accepts optional `source_input` and
`source_gesture`. These identify an existing row to move/replace atomically;
a missing source or occupied destination is rejected. New/duplicated rows can
set `create_only=1` to reject an already occupied key/event pair instead of
using legacy upsert behavior. Duplicates omit source fields and preserve the
source row. No NVS schema change is needed. A zero-count move deletes only its
source, without clearing an unrelated destination.

Inputs are `key1`, `key2`, `key3`, `key4`, `knob`, `open`, `rec`. All support
`single`, `double`, `triple`, `hold`, `hold_release`. Only `knob` also supports
`left`, `right`, `press_left`, `press_right`. The run operation executes the
binding, not the physical recognizer; see [gesture semantics](firmware.md#gestures).

For each action index 0–2 send `a0_type`, `a0_value`, `a0_target`, `a0_led`,
`a0_screen`, `a0_color`, `a0_feedback_ms` (then `a1_…`, `a2_…` as needed):

| Field | Values |
|---|---|
| `type` | `none`, `next_scene`, `previous_scene`, `switch_scene`, `screen_change`, `screen_set`, `ring_change`, `ring_set`, `keys_change`, `keys_set`, `volume_change`, `volume_set`, `rainbow`, `static`, `toggle_awake`, `sync_time`, `tone` |
| `value` | Adjustments -100…100 per step; setters 0…100%; tone 100…4000 Hz |
| `target` | Existing scene ID for `switch_scene` |
| `led` | `inherit`, `proportional`, `rainbow`, `static`, `off` |
| `screen` | `unchanged` or `clock` |
| `color` | `#RRGGBB`; initial effect color `#40ffaa` |
| `feedback_ms` | 250…10000; default 1500 |

Proportional feedback is valid only for percentage-producing brightness/speaker
actions. It fills the ring from their returned percentage; key LEDs keep General
lighting. Other LED effects affect both groups at their configured brightness.
Temporary effects expire back to General lighting and the scene's standby
screen. Screen overrides route through the scene service; Clock is currently
the only implementation. Scene switching ends the gesture's action list.
Computer volume/data integration is deferred to the companion-app phase.

```bash
curl -X POST http://coloplay.local/api/scenes -d 'op=create&name=Work'
# Substitute the returned scene ID:
curl -X POST http://coloplay.local/api/scenes -d 'op=activate&scene_id=1'
curl -X POST http://coloplay.local/api/scenes \
  -d 'op=binding&scene_id=1&input=knob&gesture=right&count=1' \
  -d 'a0_type=volume_change&a0_value=2&a0_led=proportional&a0_screen=clock' \
  --data-urlencode 'a0_color=#40ffaa'
curl -X POST http://coloplay.local/api/scenes \
  -d 'op=run&scene_id=1&input=knob&gesture=right'
```

`state.last_event` reports a sequence, timestamp, input, gesture, last action and
last percentage (-1 when unavailable). `feedback` reports the active LED effect,
color and percentage. `epoch` advances on saved scene changes and is used to
cancel pending gestures; `stored` confirms the NVS record. Hardware actions
that change lighting selections debounce NVS writes for 1.5 seconds.

## Hardware and lighting controls

`POST /api/lighting` accepts any subset of these form fields. It requires
`hardware.ready`; otherwise it returns 400. Unspecified fields retain their
current values. Validation happens before configuration is applied; changed
persistent lighting settings must be saved successfully first.

| Field | Values | Persistence / behavior |
|---|---|---|
| `page` | Legacy 4 (Clock) only | Other diagnostic pages are retired |
| `auto` | Legacy 0 only | Automatic page tour is retired; use scene gestures |
| `brightness` | Integer 0–100 | Saved screen selection; initial 70 |
| `ring_brightness` | Integer 0–100 | Saved ring selection; initial 70 |
| `keys_brightness` | Integer 0–100 | Saved key LED selection; initial 70 |
| `auto_screen`, `auto_ring`, `auto_keys` | 0 or 1 each | Independent saved Auto flags; initially 0 |
| `led_mode` | `rainbow` or `static` | Saved; initial `rainbow` |
| `led_color` | `#RRGGBB`, six hexadecimal digits | Saved; initial `#ffffff`; used in static mode |
| `auto_dark_raw`, `auto_bright_raw` | Integers 0–4095, distinct | Saved together with lighting; initial 0 / 1200; either polarity |
| `volume` | Integer 0–100 | Runtime speaker amplitude; initial 75; 0 mutes, 100 is 0 dB gain |
| `tone` | Integer 100–4000 Hz | Play a tone; omitted means no tone |
| `duration` | Integer 30–1500 ms | Tone duration; default 250 for this request |
| `awake` | 0 or 1 | Runtime screen/LED sleep; web/OTA remain available |
| `led_test` | 0 stop, 1 white, 2 red, 3 green, 4 blue | Temporary five-second test, then configured mode; brightness selections are unchanged |
| `leds` | Legacy integer 0–64 | Saved compatibility command; sets both ring and keys using the old byte scale |

Legacy `leds` values are converted by `(value * 100 + 127) / 255` using integer
arithmetic; `leds=64` is approximately 25%, not 100%. Prefer the independent
percentage fields. If both legacy and new fields are supplied, the new field
for each group takes precedence. `led_test=0` resumes the configured LED mode,
which can be static or rainbow. Frame slot 13 stays off in all current modes.

```bash
curl -X POST http://coloplay.local/api/lighting \
  -d 'brightness=70&ring_brightness=70&keys_brightness=70&auto_screen=0&auto_ring=0&auto_keys=0'
curl -X POST http://coloplay.local/api/lighting \
  -d led_mode=static --data-urlencode 'led_color=#ff8040'
curl -X POST http://coloplay.local/api/lighting \
  -d 'auto_screen=1&auto_dark_raw=0&auto_bright_raw=1200'
curl -X POST http://coloplay.local/api/lighting -d 'page=4&auto=0'
```

## Clock

`GET /api/clock` includes `zones` with supported IDs/labels. Labels show UTC
offsets; cities with daylight saving list standard and daylight offsets. Fixed
IDs such as `UTC-12:00`, `UTC+08:00` and `UTC+05:30` are accepted alongside
existing city IDs. Fixed choices include whole hours -12 through +14 (zero
continues to use `Etc/UTC`) and +05:30. The clock NVS schema is unchanged.
`POST /api/clock` accepts `ntp_server`, `ntp_server2`, `ntp_server3` and
`timezone`, requiring at least one field. The primary server is required/nonempty;
the two fallbacks may be empty. Partial updates preserve omitted fields. The server is a hostname/IPv4 literal of at most 253 characters,
without a scheme/path/port. The zone must match a returned ID. All fields are
committed as one version-2 NVS record, applied immediately and followed by a sync request.
Changing any server clears `synced` until the next NTP response.

```bash
curl http://coloplay.local/api/clock
curl -X POST http://coloplay.local/api/clock \
  --data-urlencode ntp_server=pool.ntp.org \
  --data-urlencode timezone=Asia/Shanghai
curl -X POST http://coloplay.local/api/clock/sync
```

`ntp_servers` returns all three configured slots; `ntp_server` retains the
primary value for older clients. Version-1 primary/timezone records migrate
without discarding existing settings. Empty slots are skipped when configuring SNTP.

`time_valid` means system time is at least 2024-01-01. `synced` means an NTP sync
has succeeded since boot or the last server change; it does not mean WiFi is
currently connected. `last_sync_unix`, `sync_count` and `sntp_started` provide
additional diagnostics. `local_time` is an ISO-like date/time/offset string;
the physical face only renders hours/minutes and the sync-dependent colon.

## USB settings

`POST /api/usb` accepts one or more of `mode` (`ethernet` or `serial_jtag`),
`address`, `prefix` and `dhcp_address`. Addresses must be distinct usable IPv4
hosts in the same subnet, with prefix 1–30. Saving commits one NVS record and
restarts the device. If device IP/prefix changes without an explicit host
address, the DHCP peer is computed from the new subnet. Initial settings are
172.32.29.1/30, peer 172.32.29.2. Only the exact previous defaults migrate;
custom networks are preserved. Defaults and connection instructions are in the
[firmware guide](firmware.md#usb-networking-and-serialjtag).

```bash
curl http://coloplay.local/api/usb
# Saves settings and restarts:
curl -X POST http://coloplay.local/api/usb \
  -d 'mode=ethernet&address=172.32.29.1&prefix=30'
```

Telemetry distinguishes configured `mode` from `active_mode`, including the
serial/JTAG recovery override. `ready`, `connected`, `link_up`, `error`, `stage`
and `last_boot_stage` distinguish startup from host enumeration/link state.
RX/TX/drop counters, DHCP offers/ACKs/NAKs, `lease_active` and `client_mac` expose
network activity; gateway and DNS are null because neither is advertised.

## WiFi settings

`POST /api/wifi` requires a nonempty `ssid` of at most 32 characters after
trimming; `password` is at most 63 characters. Missing/empty password keeps the
stored value, so this endpoint cannot clear an existing password. Saving
restarts the device. `GET /api/wifi` returns the stored SSID and source.
Use the [serial recovery commands](flash.md#recovery) if the device becomes
unreachable with incorrect credentials.

## Recovery and reboot

`POST /reboot` restarts without changing settings. To clear failed-start recovery
and retry the same image, use Basic-authenticated `POST /api/hardware/retry`:

```bash
# curl prompts for the OTA password; it is not included in shell history.
curl --user admin -X POST http://coloplay.local/api/hardware/retry
```

Both restart the device. Details of automatic recovery, KEY1 and ROM download
mode are in [Flashing and recovery](flash.md#recovery).

## Firmware uploads

`POST /update` accepts one multipart application file, with optional `X-MD5`.
The scripted uploader supplies the header and verifies the running image after
reboot; prefer the [documented upload workflow](flash.md#normal-updates).
Authentication is checked before flash writes. Wrong-chip, bootloader/merged,
truncated, aborted, invalid-digest and incomplete uploads are rejected. A missing
configured OTA password returns 503. Accepted uploads return
`{"updated":true,"rebooting":true}` before rebooting into the selected slot.

## Application log

`GET /api/log` returns `boot` (per-boot identifier), `capacity` (4096 bytes),
`next` (cursor after this chunk), `total` (current end cursor), `reset` and
`text`. Responses contain up to 2048 bytes of text and escape JSON control
characters. Follow with `?after=<next>` until `next == total`, then use the same
cursor for incremental reads. A cursor outside the retained window sets `reset`
and returns from the oldest retained byte. Invalid, negative or oversized
32-bit cursor values return 400.

Compare `boot` between responses: on change, discard the old cursor and fetch
without `after`. Log text is RAM-only, overwritten at capacity and lost on
reboot; the API does not log its own reads. There is no persistence or erase
operation. Web Download runs in the browser and saves the loaded text without
another full-log transfer.

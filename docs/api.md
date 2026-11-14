# HTTP API

This reference describes the current implementation in
[main.cpp](../firmware/src/main.cpp),
[hardware_demo.cpp](../firmware/src/hardware_demo.cpp),
[general_service.cpp](../firmware/src/general_service.cpp),
[scene_service.cpp](../firmware/src/scene_service.cpp),
[clock_service.cpp](../firmware/src/clock_service.cpp) and
[usb_network.cpp](../firmware/src/usb_network.cpp).
The current release is **0.6.6-input-sampling**; deployment fingerprints are in the
[release record](development.md#current-verified-release). Bluetooth's BLE wire
contract and Tab5 setup are documented in the [integration guide](tab5-integration.md).
Serial companion framing remains proposed.
The same server is reachable over WiFi and USB Ethernet; substitute the actual
address in these examples. Requests use port 80 and form-encoded POST fields,
not JSON request bodies. Successful configuration responses are JSON.

## Routes and authentication

| Method / path | Purpose | Authentication / result |
|---|---|---|
| `GET /` | Shared settings and diagnostics page | None; HTML |
| `GET /assets/<name>-<hash>.<ext>` | Local CSS, JavaScript and SVG assets | None; gzip, immutable cache |
| `GET /api/status` | Full device telemetry | None; JSON |
| `GET /api/log` | Bounded application log; optional `after` byte cursor | None; JSON, no-store |
| `GET /api/general` | General sound and inactivity-sleep preferences | None; JSON |
| `POST /api/general` | Partially update General sound/sleep preferences | None; JSON |
| `GET /api/bluetooth` | Bluetooth configuration, pairing/link and remote state | None; JSON |
| `POST /api/bluetooth` | Enable/disable, pair/stop or forget | None; JSON |
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
General, clock, USB and hardware configuration report persistence errors as
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
| `general` | Persistent `scene_change_sound`, `sleep_timeout_minutes` and `stored` |
| `scene` | Active scene, standby/current screen, NVS state, last event/action/result and temporary feedback |
| `usb` | Configured/active mode, stages, link, counters, DHCP and lease state |
| `hardware` | Startup stage, page/tour, awake/paused state, `idle_ms` since the last inactivity reset and peripheral objects |
| `hardware.display` | Framebuffer, frame/FPS/TE counters, selected/effective screen brightness |
| `hardware.encoder` | Raw edges, decoded position, pin levels and `edges_per_detent=2` |
| `hardware.leds` | Ring/key brightness, effective levels, mode/color and SPI transfer errors/counters |
| `hardware.lighting` | NVS `stored` |
| `hardware.sensors` | `vdd_raw`, approximate `vdd_mv_approx` and charge logic |
| `hardware.psram` | Pattern-test result/size and free PSRAM |
| `hardware.audio` | Codec/I2S readiness, volume, microphone levels, frame/error counters and tone state |

Screen fields are `brightness` and `effective_brightness`.
LED fields are `ring_brightness`, `keys_brightness`,
`ring_effective`, `keys_effective`, `mode`, `color`. These brightness values are
percentages except the legacy `hardware.leds.brightness`, which reports the
ring's selected 0–255 byte level.

Ambient-light and automatic-brightness telemetry have been removed. No
`light_*`, `auto_brightness`, `auto_ring`, `auto_keys`, calibration or
`sensor_usable` fields are published. Supply/charge telemetry remains.
`vdd_mv_approx` is the divided supply rail estimate, not battery state of charge.

Screen capture is disabled in 0.4.3: `/api/screenshot.bmp` returns 404 and the
web interface no longer requests display images.

## General preferences

`GET /api/general` and `status.general` return `scene_change_sound` (boolean,
default true), `sound_volume` (integer 0–100, default 70),
`sleep_timeout_minutes` (integer, default 60) and `stored`. POST accepts any of
`scene_change_sound=0|1`, `sound_volume=0..100` and
`sleep_timeout_minutes=0..1440`. Omitted fields retain their saved values;
empty/invalid requests return 400 without changing either setting. Sleep value
0 disables the timer. Save happens before apply. Changing the timeout restarts
its countdown; polling status does not. Physical buttons/knob reset inactivity
and wake the outputs; held buttons prevent inactivity sleep. The sound flag
controls scene transitions made through the web/API. Binding-triggered scene
changes use the action’s own `sound` flag. `hardware.audio.last_tone_hz` and
`last_tone_duration_ms` describe the last successfully written tone; startup and
manual audio tests also update those fields. Next/Previous cues are 880/440 Hz
for 90 ms and keep their direction at wraparound.

```bash
curl http://coloplay.local/api/general
curl -X POST http://coloplay.local/api/general -d 'sleep_timeout_minutes=60'
curl -X POST http://coloplay.local/api/general -d 'scene_change_sound=0'
```

These two POSTs preserve the other preference. `hardware.idle_ms` also resets
on scene/configuration changes, explicit wake and timeout changes. It is runtime
state, not a saved countdown. Sleep leaves HTTP/USB available and reports
`hardware.awake=false`; Blank affects only display visibility while awake.

## Scenes and gesture bindings

`GET /api/scenes` returns `state`, `limits`, recognizer `timing`, the input/
gesture/action/effect `catalog`, and `scenes`. Each scene has a stable numeric
`id`, `name`, `protected`, `standby_screen`, `ring_mode`, `ring_color`,
`keys_mode`, `keys_color`, and sparse `bindings` list. Each
binding contains `input`, `gesture`, `global` and an ordered `actions` array. NVS stores
one version-7, 3304-byte record: 8 scenes, 64 configured gestures and up to 3
actions per gesture. The active scene is persistent.

Ring and key LED modes independently accept `general`, `rainbow` or `static`;
colors use `#RRGGBB`. `catalog.lighting_modes` lists these choices. Omitted
lighting fields retain their saved values, including during rename requests.
`state` and `/api/status.scene` include the active scene's same lighting and icon fields.
`hardware.display.scene_overlay` is true while the switch icon/name is displayed.
`standby_screen` accepts `clock`, `blank`, `bluetooth_volume` or `bluetooth_playback`; omission retains the current choice.
`state.screen` reports the resolved idle/action screen behavior, including temporary
screen effects. Blank switches off only the display and reports effective
screen brightness 0; inactivity sleep sets `hardware.awake=false` and all three
effective brightness channels to 0. Neither changes saved percentages.
Brightness can inherit General or be overridden per scene; action LED
feedback overlays scene lighting, then expires back to it. Legacy v1 scenes migrate with both groups in
`general`, retaining all names, IDs, bindings and the active selection.

`POST /api/scenes` uses these form operations:

| `op` | Fields | Result |
|---|---|---|
| `create` | `name` | Create an empty scene in a free slot |
| `update` | `scene_id`, `name`, optional screen/color, `icon` and brightness fields below | Save scene settings atomically |
| `delete` | `scene_id` | Delete; active/scene-switch targets fall back to Default |
| `reorder` | `scene_id`, `position` (zero-based) | Move an existing scene in the saved display/navigation order |
| `activate` | `scene_id` | Persist active selection and clear pending input/effects |
| `binding` | `scene_id`, `input`, `gesture`, `count`, optional `global=0/1`, action fields below | Atomically replace the complete ordered binding; count 0 clears |
| `run` | `scene_id`, `input`, `gesture` | Execute the saved binding in the active scene; requires hardware ready |

The `scenes` array is returned in saved order. Reorder accepts positions
0 through scene-count minus one, preserves scene IDs/settings/bindings/active
selection and saves before applying. Default can be reordered. New scenes append
regardless of their numeric ID; deletion removes only the deleted scene from the
visible sequence. Next/Previous actions use this sequence with wraparound,
while explicit scene targets remain IDs. Invalid/missing positions return 400.
`state.order_stored` reports whether the separate 12-byte `scenes/order` NVS
record is saved. A missing/invalid record uses ascending scene IDs, preserving
the legacy order without changing the 3304-byte scene settings layout.

Scene 0 is protected: it must exist, remains named **Default**, and cannot be
deleted. Names are distinct, nonempty, at most 24 UTF-8 bytes, with no control
characters. New scenes have no local bindings; marked Global Default bindings
remain available.
Deleting a scene clears its bindings and retargets references to scene 0 before
saving. Invalid requests or NVS write failures do not apply a partial update.

Additional scene fields are `icon` (one of `catalog.icons[].id`) and, for each
`screen`, `ring`, `keys` channel: `<channel>_brightness_mode=general|override`,
`<channel>_brightness` (0–100).
Omitted fields are retained. Icon names are fixed to the 32 locally bundled
Bootstrap choices. Brightness actions update the enabled scene override or the
General setting; override actions debounce storage for 1.5 seconds. `state.revision`
changes for brightness edits without cancelling gestures; `state.epoch` changes
for configuration/scene switches. Polling clients should observe both.

Combination input strings are canonical `key1+key2`, `key1+key3+key4`, etc.,
using 2–4 distinct front buttons. For Any order (`+`), input order is accepted in either direction and responses
use button order. Modifier-first (`>`) inputs retain their distinct ordering. Knob/OPEN/REC combinations and rotation gestures on
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
`a0_screen`, `a0_color`, `a0_feedback_ms`, `a0_sound`, `a0_led_brightness` (then `a1_…`, `a2_…` as needed):

| Field | Values |
|---|---|
| `type` | `none`, `next_scene`, `previous_scene`, `switch_scene`, `screen_change`, `screen_set`, `ring_change`, `ring_set`, `keys_change`, `keys_set`, `volume_change`, `volume_set`, `rainbow`, `static`, `toggle_awake`, `sync_time`, `tone`, `bluetooth_volume`, `bluetooth_media`, `bluetooth_key` |
| `value` | Adjustments -100…100 per step; setters 0…100%; tone 100…4000 Hz |
| `target` | Existing scene ID for `switch_scene` |
| `led` | `inherit`, `on`, `off`, `static`, `rainbow`, `proportional` |
| `led_brightness` | -1 (use resolved scene brightness, default) or 0–100 (absolute override) |
| `sound` | `0` (off, default) or `1` (on); JSON returns a boolean |
| `screen` | `unchanged`, `clock`, `blank`, `bluetooth_volume`, `bluetooth_playback` |
| `color` | `#RRGGBB`; initial effect color `#40ffaa` |
| `feedback_ms` | 250…10000; default 1500 |

Action light feedback targets its binding input: Key 1–4 affect only that key,
combinations affect all participating keys, and all knob gestures affect only
the ring. OPEN/REC have no corresponding LED. Each key and the ring keep their
own overlay/deadline. The configured RGB, brightness and `feedback_ms` apply to
those targets; expiration restores their resolved scene lighting. `on` uses
scene color/animation with the chosen brightness, `off` temporarily darkens the
target, and `inherit` leaves an existing effect/deadline untouched. Brightness
0 is dark; -1 follows scene/General overrides. Sleep still turns outputs off.

Proportional feedback is valid for percentage-producing brightness/speaker
actions and Bluetooth volume. A knob fills its ring; a button scales only its
own LED intensity using the returned percentage. Independent screen overrides
route through the scene service and keep their own deadline. Scene switching
ends the gesture's action list. Explicit device lighting commands still change
the saved group settings; this input targeting applies to the `led` feedback field.

`status.scene.feedback.ring` and `keys[0..3]` expose active mode, RGB,
brightness, initial percentage, remote source and remaining milliseconds.
Legacy scalar feedback fields summarize the ring or first active key.
`hardware.leds.key_colors` and `ring_colors` report the last composed output RGB
in logical key/ring order, after gamma/brightness and sleep. SPI error counters
remain separate. A visual device check confirmed Key 1–4 map to slots 9–12
in that order. A later single-pixel check located ring slot 0 at 12 o’clock;
symmetric fill uses that confirmed top position.

Scene schema 6 migrates to 7 with inherited action brightness, preserving other
saved values and the 3304-byte record. Older versions migrate through the same
brightness defaults. RGB remains `#RRGGBB` on the API; its stored high byte holds
brightness, independently of the existing sound bit. Use a schema-7-capable
recovery image to retain scenes.

Each enabled action produces a nonblocking local cue: ordinary actions use 660 Hz
for 50 ms, scene changes retain the 880/440 Hz next/previous cue for 90 ms, and
`tone` uses its configured frequency for 200 ms. Sound off also silences `tone`
and binding-triggered scene changes. General `sound_volume` applies to all cues;
0% mutes them. A scene action that leaves the scene unchanged is silent.
Scene schema 5 migrates to 6 without growing the record or changing other saved
fields: existing `tone` actions keep sound on, scene actions inherit the current
General scene-sound preference once, and other actions start off. Older v1–v4
records migrate through the same rules. Send `aN_sound` explicitly when saving a
complete binding; omission means off, including for `tone`.
Desktop Serial integration is deferred; Bluetooth Tab5 integration is implemented.

```bash
curl -X POST http://coloplay.local/api/scenes -d 'op=create&name=Work'
# Substitute the returned scene ID:
curl -X POST http://coloplay.local/api/scenes -d 'op=activate&scene_id=1'
curl -X POST http://coloplay.local/api/scenes -d 'op=reorder&scene_id=1&position=0'
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
| `led_mode` | `rainbow` or `static` | Saved; initial `rainbow` |
| `led_color` | `#RRGGBB`, six hexadecimal digits | Saved; initial `#ffffff`; used in static mode |
| `volume` | Integer 0–100 | Runtime speaker amplitude; initial 75; 0 mutes, 100 is 0 dB gain |
| `tone` | Integer 100–4000 Hz | Play a tone; omitted means no tone |
| `duration` | Integer 30–1500 ms | Tone duration; default 250 for this request |
| `awake` | 0 or 1 | Runtime screen/LED sleep; web/OTA remain available |
| `led_test` | 0 stop, 1 white, 2 red, 3 green, 4 blue | Temporary five-second test, then configured mode; brightness selections are unchanged |
| `leds` | Legacy integer 0–64 | Saved compatibility command; sets both ring and keys using the old byte scale |

The retired `auto_screen`, `auto_ring`, `auto_keys`, `auto_dark_raw` and
`auto_bright_raw` parameters return HTTP 400 before any lighting changes.
Scene create/update likewise reject the three retired `auto_<channel>` flags.
Old on-flash fields are reserved/ignored; lighting stays version 1 / 44 bytes,
scene version 7 / 3304 bytes, preserving manual levels and all other settings.

Legacy `leds` values are converted by `(value * 100 + 127) / 255` using integer
arithmetic; `leds=64` is approximately 25%, not 100%. Prefer the independent
percentage fields. If both legacy and new fields are supplied, the new field
for each group takes precedence. `led_test=0` resumes the configured LED mode,
which can be static or rainbow. Frame slot 13 stays off in all current modes.

```bash
curl -X POST http://coloplay.local/api/lighting \
  -d 'brightness=70&ring_brightness=70&keys_brightness=70'
curl -X POST http://coloplay.local/api/lighting \
  -d led_mode=static --data-urlencode 'led_color=#ff8040'
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

## Bluetooth (0.6.0)

`POST /api/bluetooth` accepts form `op=enable|disable|pair|stop_pairing|forget`.
Pair requires enabled Bluetooth and initialized hardware; it opens a 120-second
window. Forget clears the stored peer and local bonds. Enable is saved in its
own NVS namespace; no WiFi/scene/General settings are replaced.

GET and POST return `enabled`, `ready`, `stored`, `connected`, `secured`,
`extension`, `pairing_seconds`, `peer`, `advertising`, `fresh`, `available`,
`volume`, `preview`, `playback`, `seek`, `request`, `result`, `pending`, `title`,
`artist`, `track`, `elapsed_ms`, `duration_ms`, and bounded traffic counters.
Unavailable volume is -1; unknown playback is 255. Preview is a pending grid
target, not reported MPD volume. Freshness expires after five seconds.
Initialization diagnostics include `stage`, `last_boot_stage`, `last_boot_heap`
and `error`; `last_panic` is null or a retained bounded panic record with core,
exception, fault address, reason, `image_elf_sha256` and backtrace addresses.

Bluetooth binding parameters:

| Type | value | target |
|---|---|---|
| `bluetooth_volume` | ±1 host step, ±16 selects 1/16, ±64 selects 1/64 | 0 |
| `bluetooth_media` | Consumer usage: Play 176, Pause 177, Fast Forward 179, Rewind 180, Next 181, Previous 182, Stop 183, Play/Pause 205, Mute 226 | 0 pulse/tap, 1 start seeking, 2 stop seeking; nonzero only for Forward/Rewind |
| `bluetooth_key` | Keyboard usage 1–231 | Modifier mask 0–255 |

The keyboard editor offers a searchable named-key dropdown and a Custom HID
code input for unlisted usages. Saved `value` stays the numeric keyboard usage;
key names, search aliases and the custom-input UI mode add no API/NVS fields.

The web editor presents the modifier mask as a multi-select dropdown with
removable tags. The API still uses the numeric `aN_target` mask: left Control,
Shift, Alt and GUI are 1/2/4/8; right variants are 16/32/64/128. Combine bits
with OR; 0 means no modifiers.

`screen=bluetooth_volume|bluetooth_playback` attaches remote screens;
`led=proportional` is allowed on Bluetooth volume. Both screens can be idle
behaviors. Tab5 accepts only its supported unmodified navigation usages; other
keys/modifiers return Unsupported. Up to three ordered actions remain the
sequence limit. See the shared protocol header for wire fields and result IDs.

Scene records migrate from versions 3/4 to 5 without changing their 3304-byte
layout or existing enum values. Older firmware cannot read new version 5
settings: back up scene JSON before a downgrade and restore compatible settings
explicitly. Disabling Bluetooth on 0.6.1 preserves the migrated scenes.

## Ordered modifiers, cue volume and playback ring (0.6.1)

A binding input can express modifier order with `>`: `key4>key2` requires Button
4 already down before Button 2; `key2>key4` is an independent binding. For three
or four buttons, put the modifier before `>` and remaining members in a `+` list.
Replies canonicalize the remaining members in button order. `key2+key4` retains
Any order behavior. All existing gesture/global/edit/run operations use the full
ordered string as identity. Wrong-order or simultaneous presses cannot trigger
an ordered binding; an exact ordered group takes priority over Any order.

Scene `update` accepts `ring_display=lighting|bluetooth_progress` and includes it
in scene/state JSON. Scene `create`/`update` also accept
`ring_style=clockwise|bottom_to_top` and return it in scene/state JSON. The
default is `clockwise`. `bottom_to_top` fills symmetric LED pairs from bottom
to top and applies to both playback progress and knob proportional effects
(including Bluetooth volume); it leaves button effects unchanged. Style uses
one formerly reserved scene byte, preserving schema 7 and its 3304-byte layout.
Bluetooth progress uses fresh playback elapsed/duration,
with paused/stopped/unavailable handling and action-effect priority. Diagnostic
`hardware.leds.progress_permille` reports 0–1000 or -1 when unavailable.

General `GET/POST /api/general` adds `sound_volume` (integer 0–100, default 70).
It scales local tone amplitude; it does not set MPD volume or change the codec's
speaker-level setting. `hardware.audio.last_tone_volume` reports the last tone's
captured effect gain for verification. Setting 0 is silent.

Scene schema 5 retains the 3304-byte layout: modifier identity uses previously
unused input bits, and ring display uses one reserved scene byte. Strict v3/v4
migrations preserve existing bindings, scene fields and active selection.
General schema 3 adds sound gain to a 16-byte record, preserving v1/v2 scene-cue
and timeout preferences. Older firmware cannot read these newer records.

## Japanese playback typography (0.6.2)

No settings or NVS schema changes. `hardware.display` adds `playback_font`,
`playback_font_glyphs` and `playback_font_bytes`. The embedded Noto Sans JP 16 px
font contains 16,732 glyphs in 2,353,610 flash bytes. Sorted metrics and packed
4-bit coverage remain flash-backed; only visible glyph pixels are read. UTF-8
fitting does not split multibyte characters, and unsupported codepoints use `?`.
Unavailable Bluetooth feedback shows the locally bundled Iconify
`andrsrxn:bluetooth-connecting` icon with no connection text on either remote
screen. A compact playback status row sits above vertically centered metadata,
with a slim LCD progress bar/time below. LED-ring progress behavior is unchanged.

## Shared progress clock (0.6.4)

Bluetooth JSON adds `position_ms` (smoothed display position) and `position_valid`.
`elapsed_ms` remains the raw position received from Tab5; the two fields have
separate meanings. Both LCD progress/time and ring progress consume the same
`position_ms`; neither restarts extrapolation on each heartbeat/ACK.

Duplicate samples refresh link freshness without resetting the display anchor.
New ordinary samples preserve the displayed position and correct timing with
at most ±12.5% speed adjustment. Slightly older cached samples are ignored.
Explicit seek state, pause/resume, new track, reconnect and inferred position
jumps of at least two seconds reset to the reported position. Stalled source
samples cap extrapolation after three seconds; link freshness expires after
five seconds. Raw MPD data, BLE protocol, NVS schemas and Tab5 firmware are unchanged.

## Independent input sampling and trace (0.6.6)

`hardware.inputs` reports the dedicated input worker's `ready`, target
`sample_period_ms` (1), `samples`, `max_sample_gap_us`, and `queue_drops`.
Sampling/gesture recognition run in a priority-3 task; a bounded 64-event queue
hands actions to the main loop. Scene/binding generations discard stale events.
Overflow cancels pending input and remote seeking; OTA pause disables bindings
and clears queued actions. All settings/schema and Tab5 firmware are unchanged.

Debounce is still 25 ms and click separation 300 ms. A second raw press that
begins within the click window keeps the first click pending while debounce
completes; short rejected glitches cannot postpone the timeout indefinitely.

`GET /api/input-trace` reads a bounded 256-record RAM trace for Button 2.
Records include microsecond timestamps, raw GPIO IRQ edges, sampled raw state,
debounced state, recognized gestures and long sampling gaps near input activity.
`POST /api/input-trace` with `op=clear` resets the trace and sampling timing
statistics. Capture is memory-only, without serial or NVS writes; data is lost
on reboot and older records are overwritten. This is diagnostic telemetry, not
an input-injection endpoint.

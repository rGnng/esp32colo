# Tab5 controls, action categories and feedback

Research and implementation plan, 2026-11-13. This describes proposed work;
the running ColoPlay 0.5.4 has device actions, Clock/Blank and inactivity sleep.
Serial companion actions, BLE HID, pairing and remote media screens are not
implemented by this document.

The target reviewed is [rGnng/mpdtab5](https://github.com/rGnng/mpdtab5), pinned
to commit [`8bb8fa119d1bf7d2062734f8705bf968c5864c35`](https://github.com/rGnng/mpdtab5/tree/8bb8fa119d1bf7d2062734f8705bf968c5864c35).
The clean local checkout matched GitHub HEAD when inspected. Code findings
below refer to that commit, rather than the earlier iPod bring-up history.

## Recommendation

For the finished wireless Tab5 controller, use **BLE HID plus a small custom
control/state service on the same bonded connection**. HID supplies normal
keyboard and media controls; the custom service supplies fractional volume
requests, command results and authoritative playback/volume feedback. Both
firmwares need changes. Pairing alone will not make the current Tab5 driver
understand a ColoPlay.

For a quick prototype, **WiFi/IP is already useful**: Tab5 exposes controls and
MPD state over HTTP. This proves mappings and screen feedback before a new BLE
driver is ready. BLE removes the controller's dependence on the access point;
Tab5 still needs its network connection to the MPD server. Serial remains a
separate desktop-companion option, with the same logical command/state model.

The response requirement is a **fluid local UI**, not removal of Tab5's roughly
100 ms command-check delay. Show an overlay and ring response immediately after
local gesture recognition, then reconcile with confirmed remote state. Keep
the existing MPD worker timing for the first version.

## Action categories

Use **Device**, **Serial** and **Bluetooth** as the top-level categories in the
binding editor. Within a category, group related operations. Select Category,
then Action and its parameters. Binding summaries must include the category:
`Serial · Volume Up · 1/64` differs from `Device · Speaker volume +2%`.

| Category / group | Actions | Current status |
|---|---|---|
| Device / No command | No device action; apply only attached effects | Implemented |
| Device / Scenes | Next Scene, Previous Scene, Switch to Scene | Implemented |
| Device / Display | Adjust Screen Brightness, Set Screen Brightness | Implemented |
| Device / Lighting | Adjust/Set Ring Brightness, Adjust/Set Button Brightness, Rainbow, Static Color | Implemented |
| Device / Speaker | Adjust Device Speaker Volume, Set Device Speaker Volume, Play Tone | Implemented; controls the ColoPlay codec |
| Device / System | Toggle Screen and LEDs, Synchronize Time | Implemented |
| Serial / Volume | Volume Up/Down — 1/64; Volume Up/Down — 1/16; Mute | Proposed; requires companion |
| Serial / Playback | Play/Pause, Play, Pause, Stop, Previous Track, Next Track, Fast Forward, Rewind | Proposed; requires companion |
| Serial / Keyboard | HID Key, HID Key Sequence | Proposed; requires companion |
| Bluetooth / Volume | Volume Up/Down (host step), Mute; optional 1/64 and 1/16 when the peer advertises custom control support | Proposed |
| Bluetooth / Playback | Play/Pause, Play, Pause, Stop, Previous Track, Next Track, Fast Forward, Rewind | Proposed BLE HID |
| Bluetooth / Keyboard | HID Key, HID Key Sequence | Proposed BLE HID |

Screen and light **effects remain separate fields** attached to an action.
Brightness percentages configure this gadget; remote volume fractions configure
the selected host. Do not put all of these into a single unqualified Volume list.
Do not expose an unimplemented action as a working selection. The temporary
WiFi Tab5 adapter should have an explicit Tab5/IP target, rather than silently
changing what Serial or Bluetooth means.

Use **Previous Track**, **Next Track** and **Play/Pause** in the UI, matching
Apple's `previousTrackCommand`, `nextTrackCommand` and
`togglePlayPauseCommand` naming. Fast Forward/Rewind operate within a track;
they are distinct from track changes. Separate Play and Pause are idempotent;
Play/Pause is a toggle. [Apple remote-command documentation](https://developer.apple.com/documentation/mediaplayer/mpremotecommandcenter)

Use HID usage identities, not Apple-private key numbers or platform virtual-key
codes, in configurable key actions:

| Operation | HID usage page | Usage |
|---|---|---|
| Volume Up / Down | Consumer `0x0C` | `0xE9` / `0xEA` |
| Mute | Consumer `0x0C` | `0xE2` |
| Play/Pause | Consumer `0x0C` | `0xCD` |
| Play / Pause / Stop | Consumer `0x0C` | `0xB0` / `0xB1` / `0xB7` |
| Next Track / Previous Track | Consumer `0x0C` | `0xB5` / `0xB6` |
| Fast Forward / Rewind | Consumer `0x0C` | `0xB3` / `0xB4` |
| Keys and modifier combinations | Keyboard `0x07` | Usage(s) plus modifier mask |

These are usage values, not complete serialized reports. A BLE report map
defines the actual report IDs, widths and layout. Every key/media press needs
its release. [USB HID Usage Tables, Consumer page](https://usb.org/sites/default/files/hut1_6.pdf)

### Fractional volume is application policy

1/64 means **1.5625 percentage points** of the normalized range; 1/16 means
**6.25 percentage points**. Preserve the requested fraction and detent count
in the command. A plain HID volume increment/decrement does not specify either
step size; a generic host chooses its own behavior. Do not promise a portable
1/64 action by sending the ordinary volume usage, or by repeating it.

Serial requests carry the usage and step denominator for the companion to
interpret. Bluetooth can do the same through the custom control service when
connected to the modified Tab5. With an ordinary HID-only host, expose the
host-step action and show that exact fractional steps are unavailable.

Tab5 currently sends MPD `volume 5` / `volume -5`: **five percentage points**,
not 1/16 or 1/64. MPD state in this application is integer 0–100. Add a fractional
accumulator in the Tab5 adapter and quantize the resulting target to MPD's native
resolution. Preserve the fractional remainder across the adapter's own updates;
rebase when an external volume change is observed. Report the actual resulting
integer percentage. Do not claim MPD can represent 1.5625% exactly.

## What the Tab5 source already provides

| Finding | Evidence at the reviewed commit | Consequence |
|---|---|---|
| BLE is already running through ESP-Hosted/NimBLE on P4/C6 | [`firmware/sdkconfig.defaults`, Bluetooth block](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/sdkconfig.defaults#L119) | Reuse its stack and supervisor; no speculative radio bring-up is required |
| Current BLE discovery accepts D100H/Ulanzi names or service `0x1971`; GATT setup requires vendor `0xFD00`/`0xFD01` | [`app/backend/ble_remote.c`](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/app/backend/ble_remote.c#L353) | A ColoPlay service needs its own discovery and decoding path; a generic HOGP device is not supported today |
| Bonding, reconnect, encryption, scan/forget UI and persistent peer records exist | Same driver and [`ble_remote.h`](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/app/backend/ble_remote.h) | Extend peer handling by device type; preserve the D100H implementation |
| Only one concurrent BLE connection is configured | `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` | First version can select D100H or ColoPlay; simultaneous use needs a larger limit and per-connection driver state |
| Standard USB input handles boot keyboards; keyboard-page volume keys currently map to navigation Up/Down | [`firmware/main/usb_hid_input.c`](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/main/usb_hid_input.c#L169) | Do not assume USB consumer media reports already control MPD volume |
| Serial input is a one-character debug/UI console | [`firmware/main/serial_input.c`](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/main/serial_input.c) | It is not a structured companion command/state protocol |
| HTTP controls already route into common UI actions | [`firmware/main/web_server.c`, key handlers](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/main/web_server.c#L240) | Existing IP path can prove navigation, playback and native volume steps |
| Authoritative MPD snapshot includes volume, play status, track metadata, elapsed/duration and revision | [`mpd_client.h`](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/main/mpd_client.h), [`web_server.c`, status](https://github.com/rGnng/mpdtab5/blob/8bb8fa119d1bf7d2062734f8705bf968c5864c35/firmware/main/web_server.c#L1440) | Return actual state without estimating it from sent keys |

The Tab5 uses ESP32-P4 with ESP32-C6 wireless support; ColoPlay uses ESP32-S3.
Use Bluetooth LE, not Bluetooth Classic serial/SPP or A2DP. This link carries
controls and state, not audio. [Tab5 hardware](https://docs.m5stack.com/en/core/Tab5),
[Espressif ESP-Hosted](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/solution-introduction/multimedia/application-solution/esp-hosted-mcu.html)

### Existing IP interface

These are existing Tab5 routes, not proposed ColoPlay endpoints:

| Request | Current meaning |
|---|---|
| `POST /api/key?k=prev` / `next` | Previous/Next Track |
| `POST /api/key?k=play` | Play/Pause toggle, despite the shorter parameter name |
| `POST /api/key?k=volup` / `voldown` / `mute` | Native ±5-point MPD step / mute |
| `POST /api/key?k=up` / `down` / `enter` / `back` | UI navigation |
| `POST /api/key?k=pageup` / `pagedown` | Page navigation |
| `POST /api/hidbit?bit=10&mode=press` | Begin the existing forward-seek behavior |
| `POST /api/hidbit?bit=11&mode=press` | Begin the existing reverse-seek behavior |
| `POST /api/hidbit?bit=10&mode=release` | Clear the held seek operation; release currently ignores the selected bit |
| `GET /api/status` | Read `mpd_volume`, `mpd_connected`, `play_status`, `title`, `artist`, `album`, `elapsed_ms`, `duration_ms`, etc. |

Current seeking repeats relative `seekcur +5` / `seekcur -5` in the MPD worker.
It is stepped seeking, not a promised change in playback speed. The UI's
press/hold interpretation and backend seek operation are separate layers.
Already-recognized ColoPlay Fast Forward/Rewind actions should enter the
backend directly, without another D100H gesture-recognition delay.

The root status `volume` configures Tab5 feedback sound; **`mpd_volume` is the
playback volume**. Its value `-1` means no usable mixer, not zero percent. The
legacy iAP2 symbols and comments do not establish current MPD limitations.
In particular, MPD volume is available even though older iPod absolute-volume
feedback was unavailable.

Existing text replies confirm handler/queue processing, not application by MPD.
Exact fractional steps, correlated completion, seek-state return and push
updates need an adapter in Tab5. Reuse its existing MPD state; do not invent a
second unsynchronized player state.

## Serial and Bluetooth command/state contract

Keep the logical messages transport-independent. Serial carries bounded framed
records; BLE uses bounded characteristic payloads with the same semantics.
This is a proposed protocol, not an existing `/api/` contract.

| Message | Required information |
|---|---|
| Capabilities | Protocol version, peer identity, supported controls, keyboard/sequence support, volume source/range/resolution, fractional-step support |
| Action | Session and request ID, usage page/usage or semantic operation, parameters, count; keyboard modifiers/sequence as applicable |
| Accepted | Request ID; accepted/queued, unsupported, busy or rejected |
| Result | Request ID, operation, applied/no-change/failed, error if any, resulting state revision |
| Volume state | Source, valid/available, confirmed level/range, mute, revision and freshness |
| Playback state | Playing/Paused/Stopped/Unknown, track identity/title/artist/album, elapsed/duration, availability and revision |
| Transport state | Last control and request ID, direction/phase of any active seek, command outcome and current playback-state revision |

For Serial, send HID usages plus extra parameters when the usage alone is
insufficient. For example, this illustrative record requests one fine increment:

```json
{"v":1,"session":7,"id":42,"type":"action","page":12,"usage":233,"step_denominator":64,"count":1}
```

The companion maps it to the selected system audio output and returns the
actual system level and playback state. Tab5 maps the same meaning to MPD.
HID Key Sequence is a bounded list of key chords/presses, releases and delays,
not executable script text; a first limit of 16 steps is reasonable. Execute
without blocking input/rendering. Release held keys on cancellation/disconnect.

The current Serial output also carries logs and the recovery console. A future
parser needs explicit framing, for example a reserved `@CP1 ` prefix, bounded
records and replies; a desktop receiver must not interpret log lines as keys.
CDC-ECM is a USB network interface, not an ACM serial port. Two USB-C device
ports cannot communicate by simply connecting them together. Wired Tab5 Serial
requires an appropriate USB-A host driver/bridge, which is not present today.
Preserve the existing USB recovery modes while adding any serial transport.

### Return playback state and control outcomes

Feedback includes **Play/Pause status and transport operations**, not just volume:

| Control | Immediate local feedback | Required authoritative return |
|---|---|---|
| Play/Pause, Play, Pause, Stop | Operation icon / pending indicator | Playing, Paused or Stopped, resulting position, success/failure |
| Previous Track / Next Track | Direction icon / short transition | Result plus current track identity/metadata and playback/position state |
| Fast Forward / Rewind start | Seek direction animation | Accepted/applied seeking state, direction and refreshed position |
| Seek release/cancel | Stop local seek animation | Seek inactive and final confirmed position/playback state |
| Volume / mute | Responsive target preview when a valid base exists | Actual volume/mute, or unavailable/error |

An accepted command is not yet applied. A successful GATT write is not an MPD
acknowledgment. Emit Result after backend processing, then a confirmed snapshot.
Track IDs and revisions distinguish a real transition from a repeated title.
A boundary or unsupported operation may leave state unchanged; report that
honestly instead of inventing a next track or assuming playback started.

Publish updates for **external changes too**: Tab5 touch, other controllers,
MPD clients, natural track transitions and pause/resume. Observed state updates
must never be fed back as new commands. Use request/session IDs to reject stale
responses and deduplicate retries of non-idempotent controls such as Next Track
and Play/Pause. An unacknowledged old command must not replay after reconnect.

Seeking needs explicit start/end semantics and a disconnect/watchdog release.
Map configurable Hold and Hold then release to start and end, or use a bounded
seek pulse for a click/knob event that has no release. Do not leave seeking active
when a scene changes, a macro is cancelled or a peer disappears.

## BLE connection and pairing design

ColoPlay is the **peripheral/GATT server/HID device**. Tab5 is the
**central/GATT client/HID host**. Add a peer driver selected by service identity;
reuse the current security/reconnect machinery without impersonating a D100H.
HOGP defines HID input/output reports but does not automatically publish a
computer's master volume or media-player state. [HID over GATT specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/hogp_v1-2_1777405843/HOGP_v1.2/out/en/index-en.html)

One connection can carry both HID and a custom service:

| ColoPlay characteristic | Tab5 client operation | Purpose |
|---|---|---|
| HID input reports | Subscribe/read report metadata | Standard keyboard and consumer controls |
| Custom capabilities | Read | Negotiate extension support and volume resolution |
| Custom command | Subscribe to notifications | Fractional volume and other semantic requests requiring the extension |
| Custom state/result | Write with response | Tab5 sends authoritative volume, playback, transport state and results back |

Use the HID path or extension path for a given operation, never both. Explicit
request IDs are straightforward on the extension; raw HID itself has no such
application acknowledgment. A Tab5 adapter can return the observed HID control
and updated state, but exact request correlation requires the extension.
The extension does not need a second BLE pairing or a GATT client on ColoPlay.
Keep basic control/state payloads small enough for the default ATT MTU, or
negotiate/chunk explicitly; do not assume an arbitrarily long JSON record fits
one GATT notification. Track strings can be separate bounded metadata updates.

Proposed **General → Bluetooth** panel:

- Enable Bluetooth; connection state and bonded peer.
- Start pairing / Stop pairing, with a visible bounded pairing window.
- Forget peer; clear the local bond and explain when Tab5 must forget it too.
- Extension support and feedback availability, distinct from Connected.

Start pairing advertises the ColoPlay; select it from Tab5's scan/connect UI.
The current Tab5 uses encrypted Just Works bonding with no MITM requirement.
First-version compatibility can retain that policy inside an explicit pairing
window; a displayed numeric-comparison/passkey flow would require changes on
both sides. Persist settings and bonds, reconnect known peers, and fetch a fresh
snapshot on reconnect. The one-hour screen/LED sleep must leave the link alive.

## Screens, ring feedback and responsiveness

Add **Serial volume** and **Bluetooth volume** as both saved idle screens and
temporary action screen effects. Each selects a distinct live source; neither
reads the ColoPlay speaker percentage. A serial desktop companion reports its
selected system output. A Bluetooth Tab5 peer reports the MPD mixer. Labels
must retain this distinction even if both currently control the same player.

Show a percentage/gauge, mute state and connection/freshness. A volume action
can illuminate the ring proportionally to the selected remote source. An
unknown/missing mixer displays `--`/Unavailable, never a fabricated 0%. Preserve
the existing Clock/Blank choices, scene brightness overrides and sleep behavior.
A playback/transport overlay can show the operation icon, confirmed play/pause
status, track text and seek position, then return to the chosen idle behavior.

On recognized input, immediately show local feedback. Where a confirmed base
volume exists, animate an estimated target while visibly pending; authoritative
feedback replaces it. Never relabel an estimate as the reported system volume.
Do not optimistically invent track metadata or a confirmed play/pause state.
Seek animation may extrapolate between position samples, anchored to confirmed
time and stopped/rebased on pause, seek end or loss of freshness.

The current Clock refreshes at 5 fps and LEDs at about 30 Hz. Force a first frame
on action and use a higher, bounded refresh rate while a media overlay animates
(initial target 15–20 fps, to be measured); keep Clock at 5 fps and Blank/sleep
without frame refresh. Coalesce state updates, not lost knob detents. Network
and BLE operations run outside the render/input path. Current synchronous
`int percent` action results need an asynchronous pending/completed path for
remote feedback and source-aware ring updates.

BLE is a reasonable choice for predictable small control messages. Target a
negotiated 7.5–15 ms connection interval with no skipped peripheral events for
this powered controller, and record the actual negotiated values. This is a
radio scheduling target, not a measured end-to-end latency. Healthy WiFi/IP can
match or beat BLE; its delay depends on contention, connection handling and
power saving. ColoPlay already disables WiFi sleep. BLE/WiFi coexistence and
the Tab5 hosted radio still need measurement. [BLE timing](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/bluetooth/esp_bt_defs.html),
[WiFi latency and coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/api-guides/wifi.html)

The MPD worker checks its command queue between 100 ms wait chunks and polls
state after each processed action. Additional network/artwork work may add
delay. That backend cadence is acceptable for version one: local animation
must not wait for the MPD acknowledgment. Likewise, the web configuration's
two-second status poll must not drive interactive media feedback. Use pushed
BLE state, and a dedicated IP feedback cadence/response if prototyping on WiFi.

## Implementation order and acceptance

1. Categorize existing Device actions without changing IDs or saved behavior.
   Add the typed remote-action/source model, pending results and bounded macro
   records. Preserve all existing NVS settings through an explicit migration;
   current 12-byte actions cannot directly store arbitrary key sequences.
2. Implement the shared Tab5 adapter using existing playback functions/state.
   Add fractional volume, results and subscriptions; validate basic mappings
   with the existing IP path. Leave the MPD worker timing unchanged initially.
3. Add BLE HID plus the extension on ColoPlay, a separate ColoPlay peer driver
   on Tab5, and the General pairing panel. Keep D100H support and the one-peer
   connection policy explicit.
4. Add the two remote-volume screen sources, transport/playback overlays and
   asynchronous proportional ring feedback. Add the desktop Serial companion
   later using the same logical messages and capability negotiation.

Measure first local feedback separately from remote command completion and
confirmed state return. Test rapid knob runs and held-key combinations; no lost
detents, stuck keys or seeking after release/disconnect. Verify 1/64 and 1/16
requests at both ends of the volume range, MPD quantization, absent mixer,
mute and volume changes from another MPD client. Verify Play/Pause/Stopped,
Previous/Next Track, both seek directions, errors, duplicate requests and
reconnect snapshots. Confirm smooth overlays during a 100 ms or longer backend
response, correct reversion to idle, persisted configuration and continued
pairing/feedback availability through screen/LED sleep.

This review did not change or flash either firmware and did not perform an
over-the-air latency measurement. The recommendation is based on source and
protocol capabilities; the timing comparison remains a hardware test item.

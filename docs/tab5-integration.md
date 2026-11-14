# Bluetooth controls and Tab5 feedback

Implemented in ColoPlay **0.6.6-input-sampling**, with a separate ColoPlay peer driver
in `rGnng/mpdtab5`, based on
[`a34fa8a`](https://github.com/rGnng/mpdtab5/commit/a34fa8a2c7547573f128b47061d7359f3f504a81).
The existing MPD volume-grid helper and worker remain authoritative. Tab5's
D100H support, USB, iAP2, storage, player UI and configuration remain in place.

## Pairing

1. In ColoPlay **General → Bluetooth**, enable Bluetooth and choose **Start pairing**.
2. In Tab5's existing Bluetooth page, scan and select **ColoPlay**.
3. Check Connected, extension support and fresh feedback in ColoPlay's status.

Pairing is open for 120 seconds. Devices use encrypted Secure Connections
Just Works bonding. Settings and bonds survive restart; known peers reconnect
without opening pairing again. Forget clears ColoPlay's peer and bonds; also
forget it on Tab5 before pairing again. Tab5 supports one connected controller:
selecting ColoPlay replaces the active connection, retaining other saved peers.
Screen/LED inactivity sleep leaves Bluetooth connected and receiving state.
Bluetooth is initially disabled on a new installation.

ColoPlay is the peripheral/GATT server; Tab5 is the central/GATT client.
Standard HID and a custom command/state service share the connection. Tab5
uses the extension for all ColoPlay commands, preventing duplicate HID delivery.
Without the extension, ColoPlay can emit keyboard and consumer HID reports;
generic computer/phone HID interoperability still needs physical host testing.
Standard HID alone supplies neither exact fractional steps nor volume feedback.
[Bluetooth HID over GATT](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/hogp_v1-2_1777405843/HOGP_v1.2/out/en/index-en.html)

## Actions and screens

The offline command selector groups existing Device actions by purpose and adds
Bluetooth actions. Serial is deferred; unavailable actions are not selectable.

| Group | Controls |
|---|---|
| Device Scenes | Next Scene, Previous Scene, Switch to Scene |
| Device Display / Lighting | Screen/ring/button brightness, rainbow, static color |
| Device Speaker / System | Local speaker volume, tone, sleep/wake, time sync |
| Bluetooth Volume | Volume Up/Down, using host step, 1/64 or 1/16 |
| Bluetooth Playback | Play/Pause, Play, Pause, Stop, Previous Track, Next Track, Fast Forward, Rewind, Mute |
| Bluetooth Keyboard | Keyboard HID usage and modifier mask |

Play/Pause, Previous Track and Next Track follow
[Apple remote-command naming](https://developer.apple.com/documentation/mediaplayer/mpremotecommandcenter).
HID identifiers use Consumer page `0x0C` and Keyboard page `0x07`, rather than
platform virtual-key codes. See the shared protocol header for exact values.

**Bluetooth volume** and **Bluetooth playback** are available as both idle
screens and action screen effects; Clock and Blank remain available. Volume
shows a pending target followed by confirmed MPD volume, independently of the
ColoPlay speaker. Proportional LED feedback fills the ring using that same
remote source. A disconnected/stale source or unavailable mixer shows `--`;
it never fabricates zero. Playback shows state, track text and seek direction, with position on the
configurable LED ring. Metadata uses an embedded 16 px Noto Sans JP bitmap font covering 16,732
characters, including Japanese kana/kanji, Latin and supported CJK extensions.
Unsupported characters use `?`; emoji and Korean coverage are not promised.

A binding holds up to three ordered actions, including keyboard taps. This
supports short sequences; a dedicated 16-step macro editor is not implemented.
Generic HID supports modifier masks. The Tab5 adapter currently accepts only
unmodified navigation usages: arrows, Return, Escape, Page Up/Down, brackets
(previous/next screen), C (context), and digits 1–7. It returns Unsupported for
other keys/modifiers. These commands enter Tab5's existing input queue.

### Volume behavior

An explicit 1/64 or 1/16 action selects that grid for the command, without
changing Tab5's saved step setting. **Host step** follows Tab5's current 16/64
setting. Each command starts from the raw MPD server percentage, finds the
nearest grid index, applies whole detents, then rounds the target to MPD's
integer percentage. This reuses the exact `volume_grid_step` helper introduced
in `a34fa8a`; it does not maintain a second fractional accumulator. For example,
100 → down one 1/64 step → 98 → up one step → 100; a 1/16 step gives 94.
The local preview uses the same grid, then yields to returned server state.
MPD `-1` means no mixer; mute uses Tab5's existing MPD mute behavior.

Fast Forward and Rewind are stepped `seekcur +5` / `seekcur -5` operations,
not changes in playback speed. Configure **Hold → Start** and **Hold then
release → Stop**. Click/turn bindings can use a bounded pulse. Scene/config
changes send Release; disconnect and a 10-second Tab5 lease also stop seeking.
The custom seek state is separate from existing USB/D100H hold state.

### Installed test profile

The **Tab5 BLE** scene uses navigation bindings matching Tab5's existing UI:

| Input | Binding |
|---|---|
| Knob counterclockwise / clockwise | Up / Down item |
| Knob press | Return / Enter: activate focused item or confirm an edit |
| Button 1 | Escape / Back |
| Button 4 first, then Button 2 | Page Up |
| Button 4 first, then Button 3 | Page Down |
| Button 2 first, then Button 1 | `[` previous screen/menu |
| Button 2 first, then Button 4 | `]` next screen/menu |

The same Button 2/4 pair therefore has two actions depending on which is held
first. Keep the modifier down and tap targets repeatedly. Simultaneous presses
have no modifier order; they only match an explicit Any order binding, if one
exists. Ordered bindings take priority over unordered bindings for matching
orders. Existing Global Default scene-switch bindings remain available.

Tab5 handles Enter through the current page/menu handler, falling back to
`focus_activate(true)`; edit mode confirms/exits instead. Back uses the existing
page/back behavior. `[` on Now Playing opens its queue; elsewhere it selects
the previous enabled screen. `]` selects the next enabled screen. Bluetooth
settings are skipped by screen navigation, as in the existing Tab5 UI.

Idle **Bluetooth playback** uses a small drawn playback-state icon at the
top, without Playing/Paused/Stopped labels, with 16 px title/artist lines surrounding the vertical center. Long text is
measured and ellipsized at UTF-8 boundaries. A slim LCD progress bar/time sits
below; the configured LED-ring progress remains independent. There are no emoji
or decorative boxes. When feedback is unavailable, both Bluetooth screens show
only the locally bundled `andrsrxn:bluetooth-connecting` icon, without connection
text. Glyph data stays in flash; no whole font is loaded into RAM.

Scene **Ring display → Bluetooth playback progress (clockwise)** fills the
physical LED ring from elapsed/duration using scene colors and brightness.
Playing uses a shared smoothed local clock between position packets; repeated
heartbeats/ACKs do not reset it. Pause freezes, Stop clears,
and unknown/stale state clears the progress fill. Partial pixels smooth the nine
LED segments. The progress indicator is independent of the LCD idle screen.
Explicit action light effects temporarily take priority, then return to progress;
button LEDs keep their scene lighting. Sleep turns both groups off.

General **Sound effects volume** is saved independently of brightness and remote
MPD volume. It scales local generated tones, including scene-change cues; 0% is
silent. Existing scene-cue enable and high/low direction tones are retained.
The default effect gain is 70%; the existing codec speaker level remains separate.

## Wire protocol and execution

The identical `coloplay_ble_protocol.h` headers in both projects define version
1. Every frame is 20 bytes, fitting the default ATT payload without requiring
an MTU upgrade. All multibyte integers are little endian.

| Characteristic | UUID suffix | Direction |
|---|---|---|
| Service | `0001` | Base `a91c0001-c169-4acb-9f5d-121d621d17b5` |
| Capabilities | `0002` | Tab5 reads version, capabilities and session nonce |
| Command | `0003` | ColoPlay notifies commands; encrypted CCCD |
| State/result | `0004` | Tab5 writes with response over encryption |

Common header: version byte 0, kind byte 1, request ID bytes 2–3, session
nonce bytes 4–7. Command bytes 8–15 contain usage, count, parameter and pulse
duration as four uint16 values. Reserved bytes are zero. Release cancels custom
seeking. Session checks and a bounded recent-request cache prevent duplicate
controls and stale replies after reconnect. Queues are bounded; no old command
is replayed after disconnect. Neither firmware performs MPD socket I/O in the
render/input or BLE callback path.

HELLO reports the native volume grid. STATE carries signed volume, playback
(0 stopped / 1 playing / 2 paused / 255 unknown), backend availability, seek
direction, request outcome, elapsed and duration in milliseconds. Outcomes are
Pending, Accepted, Applied, Failed, No change, Unsupported and Busy. Applied
is sent after the existing MPD worker processes the request and refreshes its
state; Accepted is only a queue acknowledgment. Track identity/revision and
bounded title/artist text are separate frames, up to 63 bytes per string.

Periodic state is scheduled at 400 ms on Tab5's existing 250 ms supervisor,
usually yielding about 500 ms updates; command results also schedule an event
immediately. Feedback older than five seconds becomes unavailable. External
MPD changes and natural track transitions publish state without creating new
commands. Live volume and playback frames refresh at up to 20 fps on ColoPlay;
Clock remains 5 fps. Incoming feedback does not wake a sleeping gadget.

The existing MPD worker checks queued commands between roughly 100 ms wait
chunks. Immediate volume preview and screen/ring feedback keep local response
independent of that backend cadence. The requested BLE connection interval is
7.5–15 ms; this is not an end-to-end latency measurement. Live API tests include
HTTP-relay overhead and cannot establish physical button-to-screen latency.
ColoPlay enables Wi-Fi modem sleep before BLE starts to permit radio coexistence
on the pinned Arduino/IDF SDK. Tab5 continues using its existing hosted BLE stack.

## Validation and limits

Both application images are OTA'd through the HTTP relays, preserving
bootloaders, partition tables and Tab5 C6 firmware. Live checks verified encrypted
pairing, bond reconnect, raw MPD volume return, native/coarse volume round trips,
Play/Pause state, both seek directions and release, Page Up/Down acknowledgment,
track metadata and continued MPD connectivity. Original settings were compared
against saved snapshots. Physical key recognition and generic PC HID require
hands-on testing; API run tests dispatch saved bindings directly.

Host tests cover packet validation, NVS v3→v4 migration, grid arithmetic,
unsupported/stale controls, seek release, duplicate requests, reordered GATT
characteristics, disconnect generations and the existing D100H lifecycle.
Tab5's full baseline suite has existing credential/legacy-fixture failures;
comparison against untouched `a34fa8a` found no new failing cases. Its focused
MPD/volume/BLE tests and native UI CTests pass. See the release record for exact
image fingerprints and remaining live qualification limits.

## Deferred Serial companion

Serial actions and a desktop companion remain future work. Preserve the same
logical command/state model, with distinct **Serial volume** and Bluetooth
sources, exact grid/count requests, command results and returned playback state.
A future sequence format must be bounded and release held keys on cancellation.
The current console carries logs, so a companion needs explicit framing and
must never interpret arbitrary log text as keys. CDC-ECM is USB networking;
a Tab5 USB-A host driver/bridge would be required for a direct wired link.

Tab5's existing HTTP controls remain available for IP integrations:
`POST /api/mpd/volume`, `/api/mpd/control` and `GET /api/status`. Root `volume`
is Tab5's feedback speaker setting; `mpd_volume` is MPD playback volume.
No BLE-versus-Wi-Fi latency comparison has been measured.

# Bluetooth controls and Tab5 feedback

Current ColoPlay firmware: **0.6.26-volume-bar**; its source/build identity
is in the [release record](development.md#current-verified-release).
Volume-scale support follows Tab5 commit
[`80acc5d`](https://github.com/rGnng/mpdtab5/commit/80acc5de9439fda8e1da57e541adb66d2f1cc20a), using matching
shared protocol and volume-grid headers. The original recorded Tab5 peer driver is
[`79c5dac`](https://github.com/rGnng/mpdtab5/commit/79c5dacd1f38a955f3ea2a8abff5b719d5d616d3),
built on [`a34fa8a`](https://github.com/rGnng/mpdtab5/commit/a34fa8a2c7547573f128b47061d7359f3f504a81).
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
| Bluetooth Volume | Volume Up/Down; Tab5 selects its configured grid |
| Bluetooth Playback | Play/Pause, Play, Pause, Stop, Previous Track, Next Track, Fast Forward, Rewind, Mute |
| Bluetooth Keyboard | Keyboard HID usage and modifier mask |

Play/Pause, Previous Track and Next Track follow
[Apple remote-command naming](https://developer.apple.com/documentation/mediaplayer/mpremotecommandcenter).
HID identifiers use Consumer page `0x0C` and Keyboard page `0x07`, rather than
platform virtual-key codes. See the shared protocol header for exact values.

**Bluetooth volume** and **Bluetooth playback** are available as both idle
screens and action screen effects; Clock and Blank remain available. Volume
shows a speaker icon and horizontal bar, independently of the
ColoPlay speaker. The screen and LEDs predict immediately using Tab5's
synchronized grid. Actual mixer feedback remains separately available in
diagnostics. Pending turns are reconciled by completed request ID.
Proportional LED feedback fills the ring using that same remote source.
A disconnected/stale source shows the connecting icon; an unavailable mixer
shows an empty bar, never a fabricated zero. Request IDs, pending state
and outcomes remain in Device diagnostics; volume command transitions also
appear in the web log. Playback shows state, track text and seek direction, with position on the
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

ColoPlay sends only Volume Up/Down and detent count. Tab5's **Volume Scale**
selects the control coordinate and absolute MPD target:

| Choice | Control positions | Raw MPD target for position `x` |
|---|---|---|
| 1/16 Squared Curve | `i / 16` | `x²` |
| 1/64 Squared Curve (default) | `i / 64` | `x²` |
| 2% Integer | 0%, 2%, 4%, …, 100% | `x` |
| 5% Integer | 0%, 5%, 10%, …, 100% | `x` |

The integer profiles provide straight integer-percent targets for Linux/Windows
and integer-only MPD servers. Switching profiles preserves the playing volume;
it changes the readout mapping and subsequent commands. The existing saved
`volume_steps` IDs 16/64 remain selected as the corresponding squared profile;
new/missing configurations default to 64. IDs 2 and 5 select the integer profiles.

Both devices share 154 compile-time float targets (616 bytes). Squared readback
is mapped to the nearest lookup position before stepping; its displayed value
is that position's control fraction. Integer profiles use directional snapping
for external off-grid values: from 61%, 2% Integer goes up to 62% or down to 60%.
Commands always use absolute `setvol`; every batched detent is counted.

For example, displayed 25% in a squared profile sends raw `setvol 6.25`.
A hardware return of 6.3488% maps back to the same displayed 25%. Actual raw
feedback stays in diagnostics. The confirmed raw target is kept separately
from its readback; external changes, missing mixer and MPD reconnect invalidate
that reference. This avoids accumulating quantization offsets.

[Tracks `628ee5b`](https://github.com/rGnng/tracks/commit/628ee5b) accepts decimal
percentages. Older integer-only servers retain an explicit invalid-argument
fallback, but the Integer profiles are the appropriate choice for their targets.
MPD `-1` means no mixer. Mute retains the selected scale's saved restore behavior.

### Responsive prediction

Tab5 announces its four-profile scale setting during connection setup and refreshes it
when settings change. With negotiated prediction support, volume feedback adds
its confirmed logical grid reference and the last completed volume-command ID.
ColoPlay waits for valid synchronization, then predicts each queued turn using
the same directional grid arithmetic as Tab5. Commands remain direction/count
only, with no step selector in the ColoPlay editor.

A bounded queue retains unconfirmed turns. A reply retires commands through its
snapshot's completion watermark and replays only newer turns. Accepted results
do not complete commands; older snapshots cannot rewind newer feedback. A
quantized readback is kept separate from its logical reference (for example,
actual raw 6.3488%, target raw 6.25%, displayed 25%). This preserves consistent prediction through rapid
reversals and host grid changes.

Prediction stays visible while turning or awaiting confirmation. After all
turns finish and input has been quiet for 150 ms, the display level blends to
actual feedback over 100 ms. Disconnect, stale/missing state, rejection or a
five-second timeout disables optimism until synchronization recovers. Peers
without the new capability continue to use raw feedback only. Older prediction peers do not interpret the new scale IDs. This uses
bounded static storage and no extra BLE packets or runtime asset work.

Old ColoPlay bindings with ±16 or ±64 are normalized to ±1 on load/save, keeping
their direction, sound, effects, duration and other scene settings. The web
editor offers only Volume Up and Volume Down. The NVS v7 layout is unchanged.

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

Scene **Ring display → Bluetooth playback progress** fills the
physical LED ring from elapsed/duration using scene colors and brightness.
Playing uses a shared smoothed local clock between position packets; repeated
heartbeats/ACKs do not reset it. Pause freezes, Stop clears,
and unknown/stale state clears the progress fill. Partial pixels smooth the nine
LED segments. The progress indicator is independent of the LCD idle screen.
Scene **Ring fill style** offers **Clockwise** or **Bottom to top (symmetric)**.
Symmetric fill lights both lower sides together, rising to the top at 100%.
It also controls knob proportional feedback for Bluetooth volume, using the
same partial-pixel renderer. Existing scenes retain Clockwise until changed.
Knob action light effects temporarily take priority over ring progress, then
return to progress. Button actions affect only their own LEDs; combinations
affect all participating buttons. Each target restores its scene lighting after
the configured duration. Sleep turns both groups off.

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
duration as four uint16 values. New volume commands contain only the Up/Down
usage and detent count: parameter is the fixed legacy host marker 1, duration
and bytes 16–19 are zero. Tab5 accepts legacy step fields during rolling updates
but ignores their grid selection.
Release cancels custom
seeking. Session checks and a bounded recent-request cache prevent duplicate
controls and stale replies after reconnect. Queues are bounded; no old command
is replayed after disconnect. Neither firmware performs MPD socket I/O in the
render/input or BLE callback path.

HELLO reports the native volume grid. Capability bit `CP_CAP_VOLUME_FLOAT`
(0x10) in the capabilities byte and the returning HELLO byte 16 negotiates exact
volume feedback. HELLO bytes 12–15 carry the normalized native step. A companion
`CP_VOLUME_STATE` (kind 20) carries normalized float volume in bytes 8–11 and the
request result in byte 12, using the same request ID/session as STATE. -1 means
unavailable. Negotiated `CP_CAP_VOLUME_PREDICT` plus `CP_CAP_VOLUME_SCALE` adds the scale ID in
byte 13, last completed volume request ID in bytes 14–15, and the confirmed
raw target (float, -1 when unanchored) in bytes 16–19. These fields describe
the same published mixer snapshot, independently of the envelope's request ID.
New peers use this frame for volume and command outcomes; the
legacy STATE remains intact for playback/timing and older peers. All frames
still fit the default 20-byte ATT payload, with full 32-bit elapsed/duration.

STATE carries legacy integer percentage volume, playback
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
chunks. Returned volume and screen/ring feedback keep local response
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

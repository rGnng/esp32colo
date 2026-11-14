# Desktop gadget guide

Current baseline: **0.6.4-smooth-progress**. The gadget is configured through the **General**,
**Scenes** and **Device** pages. Desktop uses a navigation rail; mobile uses
compact navigation tabs. Form inputs and table text are 16 px; labels and
buttons are 14 px, with larger headings and log text. [API](api.md), [development](development.md), and
[build/update/recovery](flash.md) instructions are maintained separately.
The [verified release record](development.md#current-verified-release) identifies
the compiled and OTA-tested image. Bluetooth controls, pairing and feedback are described in the
[Tab5 integration guide](tab5-integration.md); Serial remains deferred.

## Connect

Open `http://coloplay.local/` or the current WiFi IP. In USB-CDC network mode,
the initial device address is **172.32.29.1/30**, with one host DHCP address at
**172.32.29.2**. A local forwarded port such as `127.0.0.1:10081` works only if
that relay is explicitly configured. All connections serve the same settings.
The web page shows the active scene; live screen preview is disabled. Styles,
scripts and icons are served by the gadget with no CDN, external fonts or
Bootstrap JavaScript. Static CSS/JS use compressed, cached local assets. Status
refreshes every two seconds while the page is visible and pauses in hidden tabs.

## General

### WiFi

The panel shows **SSID**, connection status, RSSI in dBm and the current IP.
Click **Edit and restart** to open the SSID/password modal, then **Save and
restart** to apply them. Cancel leaves the settings unchanged. A blank password
retains the stored password. Initial credentials come from `secrets.h`; subsequent boots
and OTA use the `wifi` NVS namespace. Recovery console commands are documented
in [Flashing](flash.md#recovery).

<a id="usb-networking-and-serialjtag"></a>

### USB-CDC Network

Choose CDC-ECM network (macOS/Linux) or serial/JTAG. Configure device IP and
prefix (1–30); the firmware chooses a different usable host in that subnet for
the single DHCP client. No gateway or DNS is advertised. The initial network is
172.32.29.1/30 with peer 172.32.29.2. Saving restarts the gadget. Reconnect/renew
the computer's USB interface when changing its subnet.

Existing custom addresses are preserved. An unchanged previous default of
172.32.29.21/24 with peer 172.32.29.30 migrates to the new /30 default.
Ethernet and fixed serial/JTAG share the USB PHY and are selectable modes;
application recovery always retains serial/JTAG and WiFi/OTA. No eFuse changes.

### NTP servers

Configure one required primary and up to two optional fallback servers, using
hostnames or IPv4 addresses without a scheme/path/port. Timezone is selected
from the available embedded rules. City labels include UTC offsets (both
standard/daylight offsets where applicable). Explicit fixed choices cover
UTC-12:00 through UTC+14:00 in whole hours, plus UTC+05:30; UTC+00:00 uses
the existing Etc/UTC entry. Fixed offsets never change for daylight saving. **Save and sync** stores all fields together;
**Sync now** uses saved settings. Defaults are primary `pool.ntp.org`, empty
fallbacks and UTC. Existing primary/timezone settings migrate without resetting.
SNTP synchronizes hourly and restarts on WiFi reconnect. USB supplies local
access; WiFi remains the gadget's Internet connection.

### Lighting

General LED mode is **Rainbow** or **Static color**. Screen, ring and key LEDs
have independent 0–100% sliders and independent Auto checkboxes. Initial levels
are **70%**, with Auto off. Settings survive reboots/OTA. Scene actions can
change the saved selections or apply temporary effects without replacing them.

Auto smooths successful light readings and scales each selected level from
10% of that level in darkness to 100% in bright light. A 70% selection therefore
ranges from 7% to 70%. A failed read retains the previous sample; after five
seconds without a good sample, Auto uses selected manual levels. **Now** reports
the effective channel brightness, including zero while asleep.

Expand **Light sensor and Auto calibration** for raw/mV values, read/error
counts, browser-session min/max and dark/bright capture. Endpoints default to
0 and 1200; save distinct values within 0–4095. Either sensor polarity works.
Readings are not lux, and custom ADC scaling is not factory hardware evidence.

### Sound

**Scene change sound** enables a short cue when the active scene changes.
Next scene uses a higher tone (880 Hz), Previous a lower tone (440 Hz), including
wraparound. Direct scene selection follows numeric scene order. Cues last
90 ms. The switch defaults on and is saved to NVS. Boot, unchanged selection
and failed saves are silent.

### Sleep

**Sleep after (minutes)** controls inactivity sleep. The default is **60 minutes
(1 hour)**; choose 1–1440 minutes or **0** to disable it, then **Save**. The timer
is stored in NVS. Changing its value starts a fresh countdown. Debounced button
presses, held buttons and complete knob detents reset inactivity. Any button
press or knob turn wakes the gadget, including unbound controls. Changing scenes
or their configuration also wakes it. Web telemetry polling does not keep it awake.

Sleep turns off the display backlight, ring and button LEDs, preserving the
saved brightness and Auto selections. WiFi, USB, OTA and input handling remain
available. **Toggle screen / LEDs** in Device can also wake it. A scene whose
idle screen is Blank still keeps its display off after waking; its LEDs resume.

## Scenes

**Default** always exists, cannot be deleted or renamed, and its bindings are
editable. Each scene is a tab above its settings panel. Selecting a tab edits
that scene; **Activate scene** changes the gadget's running scene. Use **+ Scene**
to create a scene, or **Rename** / **Delete scene** for an existing custom scene.
The active scene is
saved and restored at boot. Deleting the active scene returns to Default;
saved scene-switch actions targeting a deleted scene are retargeted to Default.
A scene switch clears pending gestures and temporary effects and wakes the screen.
It shows the scene icon and name for about 1.6 seconds, then returns to the saved
Clock or Blank idle screen.

The scene settings panel has a **Scene icon** picker with 32 bundled Bootstrap
icons (icon-only button; hover for its name), **Idle screen**, **Ring color**, **Button color**, and brightness controls
for screen, ring and buttons, followed by the key binding table. Each LED group independently uses
**Static color** with its own picker, **Rainbow**, or **Use General**. Click
**Save settings** to persist the scene. Colors apply when that scene is active;
selecting an inactive tab edits its saved configuration. Each brightness channel
has **Use General** or **Override**. Override enables that scene's saved
percentage and Auto checkbox; Use General follows the General percentage/Auto
while remembering the override values. General's Now values show resolved output.
Brightness actions adjust the active scene's override when enabled, otherwise
the General setting; action-driven writes are debounced for 1.5 seconds. Temporary action effects override the
scene colors and return to them when they expire. Proportional ring feedback
leaves the button group's scene color in place.

Existing v1/v2 scenes migrate without changing their names, colors, bindings or
selection. New brightness overrides start at Use General, icons start at House,
and existing bindings are not made Global automatically. The version-3 scene
record is 3304 bytes; older firmware cannot read it, so an application downgrade
is not a scene-settings rollback.

Current bounded NVS storage supports **8 scenes**, **64 configured gestures**
across all scenes, and **3 ordered actions per gesture**. Empty bindings consume
no configured-gesture slot. New scenes start with no assigned actions; bindings
are independent. Default bindings marked Global are available in every scene.

### Global bindings

Only Default bindings have a **Global** checkbox. A scene's own binding wins for
the same key/combination and gesture; otherwise a Global Default binding runs.
The editor warns **exists in Global (Default)** for matching pairs in other
scenes and allows saving a deliberate local override. The table marks Global
rows. Unmarked Default bindings only run while Default is active.

### Gestures

The **Key bindings** table lists configured rows only, with **Key**, **Event**,
**Command**, **Light** and **Actions** columns. Click **+** to add a row. Click a
command to edit its row; **Duplicate** copies the complete command/effect list
into an editor for a free key/event pair. **Delete** removes that binding.

The editor lets you choose the key and event, configure ordered commands and
attach light/screen effects. Changing a saved row's key/event moves it in one
NVS commit. Occupied pairs are rejected rather than overwritten. A duplicate
keeps its source row and copies values, colors, durations and screen effects.
In the table, **CCW** means turn left and **CW** means turn right.

Available events are:

| Control | Available gestures |
|---|---|
| Keys 1–4, OPEN, REC | Single, double, triple, hold, hold then release |
| Combinations of 2–4 front buttons | Single, double, triple, hold, hold then release |
| Center knob press | Single, double, triple, hold, hold then release |
| Center knob rotation | Left, right, press + left, press + right |

Inputs debounce for 25 ms. Recognition uses the active scene's bindings and
Global Default bindings. A single-only input fires on debounced press. If Hold
or Hold then release is configured, a short single fires on release; a hold is
tracked only when one of those bindings exists and reaches its threshold at
600 ms. Hold does not also generate a click. Multi-clicks wait up to 300 ms
between clicks only when Double or Triple is configured. Double without Triple
resolves on the second release; Triple resolves on the third release. Unmapped
gestures are not emitted.

Buttons that could be Any order members or ordered modifiers defer their individual single until
release, allowing another key to join. The knob also defers its single when a
press-and-turn binding exists. Press-and-turn consumes its pending click/release;
turning before the hold threshold suppresses Hold. A Hold that has already fired
cannot be undone by a later turn. Batched rotary notches multiply adjustment actions.

Choose **Button combination** in the Key selector and tick at least two of
Buttons 1–4. An exact group resolves after debounce immediately unless a
larger configured group could still join; that ambiguity allows an 80 ms assembly
window. A configured 1+2+3 takes precedence over 1+2 when it joins within that
window. A matched combination consumes its members' individual gestures.

Release the trigger member to rearm, keeping the common key held: for example,
hold Button 4 and tap Button 2 / Button 3 repeatedly for separate commands.
This also works across scene changes when the next scene has those bindings.
Held members cannot trigger a new group without a fresh press edge. Boot-held
buttons remain suppressed until released.

Combination click/hold timing follows the same binding-aware rules as individual
keys. Configured Hold starts 600 ms after the full group forms; Hold then release
fires when the first member is released. When no combination matches, ordinary
button handling continues. One combination is recognized at a time; unrelated
buttons and knob rotation keep normal handling.

For a scene-switching example, configure **Button 1 + Button 2 / Single press →
Next scene** in Default and enable **Global**. Press together, or keep one held
and tap the other to repeat. This is an editable binding; actual saved mappings
can differ from the example.

These mappings apply while the firmware runs. KEY1 application recovery and
REC/knob ROM-download behavior during reset remain physical recovery mechanisms.

### Ordered actions and effects

Add up to three commands in the row editor, reorder them, then **Save binding**.
Use the table's **Delete** control to clear a binding. The currently implemented
actions belong to these Device groups:

The command selector groups Device actions by purpose and adds Bluetooth controls.

| Group | Actions |
|---|---|
| Scenes | Switch to a scene; Next/Previous Scene |
| Display | Adjust/Set Screen Brightness |
| Lighting | Adjust/Set Ring or Button Brightness; Rainbow; Static Color |
| Speaker | Adjust/Set Device Speaker Volume; Play Tone |
| System | Toggle Screen and LEDs; Synchronize Time |
| No command | No device action, for screen/LED effects alone |

Adjustments take a signed per-step value; setters use an absolute percentage.
Speaker actions control this gadget's ES8311 output, not the computer's volume.
Bluetooth adds remote volume (host step, 1/64, 1/16), playback controls and HID
keys. General contains enable/pair/forget controls. Bluetooth volume/playback
screens and proportional volume ring feedback use returned MPD state. See the
[Tab5 integration guide](tab5-integration.md) for setup, mappings and limits.
Serial companion actions remain deferred.

Each action can keep the current screen or select **Clock**, **Blank**, **Bluetooth volume** or **Bluetooth playback**, and
can retain lighting or temporarily show rainbow, static color, darkness, or proportional
ring fill. Proportional fill uses the returned percentage from a brightness or
speaker action, with partial LED intensity at the boundary. It affects the ring;
key lighting keeps its general mode. Other temporary LED effects affect both
groups while respecting independent brightness. Choose an effect color and a
250–10000 ms duration. Temporary effects expire back to General lighting and
the scene's idle screen. Later effects in an action list replace earlier
ones. A scene-switch action ends the remaining list in that gesture.

**Run saved binding** in the row editor executes that saved binding in the active scene.
It tests configured actions/effects; it does not simulate physical debounce or
multi-click timing. Unsaved drafts and inactive scenes cannot run from the editor.
Last-input telemetry shows the recognized gesture and last action/result.

<a id="clock"></a>

### Standby and screen behavior

Each scene's **Idle screen** can be **Clock**, **Blank**, **Bluetooth volume** or
**Bluetooth playback**. Blank turns off the
display backlight and stops frame updates while leaving the scene's lighting
active. Scene-switch icon/name overlays still appear briefly. Actions can also
apply a temporary Clock or Blank screen effect, then return to the saved idle
screen after the effect duration. The Clock display uses centered Inter
hours/minutes only. A colon appears once valid time has synced
since boot or the last NTP server change; it is not a WiFi connectivity indicator.
There is no date, timezone label, seconds, sync text or decorative ring.

### Initial Default bindings

| Gesture | Action |
|---|---|
| Key 1 single / Key 2 single | Next / previous scene |
| Key 4 hold | Return to Default |
| OPEN single | Toggle screen and LEDs |
| REC single | Play a 660 Hz tone |
| Knob left / right | Gadget speaker volume -2% / +2%, with proportional ring feedback |
| Knob press + left / right | Screen brightness -3% / +3% |

Other gestures start unassigned. The startup tone, fixed diagnostic pages and
automatic page tour have been retired from the normal gadget interface.

## Persistence, device details and updates

All scene definitions, bindings/effects, active selection, network, NTP and
lighting configuration are saved to NVS. Rapid action-driven lighting changes
are debounced for 1.5 seconds before writing; they do not write flash per notch.
Live audio level, temporary effects and input/error counters are runtime state.

**Device** retains hardware telemetry, light/audio
error counts, speaker/LED tests, screen/LED toggle and the authenticated uploader.
See [Flashing and recovery](flash.md) for application-only OTA and boot recovery.

### Status messages and device log

Save, unsaved-change and error notifications appear beside the page title in a
reserved header area. Their appearance does not move configuration panels.
Long messages keep their full text in the tooltip and accessible status region.

Open **Device → Device log** to read recent firmware boot, hardware, WiFi, NTP
and configuration messages. **Pause** stops automatic log reads; **Refresh**
fetches new entries manually, and **Download** saves the currently loaded text
as `coloplay-log.txt`. Automatic reads run every two seconds only while the
viewer is open, Device is selected, the browser tab is visible and not paused.
The view follows new entries when already scrolled to its bottom.

The log uses a 4 KB RAM ring buffer, overwrites oldest bytes when full and resets
on reboot. It does not write NVS or capture ROM/panic output. Large console
status dumps and OTA progress remain serial-only, keeping useful event history
in the bounded buffer. Firmware-upload logs from the separate HTTP OTA module
also remain serial-only.

## Ordered combination modifiers and playback display

In the binding editor, choose a Button combination, select its buttons, then
choose **Modifier · press first**. **None · any order** preserves the existing
combination behavior. A modifier must go down first; press both together to
match only an Any order binding. Hold the modifier and tap another member to
repeat without releasing it. Reversed pairs can be configured independently.

**Ring display** is a scene setting beside the screen/color choices. Select
Bluetooth playback progress for a clockwise fill using track elapsed/duration;
action light effects take priority temporarily. Pause holds the fill, Stop and
unavailable data clear it. This does not change the button LEDs or brightness.
The Bluetooth playback screen has a compact playback-state icon at the top and centered
16 px title/artist lines, with no emoji/boxes. Japanese metadata is supported by
an embedded Noto Sans JP bitmap font. A slim progress bar and time are below;
the LED ring can also show progress. Unavailable feedback uses only the local
`andrsrxn:bluetooth-connecting` icon, without connection text.

**General → Sound → Sound effects volume** controls generated tones, including
scene-switch sounds. It defaults to 70%; zero mutes tones. Remote playback volume
and the gadget codec speaker setting remain independent. See the
[Tab5 scene mapping](tab5-integration.md#installed-test-profile).

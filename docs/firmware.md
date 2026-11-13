# Desktop gadget guide

Current baseline: **0.5.1-wifi-dialog**. The gadget is configured through the **General**,
**Scenes** and **Device** pages. Desktop uses a navigation rail; mobile uses
compact navigation tabs. Form inputs and table text are 16 px; labels and
buttons are 14 px, with larger headings and log text. [API](api.md), [development](development.md), and
[build/update/recovery](flash.md) instructions are maintained separately.

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

## Scenes

**Default** always exists, cannot be deleted or renamed, and its bindings are
editable. Each scene is a tab above its settings panel. Selecting a tab edits
that scene; **Activate scene** changes the gadget's running scene. Use **+ Scene**
to create a scene, or **Rename** / **Delete scene** for an existing custom scene.
The active scene is
saved and restored at boot. Deleting the active scene returns to Default;
saved scene-switch actions targeting a deleted scene are retargeted to Default.
A scene switch clears pending gestures and temporary effects and wakes the screen.
It shows the scene icon and name for about 1.6 seconds, then returns to Clock.

The scene settings panel has a **Scene icon** picker with 32 bundled Bootstrap
icons, **Idle screen**, **Ring color**, **Button color**, and brightness controls
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

Inputs debounce for 25 ms. Single/double clicks resolve after a 300 ms gap;
triple resolves after the third release. Hold fires once at 600 ms and release
fires the separate hold-release binding. Hold does not also generate a click.
Press-and-turn consumes the knob's pending click/release; turning before the
hold threshold also suppresses Hold. A Hold that has already fired cannot be
undone by a later turn. Batched rotary notches multiply adjustment actions.

Choose **Button combination** in the Key selector and tick at least two of
Buttons 1–4. After debounce, buttons must join within 80 ms and overlap. The
exact group is matched, so 1+2+3 does not fire a smaller 1+2 binding. A matched
combination consumes its members' individual gestures until all are released.
Hold begins 600 ms after the full group forms; hold-release waits for all
members to release. Single/double/triple use the existing click window. When no
combination matches, ordinary button handling continues. One combination is
recognized at a time; unrelated buttons and knob rotation keep normal handling.

On this unit, **Button 1 + Button 2 / Single press → Next scene** was added as a
Global example for physical testing. Press together and release both to try it.
It is an editable saved binding, not a hardcoded input action.

These mappings apply while the firmware runs. KEY1 application recovery and
REC/knob ROM-download behavior during reset remain physical recovery mechanisms.

### Ordered actions and effects

Add up to three commands in the row editor, reorder them, then **Save binding**.
Use the table's **Delete** control to clear a binding. Available device actions include:

- Switch to a scene, or cycle to the next/previous existing scene.
- Adjust/set screen, ring, key LED or gadget speaker percentage.
- Select rainbow/static LEDs, toggle screen/LED sleep, request NTP sync, or play a tone.
- No device action, useful when only attaching a screen or LED effect.

Adjustments take a signed per-step value; setters use an absolute percentage.
Speaker actions control this gadget's ES8311 output, not the computer's volume.
Computer data sources/actions belong to the later companion-app phase.

Each action can keep the current screen or select **Clock**, and can retain
lighting or temporarily show rainbow, static color, darkness, or proportional
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

Clock is currently the only implemented screen behavior and is stored as each
scene's idle screen. Action screen overrides are routed through the scene
service and return to that standby after the effect duration. The display uses
centered Inter hours/minutes only. A colon appears once valid time has synced
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
| Knob press + left / right | Screen selection -3% / +3% |

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

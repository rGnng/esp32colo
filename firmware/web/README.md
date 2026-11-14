# Embedded web configuration

Editable sources are `index.html`, `app.css`, `app.js`, `dropdowns.js` and
`update.html`.
`coloplay.svg` is the project icon, sidebar logo and favicon on both pages.
It is bundled as a local, content-hashed SVG asset with the firmware.
`bootstrap.scss` selects a small subset of Bootstrap 5.3.8 from the MIT-licensed
sources in `vendor/bootstrap/`; `bootstrap.min.css` is checked in so normal
firmware builds need neither Sass, npm nor network access.

PlatformIO runs `scripts/pio_web_assets.py` before compilation. It calls the
standard-library packer, which regenerates `include/web_ui.h` and
`include/web_assets.h` only when their content changes. Do not edit generated
headers directly. To pack the sources manually:

```bash
python3 scripts/build_web_assets.py
```

Only when changing the Bootstrap subset or Sass variables:

```bash
.venv/bin/python -m pip install 'libsass==0.23.0'
.venv/bin/python scripts/build_web_assets.py --compile-bootstrap
```

CSS and JavaScript are gzip-compressed on the development computer. Content
hashes in their local `/assets/` paths allow immutable browser caching while
ensuring an OTA with changed assets gets fresh paths. The device sends bytes
straight from flash; it does not compile, compress or decompress assets at
runtime. There are no CDN resources, external fonts, icon packages, Bootstrap
JavaScript, jQuery or screen-image requests. Navigation icons are inline SVG; scene icons use a local sprite. Dialogs are
native HTML, and the app uses its existing native JavaScript handlers.

Only forms, buttons, navs, tables and their required Bootstrap foundation are
included. Layout, readable typography and density are defined in app.css.
Inputs/table text use 16 px; labels/buttons/log text use 14 px. Narrow screens
stack scene field labels above their controls to preserve room for the text. The configuration uses
General, Scenes and Device panels, a desktop navigation rail and mobile tabs.
Add new top-level pages to app.js's `pages` array and matching navigation/panel
markup; APIs and NVS remain the responsibility of the firmware services.

The 0.6.1 panels include:

- General: WiFi status and SSID/password dialog, USB, NTP/timezone, independent
  manual lighting controls, scene-change sound, inactivity timeout, sound-effects gain and Bluetooth
  enable/pair/forget/status.
- Scenes: Clock/Blank/Bluetooth volume/playback idle screen, icon-only selector with named tooltips,
  scene colors/brightness overrides, playback-progress ring, ordered modifier
  combinations, Global bindings and ordered
  grouped Device/Bluetooth commands with screen/light effects and per-action sound.
  Bluetooth keyboard modifiers use a local multi-select dropdown with removable
  tags for all eight modifier bits; the existing numeric API/NVS value is retained.
- Device: diagnostics, runtime sleep/wake control, incremental logs, telemetry
  and the authenticated firmware updater link.

Bluetooth feedback uses the device's BLE service, independently of the two-second
web status poll. Serial companion controls remain deferred. See the
[Tab5 integration guide](../../docs/tab5-integration.md).

Status polls every two seconds while the page is visible. Hidden tabs skip
polls and refresh on return. Full telemetry is formatted only while its Device
panel disclosure is open. Scene edits use the existing CRUD API, with ordered input identities and a
saved ring-display field. Schema migrations preserve older settings. Status messages use reserved, scrollable space to the right of their panel or
dialog title, including when empty. Binding validation and Global warnings share
its title area. Long errors remain readable without changing panel/dialog height.
The updater result and log status use the same placement. The Device log viewer
reads incremental chunks only while open/visible and running; its download is
created locally in the browser.

Scene icons are rebuilt only when their source selection changes:

```bash
.venv/bin/python -m pip install 'cairosvg==2.8.2'
.venv/bin/python scripts/build_scene_icons.py
```

The normal build needs none of those rasterization tools. `scene_icons.h` and
`scene-icons.svg` are checked-in outputs from the 32 MIT-licensed source SVGs.
The native SSID editor uses the unchanged WiFi API. Scene controls include
brightness overrides, icon selection, combination membership and Default-only
Global scope. Conflict warnings intentionally allow a local Global override.

Scene tabs overlap the content border by one pixel so the selected tab joins
its panel. The row still scrolls horizontally on narrow displays.

Modifier option text and row padding explicitly toggle the checkbox; selected
rows are highlighted. Transient focus on the dialog during label activation
does not dismiss the menu. Direct checkbox and keyboard activation stay native.

All visible dropdowns are drawn by the local `dropdowns.js` component, including
grouped timezone/action menus and dynamic binding fields. Hidden selects retain
form submission and the existing value/change API; labels and keyboard focus
point to custom triggers. Menus are placed in the owning dialog or document,
flipping above the field when needed. Long lists have local search. Arrow keys,
Home/End, type-ahead, Enter/Space, Escape and Tab are supported. Programmatic
values, changed options and disabled fieldsets synchronize with the control.
The modifier multi-select remains its existing custom checkbox/tag menu.
All interaction/rendering runs in the browser; ESP32 serves the gzip asset from
flash, without a new library, CDN, network poll or settings schema.

Use plain labels and sentences throughout the interface. Do not use middle-dot
characters as separators in labels, menus, status text, summaries or tooltips.

The scene icon is in the scene heading. Screen/ring behavior and the two color
controls use equal desktop columns and stack on narrow screens, leaving room
for static-color swatches. Keyboard HID key actions use a grouped, searchable
picker with 141 named keys. Search includes names, decimal/hex codes and aliases;
Custom HID code retains the original 1–231 input for unlisted usages. Modifier
masks are independent and retained when changing keys.

Action lighting is scoped to the binding's physical input: individual key,
combination members, or knob ring. The editor exposes on/off/color/brightness
and the existing feedback duration. Brightness can inherit scene/General or
use an absolute percentage, stored with the action. Standby screen and last
input live in a fixed-height strip in the top header. Connection/version and
IP are in the desktop sidebar footer, hidden in the compact mobile layout.
The binding-table heading has no live status text.

Scene Ring fill style selects clockwise or symmetric bottom-to-top coverage
for playback progress and knob percentage feedback, including Bluetooth volume.
The three behavior controls share a desktop row; ring/button colors retain
equal widths below. Narrow layouts use two columns or stack. The setting is
serialized as ring_style and stays separate from ring_display and light color.

Ambient-light sensing, calibration and Auto checkboxes are removed from General
and scenes. Polling needs no light/Auto fields. Manual percentages, per-scene
Use General/Override, action lighting, sleep and ring fill styles remain.

Scene order uses the arrow controls beside + Scene. Moves save immediately and
retain the selected scene ID, active scene and any unsaved settings draft. New
scenes append. Tab navigation and target-scene choices consume API array order.
The header aligns page title, a two-line active-scene block and a two-line
standby/input block on desktop; the latter moves below on narrow screens.
Both status blocks retain fixed height. The Sound and Bluetooth explanatory
paragraphs were removed; their controls and feedback remain.

# Embedded web configuration

Editable sources are `index.html`, `app.css`, `app.js` and `update.html`.
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
JavaScript, jQuery or screen-image requests. Icons are inline SVG, dialogs are
native HTML, and the app uses its existing native JavaScript handlers.

Only forms, buttons, navs, tables and their required Bootstrap foundation are
included. Layout, readable typography and density are defined in app.css.
Inputs/table text use 16 px; labels/buttons/log text use 14 px. Narrow screens
stack scene field labels above their controls to preserve room for the text. The configuration uses
General, Scenes and Device panels, a desktop navigation rail and mobile tabs.
Add new top-level pages to app.js's `pages` array and matching navigation/panel
markup; APIs and NVS remain the responsibility of the firmware services.

Status polls every two seconds while the page is visible. Hidden tabs skip
polls and refresh on return. Full telemetry is formatted only while its Device
panel disclosure is open. Scene edits and saves retain the existing APIs and
NVS schema. Status messages use fixed space beside the page title, including
when empty, so saves and errors cannot shift the panels. The Device log viewer
reads incremental chunks only while open/visible and running; its download is
created locally in the browser.

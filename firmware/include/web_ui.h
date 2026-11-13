#pragma once
#include <Arduino.h>

// Served from flash; no filesystem partition needs to be formatted or flashed.
const char kDemoPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ColoPLAY</title>
<style>
:root{color-scheme:dark;font:15px/1.5 system-ui,sans-serif;background:#0c1420;color:#eef5ff}
[hidden]{display:none!important}
body{max-width:940px;margin:32px auto;padding:0 20px}h1{font-size:28px;margin:0}h2{font-size:18px}
p{color:#a9b9cd}a{color:#7ff3c8}section{background:#141f2e;border:1px solid #2a394b;border-radius:16px;padding:20px;margin:18px 0}
.layout{display:grid;grid-template-columns:260px 1fr;gap:24px}img{width:240px;height:240px;border-radius:50%;background:#081018}
button,input,select{font:inherit;border:1px solid #40536d;border-radius:8px;padding:8px;background:#1d3044;color:#fff}
button{cursor:pointer}button:hover{border-color:#7ff3c8}button:disabled{opacity:.5}label{display:block;margin:10px 0}
input[type=range]{vertical-align:middle;padding:0;width:160px}input[type=password],input[name=ssid]{max-width:90%}
.row{display:flex;gap:8px;flex-wrap:wrap}.bad{color:#ffbc80}.ok{color:#7ff3c8}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:12px}
.light-values{display:flex;flex-wrap:wrap;gap:24px}.light-values strong{font-size:26px;font-variant-numeric:tabular-nums}#light-meter{width:100%;height:24px;accent-color:#7ff3c8}
#message{min-height:1.5em}small{color:#a9b9cd}@media(max-width:640px){.layout{grid-template-columns:1fr}body{margin:20px auto}.preview{text-align:center}}
</style>
<h1>ColoPLAY <span style="color:#7ff3c8">/ gadgets</span></h1>
<p id="connection">Connecting to the device…</p>
<section class="layout"><div class="preview"><img id="screen" alt="Current display framebuffer" src="/api/screenshot.bmp"><br><small>Live render buffer · 240 × 240</small></div><div>
<h2>Screen</h2><div class="row">
<button data-page="4">Clock</button><button data-page="0">Orbit</button><button data-page="1">Inputs</button><button data-page="2">Audio</button><button data-page="3">Sensors</button>
</div><label><input id="tour" type="checkbox"> Automatic tour</label>
<label>Speaker <input id="volume" type="range" min="0" max="100" value="75"> <output id="volume-value">75%</output></label>
<div class="row"><button id="beep">Play test tone</button><button id="led-test">Test all LEDs</button><button id="wake">Screen on / off</button></div>
<p>KEY1 opens the clock; KEY2–4 open Inputs, Audio and Sensors. Turn the knob to adjust screen brightness, or speaker volume on the Audio page. Press the knob to toggle the tour. REC plays a short tone; OPEN toggles the screen and LEDs.</p>
</div></section>
<section><h2>Lighting</h2>
<label>LED mode <select id="led-mode"><option value="rainbow">Rainbow</option><option value="static">Static color</option></select></label>
<label id="led-color-label" hidden>LED color <input id="led-color" type="color" value="#ffffff"></label>
<div><label>Screen brightness <input id="brightness" type="range" min="0" max="100" value="70"> <output id="brightness-value">70%</output></label>
<label><input id="auto-screen" type="checkbox"> Auto screen brightness · <small id="brightness-now">Now 70%</small></label></div>
<div><label>Ring brightness <input id="ring-brightness" type="range" min="0" max="100" value="70"> <output id="ring-brightness-value">70%</output></label>
<label><input id="auto-ring" type="checkbox"> Auto ring brightness · <small id="ring-brightness-now">Now 70%</small></label></div>
<div><label>Key LED brightness <input id="keys-brightness" type="range" min="0" max="100" value="70"> <output id="keys-brightness-value">70%</output></label>
<label><input id="auto-keys" type="checkbox"> Auto key brightness · <small id="keys-brightness-now">Now 70%</small></label></div>
<p>Auto dims from 10% of your selected level in darkness up to the full selected level in bright light. Each control is independent. Changes are saved on the device.</p>
<p id="auto-light-status"></p><p id="message" role="status"></p></section>
<section><h2>Clock</h2><p id="clock-status">Waiting for time…</p><form id="clock-config">
<label>NTP server <input name="ntp_server" id="ntp-server" required maxlength="253" placeholder="pool.ntp.org" autocomplete="off" spellcheck="false"></label>
<label>Timezone <select name="timezone" id="timezone"></select></label>
<small>Use a server hostname or IPv4 address. Settings are saved on the device and apply immediately.</small>
<div class="row"><button id="clock-save">Save and sync</button><button type="button" id="clock-sync">Sync now</button></div>
<p id="clock-message" role="status"></p></form></section>
<section><h2>USB</h2><p id="usb-status">Loading USB settings…</p><form id="usb-config">
<label>Mode <select name="mode" id="usb-mode"><option value="ethernet">USB Ethernet (macOS / Linux)</option><option value="serial_jtag">USB serial + JTAG</option></select></label>
<div class="row"><label>Device IP <input name="address" id="usb-address" required maxlength="15" size="15"></label>
<label>Prefix <input name="prefix" id="usb-prefix" type="number" min="1" max="30" required style="width:4em"></label></div>
<label>Host DHCP address <input name="dhcp_address" id="usb-dhcp" required maxlength="15" size="15"></label>
<p>One DHCP address, no gateway or DNS. The Mac keeps its existing Internet connection. Open this same page at the device IP over USB.</p>
<small>This port supports Ethernet or serial/JTAG. Saving restarts the device.</small><br>
<button id="usb-save">Save and restart</button><p id="usb-message" role="status"></p></form></section>
<section aria-labelledby="light-heading"><h2 id="light-heading">Ambient light</h2>
<p id="light-status" role="status">Waiting for a sensor reading…</p>
<div class="light-values"><div><strong id="light-raw">—</strong><br><small>Raw reading / 4095</small></div><div><strong id="light-mv">—</strong><br><small>Sensor voltage (mV)</small></div></div>
<label for="light-meter">Sensor reading</label><meter id="light-meter" min="0" max="4095" value="0" hidden></meter>
<p>Observed raw range: <span id="light-range">—</span> <button id="light-reset" type="button">Reset range</button></p>
<small id="light-counts">Successful reads: — · Read errors: —</small>
<p>Cover the sensor, then shine a light on it to compare readings. The range tracks this browser session. These values are not calibrated to lux.</p>
<details><summary>Auto brightness calibration</summary><form id="light-calibration">
<label>Dark raw reading <input id="auto-dark-raw" type="number" min="0" max="4095" value="0" required></label>
<button type="button" data-capture="auto-dark-raw">Use current reading as dark</button>
<label>Bright raw reading <input id="auto-bright-raw" type="number" min="0" max="4095" value="1200" required></label>
<button type="button" data-capture="auto-bright-raw">Use current reading as bright</button>
<p>Capture the dark and bright conditions you want Auto to respond to, then save. Both readings must differ.</p><button>Save calibration</button></form></details>
</section>
<section><h2>Device checks</h2><div id="checks">Waiting for telemetry…</div>
<details><summary>Full telemetry</summary><pre id="telemetry"></pre></details></section>
<section><h2>Firmware updates</h2><p><a href="/update">Open HTTP OTA uploader</a> · Login: <code>admin</code>, using your OTA password.</p>
<p>Upload the application <code>firmware.bin</code>. The device reboots after a successful upload.</p></section>
<section><h2>WiFi credentials</h2><form id="wifi"><label>SSID <input name="ssid" required maxlength="32"></label>
<label>Password <input name="password" type="password" maxlength="63" autocomplete="new-password"></label>
<small>Leave the password empty to keep the stored value. Saving restarts the device.</small><br><button>Save and restart</button></form></section>
<script>
const $=id=>document.getElementById(id);let state=null,polling=false,lastImage=0,ssidLoaded=false,lightingLoaded=false,commandPending=0,commandQueue=Promise.resolve();
let lightMin=null,lightMax=null,lightCount=-1,lightSeen=0,lightUptime=0;
function renderLight(){const h=state.hardware,s=h.sensors;
if(state.uptime_ms<lightUptime||s.light_readings<lightCount){lightMin=lightMax=null;lightCount=-1}lightUptime=state.uptime_ms;
const valid=s.light_readings>0&&Number.isInteger(s.light_raw)&&s.light_raw>=0&&s.light_raw<=4095;
if(s.light_readings!==lightCount){lightCount=s.light_readings;lightSeen=Date.now()}
const fresh=valid&&h.ready&&!h.paused&&!s.light_error&&Date.now()-lightSeen<3000;
if(fresh){lightMin=lightMin===null?s.light_raw:Math.min(lightMin,s.light_raw);lightMax=lightMax===null?s.light_raw:Math.max(lightMax,s.light_raw)}
$('light-raw').textContent=valid?s.light_raw:'—';$('light-mv').textContent=valid?s.light_mv:'—';$('light-meter').hidden=!valid;if(valid)$('light-meter').value=s.light_raw;
$('light-range').textContent=lightMin===null?'—':lightMin+' – '+lightMax;
$('light-counts').textContent='Successful reads: '+s.light_readings+' · Read errors: '+s.light_errors;
$('light-status').className=fresh?'ok':'bad';$('light-status').textContent=fresh?'Live · updates every second':s.light_error?(valid?'Last good reading':'No reading yet')+' · sensor read failed (error '+s.light_error+'); retrying…':valid?'Readings paused or stale · showing last good reading':'Waiting for a sensor reading…';}
$('light-reset').onclick=()=>{lightMin=lightMax=null;$('light-range').textContent='—'};
function command(data){commandPending++;const apply=async()=>{try{const r=await fetch('/api/demo',{method:'POST',body:new URLSearchParams(data)});const v=await r.json();if(!r.ok)throw Error(v.error||r.status);$('message').textContent='Applied';if(state)state.hardware=v}catch(e){$('message').textContent=e.message}finally{commandPending--;if(!commandPending){renderLighting();await poll()}}};commandQueue=commandQueue.then(apply,apply);return commandQueue}
function renderLighting(){if(!state||commandPending)return;const h=state.hardware,l=h.leds;
for(const [id,v,now] of [['brightness',h.display.brightness,h.display.effective_brightness],['ring-brightness',l.ring_brightness,l.ring_effective],['keys-brightness',l.keys_brightness,l.keys_effective]]){if(document.activeElement!==$(id)){$(id).value=v;$(id+'-value').textContent=v+'%'}$(id+'-now').textContent='Now '+now+'%'}
for(const [id,value] of [['auto-screen',h.display.auto_brightness],['auto-ring',l.auto_ring],['auto-keys',l.auto_keys]])if(document.activeElement!==$(id))$(id).checked=value;
if(document.activeElement!==$('led-mode'))$('led-mode').value=l.mode;if(document.activeElement!==$('led-color'))$('led-color').value=l.color;
$('led-color-label').hidden=$('led-mode').value!=='static';
if(!lightingLoaded){$('auto-dark-raw').value=h.lighting.auto_dark_raw;$('auto-bright-raw').value=h.lighting.auto_bright_raw;lightingLoaded=true}
$('auto-light-status').textContent=(h.display.auto_brightness||l.auto_ring||l.auto_keys)?h.lighting.sensor_usable?'Auto is using the ambient-light sensor.':'Light reading unavailable; Auto is using the selected levels.':'';}
$('led-mode').onchange=()=>{$('led-color-label').hidden=$('led-mode').value!=='static';command({led_mode:$('led-mode').value,led_color:$('led-color').value})};
$('led-color').onchange=()=>command({led_color:$('led-color').value});
for(const [id,param] of [['auto-screen','auto_screen'],['auto-ring','auto_ring'],['auto-keys','auto_keys']])$(id).onchange=()=>command({[param]:$(id).checked?1:0});
for(const [id,param] of [['brightness','brightness'],['ring-brightness','ring_brightness'],['keys-brightness','keys_brightness'],['volume','volume']]){$(id).oninput=()=>$(id+'-value').textContent=$(id).value+'%';$(id).onchange=()=>command({[param]:$(id).value})}
$('light-calibration').onsubmit=e=>{e.preventDefault();command({auto_dark_raw:$('auto-dark-raw').value,auto_bright_raw:$('auto-bright-raw').value})};
document.querySelectorAll('[data-capture]').forEach(b=>b.onclick=()=>{const s=state?.hardware?.sensors;if(!s||!s.light_readings||s.light_error||Date.now()-lightSeen>=3000){$('message').textContent='Wait for a live light reading before capturing calibration.';return}$(b.dataset.capture).value=s.light_raw});
async function loadClock(){try{const r=await fetch('/api/clock');if(!r.ok)throw Error('HTTP '+r.status);const c=await r.json();$('ntp-server').value=c.ntp_server;$('timezone').replaceChildren(...c.zones.map(z=>new Option(z.label,z.id)));$('timezone').value=c.timezone}catch(e){$('clock-message').textContent='Could not load clock settings: '+e.message}}
$('clock-config').onsubmit=async e=>{e.preventDefault();$('clock-save').disabled=true;try{const r=await fetch('/api/clock',{method:'POST',body:new URLSearchParams(new FormData(e.target))});const c=await r.json();if(!r.ok)throw Error(c.error||r.status);$('clock-message').textContent='Saved. Sync requested.';await poll()}catch(err){$('clock-message').textContent=err.message}finally{$('clock-save').disabled=false}};
$('clock-sync').onclick=async()=>{try{const r=await fetch('/api/clock/sync',{method:'POST'});if(!r.ok)throw Error('HTTP '+r.status);$('clock-message').textContent='Sync requested.'}catch(e){$('clock-message').textContent=e.message}};
async function loadUsb(){try{const r=await fetch('/api/usb');if(!r.ok)throw Error('HTTP '+r.status);const u=await r.json();$('usb-mode').value=u.mode;$('usb-address').value=u.address;$('usb-prefix').value=u.prefix;$('usb-dhcp').value=u.dhcp_address}catch(e){$('usb-message').textContent='Could not load USB settings: '+e.message}}
$('usb-config').onsubmit=async e=>{e.preventDefault();$('usb-save').disabled=true;try{const r=await fetch('/api/usb',{method:'POST',body:new URLSearchParams(new FormData(e.target))});const u=await r.json();if(!r.ok)throw Error(u.error||r.status);$('usb-message').textContent='Saved; restarting…'}catch(err){$('usb-message').textContent=err.message}finally{$('usb-save').disabled=false}};
document.querySelectorAll('[data-page]').forEach(b=>b.onclick=()=>{$('tour').checked=false;command({page:b.dataset.page,auto:0})});
$('tour').onchange=()=>command({auto:$('tour').checked?1:0});
$('beep').onclick=()=>command({tone:660,duration:300});$('wake').onclick=()=>command({awake:state?.hardware?.awake?0:1});
$('led-test').onclick=()=>command({led_test:1});
$('wifi').onsubmit=async e=>{e.preventDefault();try{const r=await fetch('/api/wifi',{method:'POST',body:new URLSearchParams(new FormData(e.target))});if(!r.ok)throw Error('Save failed');$('message').textContent='Saved; restarting…'}catch(err){$('message').textContent=err.message}};
async function poll(){if(polling)return;polling=true;try{const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(5000)});if(!r.ok)throw Error('HTTP '+r.status);state=await r.json();const h=state.hardware,a=h.audio;
$('connection').textContent=state.firmware+' · '+state.wifi.ip+' · '+state.wifi.rssi+' dBm · up '+Math.floor(state.uptime_ms/1000)+' s';
renderLight();
renderLighting();
$('clock-status').textContent=(state.clock.time_valid?state.clock.local_time.slice(11,16):'--:--').replace(':',state.clock.synced?':':' ');
$('usb-status').textContent=state.usb.recovery_override?'Recovery mode: USB serial/JTAG active':state.usb.error?'USB startup error: '+state.usb.error:state.usb.active_mode==='serial_jtag'?'USB serial/JTAG active':(state.usb.link_up?'Ethernet link connected':'Waiting for USB host')+' · device '+state.usb.address+'/'+state.usb.prefix+' · '+(state.usb.lease_active?'host leased '+state.usb.dhcp_address:'DHCP ready for '+state.usb.dhcp_address);
const lines=[state.recovery_mode?'Recovery mode: hardware stopped; HTTP OTA remains available.':h.ready?'Hardware demo running':'Starting hardware…',
'Display: '+h.display.fps+' fps; panel TE: '+h.display.te_hz+' Hz',
'PSRAM: '+(state.psram_bytes/1048576).toFixed(1)+' MiB; pattern test: '+(h.psram.test_passed?'pass':'unverified'),
'Audio: '+(a.codec_ready?'ES8311 detected':'not ready')+'; microphone '+a.mic_dbfs+' dBFS; I/O errors '+a.io_errors,
'Supply: ~'+(h.sensors.vdd_mv_approx/1000).toFixed(2)+' V; light: '+h.sensors.light_raw+(h.sensors.light_error?' (last read failed)':''),
'Encoder position: '+h.encoder.position+'; buttons: '+state.buttons.filter(b=>b.pressed).map(b=>b.name).join(', ')];
$('checks').replaceChildren(...lines.map(s=>{const p=document.createElement('p');p.textContent=s;return p}));$('telemetry').textContent=JSON.stringify(state,null,2);
if(!ssidLoaded){$('wifi').elements.ssid.value=state.wifi.stored_ssid;ssidLoaded=true}
if(document.activeElement!==$('tour'))$('tour').checked=h.auto;
if(document.activeElement!==$('volume')){$('volume').value=a.volume;$('volume-value').textContent=a.volume+'%'}
if(h.ready&&Date.now()-lastImage>2500){lastImage=Date.now();$('screen').src='/api/screenshot.bmp?t='+lastImage}
}catch(e){$('connection').textContent='Device unavailable · '+e.message;$('light-status').className='bad';$('light-status').textContent='Device unavailable · readings are not live'}finally{polling=false}}
loadClock();loadUsb();poll();setInterval(poll,1000);
</script></html>)HTML";

const char kUpdatePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ColoPLAY HTTP OTA</title><style>body{font:16px/1.5 system-ui;max-width:600px;margin:40px auto;padding:0 20px}button,input{font:inherit;margin:12px 0}progress{width:100%}</style>
<h1>Update ColoPLAY firmware</h1><p>Select the application <code>firmware.bin</code> from the ColoPLAY build.</p>
<form id="upload"><input name="firmware" type="file" accept=".bin" required><br><button>Upload and restart</button></form>
<progress id="progress" max="100" value="0"></progress><p id="result" role="status"></p><a href="/">Back to demo</a>
<script>document.getElementById('upload').onsubmit=e=>{e.preventDefault();const b=e.target.querySelector('button');b.disabled=true;const x=new XMLHttpRequest();x.open('POST','/update');x.upload.onprogress=p=>{if(p.lengthComputable)document.getElementById('progress').value=p.loaded/p.total*100};x.onload=()=>{document.getElementById('result').textContent=x.status===200?'Upload complete. Restarting…':x.responseText;b.disabled=false};x.onerror=()=>{document.getElementById('result').textContent='Connection lost. Check device status before retrying.';b.disabled=false};x.send(new FormData(e.target))}</script></html>)HTML";

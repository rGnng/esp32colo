#pragma once
#include <Arduino.h>

// Served from flash; no filesystem partition needs to be formatted or flashed.
const char kDemoPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ColoPLAY hardware demo</title>
<style>
:root{color-scheme:dark;font:15px/1.5 system-ui,sans-serif;background:#0c1420;color:#eef5ff}
body{max-width:940px;margin:32px auto;padding:0 20px}h1{font-size:28px;margin:0}h2{font-size:18px}
p{color:#a9b9cd}a{color:#7ff3c8}section{background:#141f2e;border:1px solid #2a394b;border-radius:16px;padding:20px;margin:18px 0}
.layout{display:grid;grid-template-columns:260px 1fr;gap:24px}img{width:240px;height:240px;border-radius:50%;background:#081018}
button,input,select{font:inherit;border:1px solid #40536d;border-radius:8px;padding:8px;background:#1d3044;color:#fff}
button{cursor:pointer}button:hover{border-color:#7ff3c8}button:disabled{opacity:.5}label{display:block;margin:10px 0}
input[type=range]{vertical-align:middle;padding:0;width:160px}input[type=password],input[name=ssid]{max-width:90%}
.row{display:flex;gap:8px;flex-wrap:wrap}.bad{color:#ffbc80}.ok{color:#7ff3c8}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:12px}
#message{min-height:1.5em}small{color:#a9b9cd}@media(max-width:640px){.layout{grid-template-columns:1fr}body{margin:20px auto}.preview{text-align:center}}
</style>
<h1>ColoPLAY <span style="color:#7ff3c8">/ hardware demo</span></h1>
<p id="connection">Connecting to the device…</p>
<section class="layout"><div class="preview"><img id="screen" alt="Current display framebuffer" src="/api/screenshot.bmp"><br><small>Live render buffer · 240 × 240</small></div><div>
<h2>Try the hardware</h2><div class="row">
<button data-page="0">Orbit</button><button data-page="1">Inputs</button><button data-page="2">Audio</button><button data-page="3">Sensors</button>
</div><label><input id="tour" type="checkbox" checked> Automatic tour</label>
<label>Screen <input id="brightness" type="range" min="0" max="100" value="65"> <output id="brightness-value">65%</output></label>
<label>LEDs <input id="leds" type="range" min="0" max="64" value="40"> <output id="leds-value">40 / 64</output></label>
<label>Speaker <input id="volume" type="range" min="0" max="100" value="75"> <output id="volume-value">75%</output></label>
<div class="row"><button id="beep">Play test tone</button><button id="led-test">Test all LEDs</button><button id="wake">Screen on / off</button></div>
<p>Keys 1–4 select a page. Turn the knob to adjust screen brightness, or speaker volume on the Audio page. Press the knob to toggle the tour. REC plays a short tone; OPEN toggles the screen and LEDs.</p>
</div></section>
<section><h2>Device checks</h2><div id="checks">Waiting for telemetry…</div><p id="message" role="status"></p>
<details><summary>Full telemetry</summary><pre id="telemetry"></pre></details></section>
<section><h2>Firmware updates</h2><p><a href="/update">Open HTTP OTA uploader</a> · Login: <code>admin</code>, using your OTA password.</p>
<p>Upload the application <code>firmware.bin</code>. The device reboots after a successful upload.</p></section>
<section><h2>WiFi credentials</h2><form id="wifi"><label>SSID <input name="ssid" required maxlength="32"></label>
<label>Password <input name="password" type="password" maxlength="63" autocomplete="new-password"></label>
<small>Leave the password empty to keep the stored value. Saving restarts the device.</small><br><button>Save and restart</button></form></section>
<script>
const $=id=>document.getElementById(id);let state=null,polling=false,lastImage=0,ssidLoaded=false;
async function command(data){try{const r=await fetch('/api/demo',{method:'POST',body:new URLSearchParams(data)});const v=await r.json();if(!r.ok)throw Error(v.error||r.status);$('message').textContent='Applied';await poll()}catch(e){$('message').textContent=e.message}}
document.querySelectorAll('[data-page]').forEach(b=>b.onclick=()=>{$('tour').checked=false;command({page:b.dataset.page,auto:0})});
$('tour').onchange=()=>command({auto:$('tour').checked?1:0});
for(const key of ['brightness','leds','volume']){$(key).oninput=()=>$(key+'-value').textContent=$(key).value+(key==='leds'?' / 64':'%');$(key).onchange=()=>command({[key]:$(key).value})}
$('beep').onclick=()=>command({tone:660,duration:300});$('wake').onclick=()=>command({awake:state?.hardware?.awake?0:1});
$('led-test').onclick=()=>command({led_test:1,leds:48});
$('wifi').onsubmit=async e=>{e.preventDefault();try{const r=await fetch('/api/wifi',{method:'POST',body:new URLSearchParams(new FormData(e.target))});if(!r.ok)throw Error('Save failed');$('message').textContent='Saved; restarting…'}catch(err){$('message').textContent=err.message}};
async function poll(){if(polling)return;polling=true;try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);state=await r.json();const h=state.hardware,a=h.audio;
$('connection').textContent=state.firmware+' · '+state.wifi.ip+' · '+state.wifi.rssi+' dBm · up '+Math.floor(state.uptime_ms/1000)+' s';
const lines=[state.recovery_mode?'Recovery mode: hardware stopped; HTTP OTA remains available.':h.ready?'Hardware demo running':'Starting hardware…',
'Display: '+h.display.fps+' fps; panel TE: '+h.display.te_hz+' Hz',
'PSRAM: '+(state.psram_bytes/1048576).toFixed(1)+' MiB; pattern test: '+(h.psram.test_passed?'pass':'unverified'),
'Audio: '+(a.codec_ready?'ES8311 detected':'not ready')+'; microphone '+a.mic_dbfs+' dBFS; I/O errors '+a.io_errors,
'Supply: ~'+(h.sensors.vdd_mv_approx/1000).toFixed(2)+' V; light: '+h.sensors.light_raw+(h.sensors.light_error?' (last read failed)':''),
'Encoder position: '+h.encoder.position+'; buttons: '+state.buttons.filter(b=>b.pressed).map(b=>b.name).join(', ')];
$('checks').replaceChildren(...lines.map(s=>{const p=document.createElement('p');p.textContent=s;return p}));$('telemetry').textContent=JSON.stringify(state,null,2);
if(!ssidLoaded){$('wifi').elements.ssid.value=state.wifi.stored_ssid;ssidLoaded=true}
if(document.activeElement!==$('tour'))$('tour').checked=h.auto;
for(const key of ['brightness','leds','volume']){const v=key==='brightness'?h.display.brightness:key==='leds'?h.leds.brightness:a.volume;if(document.activeElement!==$(key)){$(key).value=v;$(key+'-value').textContent=v+(key==='leds'?' / 64':'%')}}
if(h.ready&&Date.now()-lastImage>2500){lastImage=Date.now();$('screen').src='/api/screenshot.bmp?t='+lastImage}
}catch(e){$('connection').textContent='Device unavailable · '+e.message}finally{polling=false}}
poll();setInterval(poll,1000);
</script></html>)HTML";

const char kUpdatePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ColoPLAY HTTP OTA</title><style>body{font:16px/1.5 system-ui;max-width:600px;margin:40px auto;padding:0 20px}button,input{font:inherit;margin:12px 0}progress{width:100%}</style>
<h1>Update ColoPLAY firmware</h1><p>Select the application <code>firmware.bin</code> from the ColoPLAY build.</p>
<form id="upload"><input name="firmware" type="file" accept=".bin" required><br><button>Upload and restart</button></form>
<progress id="progress" max="100" value="0"></progress><p id="result" role="status"></p><a href="/">Back to demo</a>
<script>document.getElementById('upload').onsubmit=e=>{e.preventDefault();const b=e.target.querySelector('button');b.disabled=true;const x=new XMLHttpRequest();x.open('POST','/update');x.upload.onprogress=p=>{if(p.lengthComputable)document.getElementById('progress').value=p.loaded/p.total*100};x.onload=()=>{document.getElementById('result').textContent=x.status===200?'Upload complete. Restarting…':x.responseText;b.disabled=false};x.onerror=()=>{document.getElementById('result').textContent='Connection lost. Check device status before retrying.';b.disabled=false};x.send(new FormData(e.target))}</script></html>)HTML";

#pragma once

// Embedded local overview page (PROGMEM). Served at GET /.

#include <Arduino.h>

static const char OVERVIEW_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>iCaNova Edge Gateway</title>
<style>
:root{
  --bg:#111820;--bg2:#263746;--ink:#F2F1ED;--muted:#B8BABC;
  --accent:#A8754D;--warn:#D18A26;--bad:#C64747;--ok:#24845B;--line:#637B83;
  --font:"Segoe UI",system-ui,sans-serif;
}
*{box-sizing:border-box}
body{margin:0;font-family:var(--font);background:var(--bg);
color:var(--ink);min-height:100vh}
.wrap{max-width:960px;margin:0 auto;padding:1.25rem 1rem 2.5rem}
nav{display:flex;gap:.4rem;margin:.7rem 0 0}
nav button,button.act,.tools button,.tools select,.tools input{font:inherit;color:var(--ink);background:var(--bg2);border:1px solid var(--line);padding:.35rem .65rem}
nav button.on{border-color:var(--accent);color:#F2F1ED}
.tools{display:flex;flex-wrap:wrap;gap:.4rem;align-items:center;margin:.7rem 0}
table.cfg{width:100%;border-collapse:collapse;font-size:.85rem}
table.cfg th,table.cfg td{border-bottom:1px solid var(--line);text-align:left;padding:.4rem .3rem;vertical-align:top}
table.cfg th{color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.04em}
.scroll{overflow-x:auto}
.brand{font-size:clamp(1.6rem,5vw,2.2rem);font-weight:700;letter-spacing:.02em;margin:0}
.sub{color:var(--muted);margin:.35rem 0 1.25rem;font-size:.95rem}
.hero{padding:1.25rem 0 1rem;border-bottom:1px solid var(--line)}
.headline{font-size:clamp(1.8rem,6vw,2.6rem);font-weight:650;margin:.2rem 0;line-height:1.1}
.meta{color:var(--muted);font-size:.85rem}
.pill{display:inline-block;padding:.15rem .55rem;border-radius:4px;font-size:.75rem;
background:var(--bg2);border:1px solid var(--line);margin-right:.35rem}
.pill.ok{border-color:var(--ok);color:#9fd9b8}
.pill.bad{border-color:var(--bad);color:#f0a8a8}
.pill.warn{border-color:var(--warn);color:#e8d08a}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:.65rem;margin-top:1.1rem}
@media(min-width:560px){.grid{grid-template-columns:repeat(3,1fr)}}
.metric{background:var(--bg2);border:1px solid var(--line);padding:.75rem .85rem}
.metric .label{color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.06em}
.metric .val{font-size:1.35rem;font-weight:600;margin-top:.2rem;font-variant-numeric:tabular-nums}
.metric .unit{color:var(--muted);font-size:.8rem;font-weight:400}
.section{margin-top:1.4rem}
.section h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);
margin:0 0 .55rem;font-weight:600}
.alarms{list-style:none;padding:0;margin:0}
.alarms li{padding:.55rem .7rem;border:1px solid var(--line);background:var(--bg2);margin-bottom:.4rem;
font-size:.9rem}
.alarms .st{color:var(--bad);font-weight:600}
.empty{color:var(--muted);font-size:.9rem}
.stale{opacity:.55}
footer{margin-top:2rem;color:var(--muted);font-size:.75rem}
#map{height:260px;margin-top:.7rem;border:1px solid var(--line);background:var(--bg2)}
.bar{height:.45rem;background:var(--bg2);border:1px solid var(--line);margin:.5rem 0}
.bar>i{display:block;height:100%;background:var(--accent);width:0}
</style>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
</head>
<body>
<div class="wrap" id="app">
  <header class="hero">
    <p class="brand">iCaNova</p>
    <p class="sub">iCaNova Edge Gateway</p>
    <p class="meta">Imagine. Connect. Innovate. · Family: iCaNova Edge</p>
    <nav>
      <button type="button" id="tabMon" class="on">Monitor</button>
      <button type="button" id="tabCfg">Configure</button>
      <button type="button" id="tabLoc">Location</button>
      <button type="button" id="tabOta">Update</button>
    </nav>
    <p class="sub" id="support">Connecting…</p>
    <div class="headline" id="headline">—</div>
    <p class="meta" id="meta"></p>
  </header>
  <div id="monitor">
  <div class="grid" id="metrics"></div>
  <section class="section">
    <h2>Active alarms</h2>
    <ul class="alarms" id="alarms"><li class="empty">Waiting for data…</li></ul>
  </section>
  </div>
  <div id="configure" hidden>
    <div class="tools">
      <select id="cfgPage" aria-label="GenComm page"></select>
      <input id="cfgFind" aria-label="Filter registers" placeholder="Filter name or address"/>
      <label class="meta"><input type="checkbox" id="cfgRw"/> Writable only</label>
      <button type="button" id="cfgRead" class="act">Read page</button>
    </div>
    <section class="section">
      <h2>Control</h2>
      <div class="tools" id="cfgControl"></div>
      <p class="meta">Stop and off mode stay blocked.</p>
    </section>
    <div class="scroll"><table class="cfg"><thead><tr><th>Address</th><th>Name</th><th>Access</th><th>Value</th><th>Write</th></tr></thead><tbody id="cfgRows"></tbody></table></div>
    <p class="meta" id="cfgMsg"></p>
  </div>
  <div id="location" hidden>
    <section class="section">
      <h2>Site coordinates</h2>
      <p class="meta">Saved on this gateway. Sync to cloud when STA has internet and uplink is configured.</p>
      <div class="tools">
        <label class="meta">Lat <input id="locLat" inputmode="decimal" size="12"/></label>
        <label class="meta">Lon <input id="locLon" inputmode="decimal" size="12"/></label>
        <button type="button" id="locGeo" class="act">Use my location</button>
        <button type="button" id="locSave" class="act">Save</button>
        <button type="button" id="locSync" class="act">Save + sync cloud</button>
      </div>
      <div id="map"></div>
      <p class="meta" id="locMsg"></p>
    </section>
  </div>
  <div id="ota" hidden>
    <section class="section">
      <h2>Firmware update</h2>
      <p class="meta" id="otaMeta">Loading…</p>
      <div class="tools">
        <input type="file" id="otaFile" accept=".bin,application/octet-stream"/>
        <button type="button" id="otaUpload" class="act">Upload .bin</button>
        <button type="button" id="otaCheck" class="act">Check cloud</button>
        <button type="button" id="otaApply" class="act">Apply cloud update</button>
      </div>
      <div class="bar"><i id="otaBar"></i></div>
      <p class="meta" id="otaMsg"></p>
    </section>
  </div>
  <footer>iCaNova Edge Gateway · ASIEDU DEVELOPMENT HUB (ADH).</footer>
</div>
<script>
const $=id=>document.getElementById(id);
function fmt(v,d){return v==null||v===undefined?'—':Number(v).toFixed(d)}
function pill(link){
  if(link==='ok')return '<span class="pill ok">link ok</span>';
  if(link==='degraded')return '<span class="pill warn">degraded</span>';
  if(link==='fail')return '<span class="pill bad">link fail</span>';
  return '<span class="pill">link ?</span>';
}
function metric(label,val,unit){
  return `<div class="metric"><div class="label">${label}</div><div class="val">${val}`+
    (unit?` <span class="unit">${unit}</span>`:'')+`</div></div>`;
}
function render(s){
  const stale=s.ageMs>8000;
  $('app').classList.toggle('stale',stale);
  $('headline').textContent=s.headline||'—';
  const mode=s.opMode?s.opMode.name:'—';
  const eng=s.engStatus?s.engStatus.name:'—';
  $('support').innerHTML=`${mode} · ${eng} · ${pill(s.link)}`+
    (stale?' <span class="pill warn">stale</span>':'');
  $('meta').textContent=`slave ${s.slaveId} · ${s.wifiMode} ${s.wifiIp}`+
    (s.rssi?` · ${s.rssi} dBm`:'')+` · age ${s.ageMs||0} ms`+
    (s.lastError?` · ${s.lastError}`:'');
  $('metrics').innerHTML=[
    metric('Battery',fmt(s.batteryV,1),'V'),
    metric('Engine RPM',fmt(s.engRpm,0),''),
    metric('Frequency',fmt(s.genFreq,1),'Hz'),
    metric('L1 voltage',fmt(s.genVan,1),'V'),
    metric('L2 voltage',fmt(s.genVbn,1),'V'),
    metric('L3 voltage',fmt(s.genVcn,1),'V'),
    metric('L1 current',fmt(s.genIa,1),'A'),
    metric('L2 current',fmt(s.genIb,1),'A'),
    metric('L3 current',fmt(s.genIc,1),'A'),
    metric('Total power',fmt(s.genPTotKw,2),'kW'),
    metric('Load',fmt(s.genPctKw,1),'%'),
  ].join('');
  const al=s.alarms||[];
  if(!al.length){
    $('alarms').innerHTML='<li class="empty">No active alarms</li>';
  }else{
    $('alarms').innerHTML=al.map(a=>
      `<li><span class="st">${a.state}</span> — ${a.name} <span class="meta">@${a.addr}[${a.slot}]</span></li>`
    ).join('');
  }
}
async function tick(){
  try{
    const r=await fetch('/api/status');
    if(!r.ok)throw new Error('HTTP '+r.status);
    render(await r.json());
  }catch(e){
    $('headline').textContent='Offline';
    $('support').textContent='Cannot reach /api/status';
    $('app').classList.add('stale');
  }
}
let catalog=null, pageId=null;
const SENT=['reserved','low input','high input','bad data','transducer fault','under range','over range','unimplemented'];
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function sentName(raw,ty){
  const signed=ty&&/s/i.test(ty);
  if(!signed&&raw>=0xFFF8&&raw<=0xFFFF)return SENT[raw-0xFFF8];
  if(signed&&raw>=0x7FF8&&raw<=0x7FFF)return SENT[raw-0x7FF8];
  return null;
}
function showWord(raw,ty){
  if(raw==null)return '—';
  const name=sentName(raw,ty);
  if(name)return name;
  return raw+' (0x'+raw.toString(16).toUpperCase().padStart(4,'0')+')';
}
function currentPage(){return catalog&&catalog.pages.find(p=>p.p===pageId);}
function renderCfg(){
  const page=currentPage();
  if(!page)return;
  const q=$('cfgFind').value.trim().toLowerCase();
  const rwOnly=$('cfgRw').checked;
  const rows=page.regs.filter(r=>{
    if(rwOnly&&r.a==='r')return false;
    if(!q)return true;
    return r.n.toLowerCase().includes(q)||String(page.b+r.o).includes(q);
  });
  $('cfgRows').innerHTML=rows.map(r=>{
    const addr=page.b+r.o;
    const access=r.a==='rw'?'read/write':(r.a==='w'?'write':'read');
    const value=(r.v||[]).map(w=>showWord(w,r.ty)).join('<br>')||'—';
    const write=r.a==='r'?'':`<input data-addr="${addr}" placeholder="${r.comp?'value':(r.w+' words')}" size="12"/> <button type="button" class="act" data-write="${addr}">Write</button>`;
    const extra=r.comp?' <span class="meta">+ complement</span>':'';
    return `<tr><td>${addr}</td><td>${esc(r.n)}${r.ty?' <span class="meta">'+esc(r.ty)+'</span>':''}${extra}</td><td>${access}</td><td>${value}</td><td>${write}</td></tr>`;
  }).join('')||'<tr><td colspan="5" class="empty">No registers match.</td></tr>';
}
async function loadCatalog(){
  if(catalog)return;
  $('cfgMsg').textContent='Loading pages…';
  const res=await fetch('/api/catalog');
  if(!res.ok)throw new Error('HTTP '+res.status);
  catalog=await res.json();
  $('cfgPage').innerHTML=catalog.pages.map(p=>`<option value="${p.p}">Page ${p.p} — ${esc(p.t)}</option>`).join('');
  pageId=catalog.pages[0]?catalog.pages[0].p:null;
  $('cfgControl').innerHTML=catalog.scf.filter(c=>!c.blocked).map(c=>`<button type="button" class="act" data-key="${c.k}" data-risk="${c.risk?1:0}">${esc(c.n)}</button>`).join('');
  $('cfgMsg').textContent=catalog.pages.length+' pages. Values shown are the last snapshot until you read the module.';
  renderCfg();
}
let map=null,marker=null,leafletReady=false;
function ensureMap(){
  if(map||typeof L==='undefined'){if(map)setTimeout(()=>map.invalidateSize(),50);return;}
  leafletReady=true;
  map=L.map('map').setView([5.6,-0.2],6);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'© OSM'}).addTo(map);
  map.on('click',e=>{
    $('locLat').value=e.latlng.lat.toFixed(6);
    $('locLon').value=e.latlng.lng.toFixed(6);
    if(marker)marker.setLatLng(e.latlng);else marker=L.marker(e.latlng).addTo(map);
  });
}
function setMapPoint(lat,lon){
  if(!map||!Number.isFinite(lat)||!Number.isFinite(lon))return;
  const ll=[lat,lon];
  map.setView(ll,14);
  if(marker)marker.setLatLng(ll);else marker=L.marker(ll).addTo(map);
}
async function loadLocation(){
  $('locMsg').textContent='Loading…';
  try{
    const r=await fetch('/api/location');
    const j=await r.json();
    if(j.lat!=null)$('locLat').value=j.lat;
    if(j.lon!=null)$('locLon').value=j.lon;
    ensureMap();
    if(j.lat!=null&&j.lon!=null)setMapPoint(Number(j.lat),Number(j.lon));
    $('locMsg').textContent=j.staConnected?'STA online — map and cloud sync available when this browser has internet.':'STA offline — local save still works.';
  }catch(e){$('locMsg').textContent=e.message;}
}
async function saveLocation(sync){
  const lat=Number($('locLat').value),lon=Number($('locLon').value);
  if(!Number.isFinite(lat)||!Number.isFinite(lon)){$('locMsg').textContent='Enter valid lat and lon.';return;}
  $('locMsg').textContent='Saving…';
  try{
    const r=await fetch('/api/location',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lat,lon,sync:!!sync})});
    const j=await r.json();
    if(!r.ok||!j.ok){$('locMsg').textContent=j.error||'save failed';return;}
    setMapPoint(lat,lon);
    $('locMsg').textContent=sync?(j.cloudSynced?'Saved and synced to cloud.':'Saved locally. Cloud: '+(j.cloudError||'failed')):'Saved locally.';
  }catch(e){$('locMsg').textContent=e.message;}
}
async function loadOta(){
  try{
    const r=await fetch('/api/ota');
    const j=await r.json();
    $('otaMeta').textContent='Firmware '+j.fw+' · '+j.git+' · slot '+j.slot+(j.otaEnable?' · cloud OTA on':' · cloud OTA off')+' · channel '+(j.channel||'stable');
    $('otaBar').style.width=(j.progress||0)+'%';
    if(j.error)$('otaMsg').textContent=j.error;
  }catch(e){$('otaMeta').textContent=e.message;}
}
function show(which){
  $('monitor').hidden=which!=='mon';
  $('configure').hidden=which!=='cfg';
  $('location').hidden=which!=='loc';
  $('ota').hidden=which!=='ota';
  $('tabMon').classList.toggle('on',which==='mon');
  $('tabCfg').classList.toggle('on',which==='cfg');
  $('tabLoc').classList.toggle('on',which==='loc');
  $('tabOta').classList.toggle('on',which==='ota');
  if(which==='cfg')loadCatalog().catch(e=>{$('cfgMsg').textContent=e.message;});
  if(which==='loc')loadLocation();
  if(which==='ota')loadOta();
}
$('tabMon').onclick=()=>show('mon');
$('tabCfg').onclick=()=>show('cfg');
$('tabLoc').onclick=()=>show('loc');
$('tabOta').onclick=()=>show('ota');
$('locGeo').onclick=()=>{
  if(!navigator.geolocation){$('locMsg').textContent='Geolocation not available.';return;}
  $('locMsg').textContent='Getting location…';
  navigator.geolocation.getCurrentPosition(p=>{
    $('locLat').value=p.coords.latitude.toFixed(6);
    $('locLon').value=p.coords.longitude.toFixed(6);
    ensureMap();
    setMapPoint(p.coords.latitude,p.coords.longitude);
    $('locMsg').textContent='Location from browser.';
  },e=>{$('locMsg').textContent=e.message||'geolocation failed';},{enableHighAccuracy:true,timeout:15000});
};
$('locSave').onclick=()=>saveLocation(false);
$('locSync').onclick=()=>saveLocation(true);
$('otaUpload').onclick=async()=>{
  const f=$('otaFile').files&&$('otaFile').files[0];
  if(!f){$('otaMsg').textContent='Choose a .bin file.';return;}
  if(!confirm('Upload '+f.name+' ('+f.size+' bytes) and reboot?'))return;
  $('otaMsg').textContent='Uploading…';
  try{
    const fd=new FormData();
    fd.append('firmware',f,f.name||'firmware.bin');
    const r=await fetch('/api/ota',{method:'POST',body:fd});
    const j=await r.json();
    if(!r.ok||!j.ok){$('otaMsg').textContent=j.error||'upload failed';return;}
    $('otaBar').style.width='100%';
    $('otaMsg').textContent='Update applied. Rebooting…';
  }catch(e){$('otaMsg').textContent=e.message;}
};
async function cloudOta(apply){
  $('otaMsg').textContent=apply?'Applying cloud update…':'Checking cloud…';
  try{
    const r=await fetch('/api/ota/cloud',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({apply:!!apply})});
    const j=await r.json();
    await loadOta();
    if(j.rebooting){$('otaMsg').textContent='Update applied. Rebooting…';return;}
    $('otaMsg').textContent=j.error||(j.ok?'Up to date.':'Check failed.');
  }catch(e){$('otaMsg').textContent=e.message;}
}
$('otaCheck').onclick=()=>cloudOta(false);
$('otaApply').onclick=()=>{if(confirm('Download and apply the published firmware from cloud?'))cloudOta(true);};
(function(){
  const s=document.createElement('script');
  s.src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
  s.onload=()=>{leafletReady=true;};
  document.head.appendChild(s);
})();
$('cfgPage').onchange=()=>{pageId=Number($('cfgPage').value);renderCfg();};
$('cfgFind').oninput=renderCfg;
$('cfgRw').onchange=renderCfg;
$('cfgRead').onclick=async()=>{
  const page=currentPage();
  if(!page)return;
  $('cfgMsg').textContent='Reading…';
  try{
    const addrs=[];
    page.regs.forEach(r=>{for(let i=0;i<r.w;i++)addrs.push(page.b+r.o+i);});
    addrs.sort((a,b)=>a-b);
    let i=0;
    while(i<addrs.length){
      let qty=1;
      while(i+qty<addrs.length&&addrs[i+qty]===addrs[i]+qty&&qty<8)qty++;
      const res=await fetch('/api/read?addr='+addrs[i]+'&qty='+qty);
      const body=await res.json();
      if(!res.ok||body.result!=='ok')throw new Error(body.error||body.result||'read failed');
      for(let n=0;n<qty;n++){
        const at=addrs[i]+n;
        page.regs.forEach(r=>{
          const start=page.b+r.o;
          if(at>=start&&at<start+r.w){
            if(!r.v)r.v=Array(r.w).fill(null);
            r.v[at-start]=body.words[n];
          }
        });
      }
      i+=qty;
    }
    $('cfgMsg').textContent='Read page '+page.p+'.';
    renderCfg();
  }catch(e){$('cfgMsg').textContent=e.message;}
};
$('configure').onclick=async(ev)=>{
  const t=ev.target;
  if(!(t instanceof HTMLElement))return;
  const write=t.getAttribute('data-write');
  const key=t.getAttribute('data-key');
  if(write){
    const page=currentPage();
    const addr=Number(write);
    const reg=page&&page.regs.find(r=>page.b+r.o===addr);
    const input=document.querySelector('input[data-addr="'+addr+'"]');
    const text=input?input.value.trim():'';
    const parts=text.split(/[,\s]+/).filter(Boolean).map(Number);
    if(!reg||parts.some(n=>!Number.isInteger(n)||n<0||n>65535)){$('cfgMsg').textContent='Enter integers from 0 to 65535.';return;}
    let words=parts;
    if(reg.comp){
      if(parts.length!==1){$('cfgMsg').textContent='Enter one value. The complement is added.';return;}
      words=[parts[0], (65535-parts[0])&65535];
    }else if(parts.length!==reg.w){$('cfgMsg').textContent='Enter '+reg.w+' word(s).';return;}
    const ask=reg.comp?('Write '+words[0]+' and complement '+words[1]+' to '+addr+'? A wrong password locks the module after three tries.'):('Write '+words.join(', ')+' to '+addr+' ('+reg.n+')?');
    if(!confirm(ask))return;
    const res=await fetch('/api/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({addr,words})});
    const body=await res.json();
    if(!res.ok||body.result!=='ok'){$('cfgMsg').textContent=body.error||body.result||'write failed';return;}
    if(!reg.v)reg.v=Array(reg.w).fill(null);
    for(let n=0;n<reg.w&&n<words.length;n++)reg.v[n]=words[n];
    $('cfgMsg').textContent='Wrote '+addr+'.';
    renderCfg();
  }
  if(key){
    const name=t.textContent||'command';
    const risk=t.getAttribute('data-risk')==='1';
    const ask=risk?(name+' can start the engine, change mode, or move a breaker. Send it?'):('Send '+name+'?');
    if(!confirm(ask))return;
    const res=await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({key:Number(key)})});
    const body=await res.json();
    $('cfgMsg').textContent=(!res.ok||body.result!=='ok')?(body.error||body.result||'control failed'):('Sent '+name+'.');
  }
};
tick();
setInterval(tick,2000);
</script>
</body>
</html>)HTML";

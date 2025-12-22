async function loadConfig(){
  const r = await fetch('/api/time/config');
  const cfg = await r.json();
  document.getElementById('ntp-enabled').checked = !!cfg.enabled;
  document.getElementById('ntp-server').value = cfg.server || 'pool.ntp.org';
  document.getElementById('ntp-interval').value = cfg.intervalSec || 3600;
  document.getElementById('tz-offset').value = (typeof cfg.tzOffsetMinutes==='number')?cfg.tzOffsetMinutes:0;
}
async function save(){
  const body = {
    enabled: document.getElementById('ntp-enabled').checked,
    server: document.getElementById('ntp-server').value.trim() || 'pool.ntp.org',
    intervalSec: parseInt(document.getElementById('ntp-interval').value,10)||3600,
    tzOffsetMinutes: parseInt(document.getElementById('tz-offset').value,10)||0,
  };
  const r = await fetch('/api/time/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const j = await r.json();
  document.getElementById('save-status').textContent = j.success? 'Saved' : 'Failed';
  setTimeout(()=>document.getElementById('save-status').textContent='',2000);
}
async function syncNow(){
  const sensorStr = document.getElementById('ts-sensor').value.trim();
  const tz = parseInt(document.getElementById('tz-offset').value,10)||0;
  const body = sensorStr? {sensorId:parseInt(sensorStr,10), tzOffsetMinutes:tz} : {all:true, tzOffsetMinutes:tz};
  const r = await fetch('/api/time/sync',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  let j = null;
  try {
    j = await r.json();
  } catch (e) {
    // Leave j null
  }

  const msg = (j && (j.error || j.message)) ? (j.error || j.message) : (r.ok ? 'Unknown error' : `HTTP ${r.status}`);
  document.getElementById('sync-status').textContent = (j && j.success)
    ? `Sent ${j.commandsSent}`
    : `Failed: ${msg}`;
  setTimeout(()=>document.getElementById('sync-status').textContent='',2000);
}
function useBrowserTz(){
  const minutesWest = new Date().getTimezoneOffset();
  const minutesEast = -minutesWest;
  document.getElementById('tz-offset').value = minutesEast;
}
document.getElementById('btn-save').addEventListener('click', save);
document.getElementById('btn-sync').addEventListener('click', syncNow);
document.getElementById('btn-browser').addEventListener('click', (e)=>{e.preventDefault();useBrowserTz();});
loadConfig();

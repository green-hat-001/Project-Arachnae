#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Teach Mode Web Server
//
//  Serves a mobile-friendly UI at http://<ESP32_IP>/teach
//  No app, no Python, no dependencies — just open in browser.
//
//  Endpoints:
//    GET  /teach          → serve the HTML UI page
//    GET  /teach/status   → JSON: live angles + saved flags
//    POST /teach/start    → disable torque (arm goes limp)
//    POST /teach/save     → body: {"pos":"home"} → save current position
//    POST /teach/preview  → body: {"pos":"home"} → move to saved position
//    POST /teach/done     → re-enable torque, exit teach mode
//    POST /teach/clear    → wipe all saved positions
// ============================================================

#include <WebServer.h>
#include "teach_mode.h"
#include "ArduinoJson.h"

static WebServer g_teachServer(80);

// ============================================================
//  HTML + CSS + JS for the teach UI
//  Stored in flash (PROGMEM) to save RAM
// ============================================================
static const char TEACH_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Arachnae — Teach Mode</title>
<style>
  :root {
    --bg:      #0f0f13;
    --surface: #1a1a24;
    --card:    #22222f;
    --border:  #2e2e40;
    --accent:  #00e5a0;
    --warn:    #ff6b35;
    --danger:  #e05252;
    --text:    #e8e8f0;
    --muted:   #6b6b80;
    --radius:  14px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    min-height: 100vh;
    padding: 16px;
  }
  header {
    text-align: center;
    padding: 20px 0 8px;
  }
  header h1 {
    font-size: 1.5rem;
    letter-spacing: 0.12em;
    color: var(--accent);
    text-transform: uppercase;
  }
  header p { color: var(--muted); font-size: 0.85rem; margin-top: 4px; }

  /* Live angle readout */
  #live-bar {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 8px;
    margin: 16px 0;
  }
  .angle-box {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 10px 6px;
    text-align: center;
  }
  .angle-box .label { font-size: 0.65rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; }
  .angle-box .value { font-size: 1.15rem; font-weight: 700; color: var(--accent); margin-top: 2px; }

  /* Status bar */
  #status-bar {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 12px 16px;
    margin-bottom: 16px;
    font-size: 0.85rem;
    display: flex;
    align-items: center;
    gap: 10px;
  }
  #status-dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: var(--muted);
    flex-shrink: 0;
    transition: background 0.3s;
  }
  #status-dot.active { background: var(--accent); box-shadow: 0 0 8px var(--accent); }
  #status-dot.danger { background: var(--danger); }

  /* Mode buttons */
  #mode-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-bottom: 20px;
  }

  /* Position cards */
  .pos-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 14px;
    margin-bottom: 10px;
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .pos-card.saved { border-color: var(--accent); }
  .pos-icon {
    font-size: 1.6rem;
    width: 40px;
    text-align: center;
    flex-shrink: 0;
  }
  .pos-info { flex: 1; }
  .pos-info h3 { font-size: 0.95rem; font-weight: 600; }
  .pos-info p  { font-size: 0.75rem; color: var(--muted); margin-top: 2px; }
  .pos-badge {
    font-size: 0.65rem;
    padding: 2px 8px;
    border-radius: 20px;
    background: var(--border);
    color: var(--muted);
    margin-top: 4px;
    display: inline-block;
  }
  .pos-badge.done { background: #00e5a020; color: var(--accent); }
  .pos-actions { display: flex; flex-direction: column; gap: 6px; }

  /* Buttons */
  button {
    border: none;
    border-radius: 10px;
    font-size: 0.85rem;
    font-weight: 600;
    cursor: pointer;
    padding: 10px 16px;
    transition: opacity 0.15s, transform 0.1s;
    letter-spacing: 0.03em;
  }
  button:active { transform: scale(0.97); }
  button:disabled { opacity: 0.35; cursor: not-allowed; transform: none; }
  .btn-primary   { background: var(--accent); color: #000; }
  .btn-secondary { background: var(--surface); color: var(--text); border: 1px solid var(--border); }
  .btn-warn      { background: var(--warn);   color: #000; }
  .btn-danger    { background: var(--danger); color: #fff; }
  .btn-full      { width: 100%; padding: 14px; font-size: 1rem; }
  .btn-sm        { padding: 7px 12px; font-size: 0.78rem; }

  /* Progress */
  #progress-bar-wrap {
    background: var(--border);
    border-radius: 4px;
    height: 6px;
    margin: 16px 0 20px;
    overflow: hidden;
  }
  #progress-bar {
    height: 100%;
    background: var(--accent);
    border-radius: 4px;
    transition: width 0.4s ease;
  }
  #progress-label {
    text-align: center;
    font-size: 0.75rem;
    color: var(--muted);
    margin-bottom: 16px;
  }

  /* Done screen */
  #done-screen {
    display: none;
    text-align: center;
    padding: 40px 20px;
  }
  #done-screen .big-tick { font-size: 4rem; }
  #done-screen h2 { font-size: 1.3rem; margin: 16px 0 8px; color: var(--accent); }
  #done-screen p  { color: var(--muted); font-size: 0.9rem; }

  section h2 {
    font-size: 0.75rem;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--muted);
    margin-bottom: 10px;
  }
  .divider { border: none; border-top: 1px solid var(--border); margin: 20px 0; }
  #toast {
    position: fixed;
    bottom: 24px; left: 50%;
    transform: translateX(-50%) translateY(80px);
    background: var(--accent);
    color: #000;
    font-weight: 700;
    padding: 12px 24px;
    border-radius: 30px;
    font-size: 0.9rem;
    transition: transform 0.3s ease;
    pointer-events: none;
    z-index: 100;
    white-space: nowrap;
  }
  #toast.show { transform: translateX(-50%) translateY(0); }
  #toast.error { background: var(--danger); color: #fff; }
</style>
</head>
<body>

<header>
  <h1>⚙ Arachnae</h1>
  <p>Teach Mode — physical position calibration</p>
</header>

<!-- Live angle readout -->
<div id="live-bar">
  <div class="angle-box"><div class="label">Base</div><div class="value" id="a-base">—</div></div>
  <div class="angle-box"><div class="label">Shoulder</div><div class="value" id="a-sh">—</div></div>
  <div class="angle-box"><div class="label">Elbow</div><div class="value" id="a-el">—</div></div>
  <div class="angle-box"><div class="label">Forearm</div><div class="value" id="a-fa">—</div></div>
</div>

<!-- Status -->
<div id="status-bar">
  <div id="status-dot"></div>
  <span id="status-text">Connecting to arm...</span>
</div>

<!-- Mode control -->
<div id="mode-row">
  <button class="btn-warn btn-full" id="btn-start" onclick="startTeach()">
    🖐 Enable Teach<br><small>Arm goes limp</small>
  </button>
  <button class="btn-secondary btn-full" id="btn-done" onclick="finishTeach()" disabled>
    ✅ Lock & Save<br><small>Re-enable torque</small>
  </button>
</div>

<!-- Progress -->
<div id="progress-bar-wrap"><div id="progress-bar" style="width:0%"></div></div>
<div id="progress-label">0 / 7 positions set</div>

<!-- Position cards -->
<section>
<h2>Positions to teach</h2>

<div class="pos-card" id="card-home">
  <div class="pos-icon">🏠</div>
  <div class="pos-info">
    <h3>Home</h3>
    <p>Safe tucked position — arm starts here on boot</p>
    <span class="pos-badge" id="badge-home">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('home')" id="save-home" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('home')" id="prev-home" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-fwd_reach">
  <div class="pos-icon">🎯</div>
  <div class="pos-info">
    <h3>Max Forward Reach</h3>
    <p>Furthest the arm can reach in front — IK won't exceed this</p>
    <span class="pos-badge" id="badge-fwd_reach">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('fwd_reach')" id="save-fwd_reach" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('fwd_reach')" id="prev-fwd_reach" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-handoff">
  <div class="pos-icon">🤝</div>
  <div class="pos-info">
    <h3>Handoff / Drop-off</h3>
    <p>Where the arm delivers objects to your right hand</p>
    <span class="pos-badge" id="badge-handoff">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('handoff')" id="save-handoff" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('handoff')" id="prev-handoff" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-left_limit">
  <div class="pos-icon">⬅️</div>
  <div class="pos-info">
    <h3>Left Sweep Limit</h3>
    <p>Furthest the base can rotate leftward (toward left shoulder)</p>
    <span class="pos-badge" id="badge-left_limit">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('left_limit')" id="save-left_limit" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('left_limit')" id="prev-left_limit" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-right_limit">
  <div class="pos-icon">➡️</div>
  <div class="pos-info">
    <h3>Right Sweep Limit</h3>
    <p>Furthest the base can rotate rightward (stays clear of neck)</p>
    <span class="pos-badge" id="badge-right_limit">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('right_limit')" id="save-right_limit" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('right_limit')" id="prev-right_limit" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-height_max">
  <div class="pos-icon">⬆️</div>
  <div class="pos-info">
    <h3>Max Height</h3>
    <p>Highest the arm can reach — prevents hitting your head</p>
    <span class="pos-badge" id="badge-height_max">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('height_max')" id="save-height_max" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('height_max')" id="prev-height_max" disabled>Go</button>
  </div>
</div>

<div class="pos-card" id="card-depth_max">
  <div class="pos-icon">⬇️</div>
  <div class="pos-info">
    <h3>Max Depth (Table reach)</h3>
    <p>Lowest the arm can reach — set at table / surface height</p>
    <span class="pos-badge" id="badge-depth_max">Not set</span>
  </div>
  <div class="pos-actions">
    <button class="btn-primary btn-sm" onclick="savePos('depth_max')" id="save-depth_max" disabled>Set</button>
    <button class="btn-secondary btn-sm" onclick="previewPos('depth_max')" id="prev-depth_max" disabled>Go</button>
  </div>
</div>

</section>

<hr class="divider">

<section>
<h2>Danger zone</h2>
<button class="btn-danger btn-full" onclick="clearAll()" style="margin-top:4px">
  🗑 Clear All Saved Positions
</button>
</section>

<!-- Done screen (shown after lock & save) -->
<div id="done-screen">
  <div class="big-tick">✅</div>
  <h2>Calibration Complete</h2>
  <p>All positions saved to flash.<br>The arm will use these on every boot.</p>
  <br>
  <button class="btn-primary btn-full" onclick="location.reload()">Back to Teach</button>
</div>

<div id="toast"></div>

<script>
const POSITIONS = ['home','fwd_reach','handoff','left_limit','right_limit','height_max','depth_max'];
let teachActive = false;
let savedSet = new Set();

// ── Poll status every 500ms ──────────────────────────────────
async function pollStatus() {
  try {
    const r = await fetch('/teach/status');
    const d = await r.json();

    // Update angle display
    document.getElementById('a-base').textContent = d.live.base_deg.toFixed(1) + '°';
    document.getElementById('a-sh').textContent   = d.live.sh_deg.toFixed(1)   + '°';
    document.getElementById('a-el').textContent   = d.live.el_deg.toFixed(1)   + '°';
    document.getElementById('a-fa').textContent   = d.live.fa_deg.toFixed(1)   + '°';

    teachActive = d.teach_active;
    const dot  = document.getElementById('status-dot');
    const stxt = document.getElementById('status-text');

    if (teachActive) {
      dot.className = 'active';
      stxt.textContent = '🟢 Teach active — arm is free to move';
    } else {
      dot.className = '';
      stxt.textContent = d.calibrated
        ? '🔒 Torque on — press Enable Teach to move arm'
        : '⚠️ Not yet calibrated — press Enable Teach to start';
    }

    // Update card badges and buttons
    savedSet = new Set();
    POSITIONS.forEach(pos => {
      const isSaved = d.saved[pos];
      if (isSaved) savedSet.add(pos);

      document.getElementById('card-' + pos).className =
        'pos-card' + (isSaved ? ' saved' : '');
      const badge = document.getElementById('badge-' + pos);
      badge.textContent  = isSaved ? '✓ Saved' : 'Not set';
      badge.className    = 'pos-badge' + (isSaved ? ' done' : '');

      document.getElementById('save-' + pos).disabled  = !teachActive;
      document.getElementById('prev-' + pos).disabled  = !isSaved || teachActive;
    });

    // Progress
    const count = savedSet.size;
    document.getElementById('progress-bar').style.width = (count / 7 * 100) + '%';
    document.getElementById('progress-label').textContent =
      count + ' / 7 positions set' + (count === 7 ? ' — ready to lock!' : '');

    document.getElementById('btn-done').disabled = !teachActive;
    document.getElementById('btn-start').disabled = teachActive;

  } catch(e) {
    document.getElementById('status-text').textContent = '⚠️ Cannot reach arm — check WiFi';
    document.getElementById('status-dot').className = 'danger';
  }
}

async function post(url, body = {}) {
  const r = await fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body)
  });
  return r.json();
}

async function startTeach() {
  document.getElementById('btn-start').disabled = true;
  const d = await post('/teach/start');
  toast(d.ok ? '🖐 Arm is free — move it to position' : '❌ ' + d.error, !d.ok);
}

async function savePos(pos) {
  const btn = document.getElementById('save-' + pos);
  btn.textContent = '...';
  btn.disabled = true;
  const d = await post('/teach/save', {pos});
  toast(d.ok ? '✅ ' + pos.replace('_',' ') + ' saved!' : '❌ ' + d.error, !d.ok);
  btn.textContent = 'Set';
}

async function previewPos(pos) {
  const d = await post('/teach/preview', {pos});
  toast(d.ok ? '➡️ Moving to ' + pos.replace('_',' ') : '❌ ' + d.error, !d.ok);
}

async function finishTeach() {
  if (savedSet.size < 7) {
    const missing = POSITIONS.filter(p => !savedSet.has(p)).map(p => p.replace('_',' ')).join(', ');
    if (!confirm('Not all positions set. Missing: ' + missing + '\n\nLock anyway?')) return;
  }
  const d = await post('/teach/done');
  if (d.ok) {
    document.getElementById('done-screen').style.display = 'block';
    document.querySelectorAll('section, #mode-row, #progress-bar-wrap, #progress-label, #live-bar').forEach(e => e.style.display = 'none');
  }
  toast(d.ok ? '🔒 Locked! Torque enabled.' : '❌ ' + d.error, !d.ok);
}

async function clearAll() {
  if (!confirm('Clear ALL saved positions? This cannot be undone.')) return;
  const d = await post('/teach/clear');
  toast(d.ok ? '🗑 Cleared' : '❌ Error', !d.ok);
}

function toast(msg, isError = false) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'show' + (isError ? ' error' : '');
  setTimeout(() => t.className = '', 2800);
}

// Start polling
pollStatus();
setInterval(pollStatus, 500);
</script>
</body>
</html>
)=====";

// ============================================================
//  HTTP route handlers
// ============================================================
void handleTeachPage() {
  g_teachServer.send_P(200, "text/html", TEACH_HTML);
}

void handleTeachStatus() {
  g_teachServer.send(200, "application/json", teachStatusJSON());
}

void handleTeachStart() {
  teachModeStart();
  g_teachServer.send(200, "application/json", "{\"ok\":true}");
}

void handleTeachSave() {
  if (!g_teachServer.hasArg("plain")) {
    g_teachServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }
  StaticJsonDocument<64> doc;
  deserializeJson(doc, g_teachServer.arg("plain"));
  const char* pos = doc["pos"] | "";
  bool ok = savePosition(pos);
  g_teachServer.send(200, "application/json",
    ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"unknown position\"}");
}

void handleTeachPreview() {
  if (!g_teachServer.hasArg("plain")) {
    g_teachServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
    return;
  }
  // Must re-enable torque temporarily to move
  uint8_t ids[] = {SERVO_BASE,SERVO_SHOULDER,SERVO_ELBOW,SERVO_FOREARM,SERVO_GRIPPER};
  for (uint8_t id : ids) { g_bus.writeByte(id, REG_TORQUE_ENABLE, 1); delay(5); }

  StaticJsonDocument<64> doc;
  deserializeJson(doc, g_teachServer.arg("plain"));
  const char* pos = doc["pos"] | "";
  moveToTaught(pos, APPROACH_SPEED);
  delay(200);

  // Disable torque again if teach mode is active
  if (g_teachActive) {
    for (uint8_t id : ids) { g_bus.writeByte(id, REG_TORQUE_ENABLE, 0); delay(5); }
  }
  g_teachServer.send(200, "application/json", "{\"ok\":true}");
}

void handleTeachDone() {
  teachModeEnd();
  g_teachServer.send(200, "application/json", "{\"ok\":true}");
}

void handleTeachClear() {
  clearAllTaught();
  g_teachServer.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================
//  Start the teach HTTP server (call from setup())
// ============================================================
void teachServerSetup() {
  loadTaughtPositions();

  g_teachServer.on("/teach",         HTTP_GET,  handleTeachPage);
  g_teachServer.on("/teach/status",  HTTP_GET,  handleTeachStatus);
  g_teachServer.on("/teach/start",   HTTP_POST, handleTeachStart);
  g_teachServer.on("/teach/save",    HTTP_POST, handleTeachSave);
  g_teachServer.on("/teach/preview", HTTP_POST, handleTeachPreview);
  g_teachServer.on("/teach/done",    HTTP_POST, handleTeachDone);
  g_teachServer.on("/teach/clear",   HTTP_POST, handleTeachClear);

  g_teachServer.begin();
  Serial.printf("[Teach] UI ready at http://%s/teach\n",
                WiFi.localIP().toString().c_str());
}

void teachServerLoop() {
  g_teachServer.handleClient();
}

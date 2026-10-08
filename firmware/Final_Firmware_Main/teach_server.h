#pragma once
// ============================================================
//  PROJET ARACHNAE v2 — Teach Mode Web Server
//
//  Mobile-friendly UI at http://<ESP32_IP>/teach  (port 80)
//
//    GET  /teach            HTML UI
//    GET  /teach/status     JSON: live angles, saved flags, armed, fault, joint cfg
//    POST /teach/arm        {"force":bool} confirm the arm booted in its reference pose
//    POST /teach/estop      cut all torque NOW (latches a fault)
//    POST /teach/fault_reset  clear fault; arm HOLDS where it is (does not move)
//    POST /teach/start      torque off (arm goes limp)
//    POST /teach/save       {"pos":"home"}   save current pose
//    POST /teach/preview    {"pos":"home"}   move to a saved pose (non-blocking)
//    POST /teach/jog        {"joint":"base","delta":2}  nudge one joint (powers it)
//    POST /teach/done       torque on, holding the current pose
//    POST /teach/scale      {"scale":0.1}    safety speed scale
//    POST /teach/demo       {"op":"add","grip":0-2,"pause":ms} | undo | clear | play | stop
//    POST /teach/cfg        {"joint":0,"ratio":50,"sign":1[,"backlash":0,"torque":350]} | {"revbit":1}
//                           | {"grip":"open"|"close"|"clear"}  current gripper pos -> open/closed
//    POST /teach/clear      wipe taught positions
//
//    GET  /test             bench-test UI (base spin)
//    GET  /test/status      JSON: base angle/load/temp/volt + spin state
//    POST /test/base/spin   {"dir":1|-1,"pct":10-100}  free-run the base until stopped (no angle limit);
//                           while running, same dir = change speed
//    POST /test/base/stop   stop a spin (holds where it lands)
//    POST /test/cam         {"brightness":-2..2,"contrast","saturation","ae_level","wb_mode":0-4,"hmirror","vflip"}
//    POST /test/cv/approach move to the approach point above the last CV detection (needs 7 taught poses)
//    POST /test/hold        abort any move, hold where it is
//    POST /test/cv/capture  {"secs":3-120}  ask the laptop capture agent to record the CV preview
//  WebSocket {"type":"cv_obs",...} from the CV pipeline feeds the /test CV panel; never moves the arm.
//  WebSocket agent_hello / cv_capture_status from Final_Tools/cv_capture_agent.py; never moves the arm.
// ============================================================

#include <WebServer.h>
#include "teach_mode.h"
#include "ArduinoJson.h"

static WebServer g_teachServer(80);

// set by the sketch: resets FSM / auto-mode after a fault reset
static void (*g_onFaultReset)() = nullptr;

// ============================================================
//  HTML + CSS + JS for the teach UI (PROGMEM)
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

  /* Jog / fine-adjust controls */
  #jog-step-row {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 12px;
    font-size: 0.8rem;
    color: var(--muted);
  }
  #jog-step-row select {
    background: var(--card);
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 6px 10px;
    font-size: 0.85rem;
  }
  .jog-row {
    display: flex;
    align-items: center;
    gap: 10px;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 10px 12px;
    margin-bottom: 8px;
  }
  .jog-row .jog-label { flex: 1; font-size: 0.85rem; font-weight: 600; }
  .jog-btn {
    width: 44px;
    padding: 8px 0;
    font-size: 1.1rem;
    background: var(--surface);
    color: var(--text);
    border: 1px solid var(--border);
  }

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

  /* --- v3 additions --- */
  #estop-bar { position: sticky; top: 0; z-index: 50; padding: 6px 0 10px; background: linear-gradient(var(--bg) 80%, transparent); }
  #btn-estop { width: 100%; padding: 16px; font-size: 1.1rem; letter-spacing: 0.15em; background: #ff1f1f; color: #fff; border: 2px solid #fff3; }
  .banner { border-radius: var(--radius); padding: 14px 16px; margin-bottom: 14px; font-size: 0.9rem; display: none; }
  #fault-banner { background: #3a1214; border: 1px solid var(--danger); }
  #arm-banner   { background: #2c2410; border: 1px solid var(--warn); }
  .banner h3 { font-size: 1rem; margin-bottom: 6px; }
  .banner p  { color: var(--muted); margin-bottom: 10px; }
  .banner button { width: 100%; }
  #sig-text { color: var(--warn); font-weight: 600; margin-bottom: 10px; }
  details.card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 12px 14px; margin-bottom: 12px; }
  details.card summary { cursor: pointer; font-weight: 600; font-size: 0.9rem; }
  .cfg-row { display: flex; align-items: center; gap: 8px; margin-top: 10px; font-size: 0.85rem; }
  .cfg-row .cfg-name { width: 70px; font-weight: 600; }
  #cfg-rows .cfg-row { flex-wrap: wrap; row-gap: 6px; }
  .cfg-row .cfg-lbl { color: var(--muted); font-size: 0.75rem; }
  .cfg-row input, .cfg-row select { background: var(--surface); color: var(--text); border: 1px solid var(--border); border-radius: 8px; padding: 8px; width: 80px; }
  .hint { color: var(--muted); font-size: 0.75rem; margin-top: 8px; line-height: 1.4; }
  #scale-row { display:flex; align-items:center; gap:10px; margin-bottom: 14px; font-size:0.85rem; }
  #scale-row select { background: var(--surface); color: var(--text); border: 1px solid var(--border); border-radius: 8px; padding: 8px; }
</style>
</head>
<body>

<div id="estop-bar"><button id="btn-estop" onclick="estop()">🛑 E-STOP</button></div>

<div id="fault-banner" class="banner">
  <h3>🛑 FAULT — all torque is OFF</h3>
  <p id="fault-text"></p>
  <button class="btn-warn" onclick="resetFault()">Reset fault (arm will HOLD where it is)</button>
</div>

<div id="arm-banner" class="banner">
  <h3>⚠️ Not armed — no motion allowed</h3>
  <p>Confirm the arm was resting in its <b>boot reference pose</b> when it was powered on.
     Every angle and every taught position is measured from that pose.</p>
  <div id="sig-text"></div>
  <button class="btn-primary" onclick="armNow()">✅ Yes — arm was in its boot pose. Arm.</button>
</div>

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
    🖐 Free the arm<br><small>All joints go limp</small>
  </button>
  <button class="btn-secondary btn-full" id="btn-done" onclick="finishTeach()" disabled>
    ✅ Lock & Save<br><small>Re-enable torque</small>
  </button>
</div>

<!-- Progress -->
<div id="progress-bar-wrap"><div id="progress-bar" style="width:0%"></div></div>
<div id="progress-label">0 / 7 positions set</div>

<!-- Fine adjust / jog — for belts that slip instead of backdriving,
     where hand-teaching can't move the servo directly. Works whether
     or not Enable Teach is active. -->
<section>
<h2>Fine Adjust</h2>
<div id="jog-step-row">
  Step:
  <select id="jog-step">
    <option value="0.5">0.5°</option>
    <option value="1" selected>1°</option>
    <option value="2">2°</option>
    <option value="5">5°</option>
  </select>
</div>
<div class="jog-row">
  <span class="jog-label">Base</span>
  <button class="jog-btn" onclick="jog('base',-1)">−</button>
  <button class="jog-btn" onclick="jog('base',1)">+</button>
</div>
<div class="jog-row">
  <span class="jog-label">Shoulder</span>
  <button class="jog-btn" onclick="jog('shoulder',-1)">−</button>
  <button class="jog-btn" onclick="jog('shoulder',1)">+</button>
</div>
<div class="jog-row">
  <span class="jog-label">Elbow</span>
  <button class="jog-btn" onclick="jog('elbow',-1)">−</button>
  <button class="jog-btn" onclick="jog('elbow',1)">+</button>
</div>
<div class="jog-row">
  <span class="jog-label">Forearm</span>
  <button class="jog-btn" onclick="jog('forearm',-1)">−</button>
  <button class="jog-btn" onclick="jog('forearm',1)">+</button>
</div>
<div class="jog-row">
  <span class="jog-label">Gripper</span>
  <button class="jog-btn" onclick="jog('gripper',-1)">−</button>
  <button class="jog-btn" onclick="jog('gripper',1)">+</button>
</div>
<div class="cfg-row"><span class="cfg-name">Gripper</span>
  <span id="grip-txt">open —, closed —</span>
  <button class="btn-secondary btn-sm" onclick="setGrip('open')">Set open</button>
  <button class="btn-secondary btn-sm" onclick="setGrip('close')">Set closed</button></div>
<p class="hint">Jog the gripper fully open and press <b>Set open</b>, then gently closed (jaws just touching) and
press <b>Set closed</b>. Jogging then stays between the two, and auto mode needs both.</p>
</section>

<hr class="divider">

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
<h2>Demo sequence</h2>
<p class="hint" style="margin-top:0">A grab scripted from poses: jog to a pose, choose what the gripper does
there, <b>Add step</b>. <b>Play</b> runs the steps in order at the speed scale below. Stop, E-STOP or any
fault ends it and the arm holds. Re-zeroing wipes the steps.</p>
<div class="cfg-row" style="flex-wrap:wrap;row-gap:6px">
  <span class="cfg-lbl">then gripper</span>
  <select id="demo-grip"><option value="0">keep</option><option value="1">open</option><option value="2">close</option></select>
  <span class="cfg-lbl">pause ms</span>
  <input id="demo-pause" type="number" min="0" max="10000" step="250" value="500">
  <button class="btn-primary btn-sm" onclick="demoOp('add')">Add step</button>
</div>
<ol id="demo-list" style="margin:12px 0 0 22px;font-size:0.82rem;line-height:1.6;font-variant-numeric:tabular-nums"></ol>
<div id="demo-state" class="hint" style="margin-top:6px">—</div>
<div class="cfg-row">
  <button class="btn-primary btn-sm" id="demo-play" onclick="demoOp('play')">▶ Play</button>
  <button class="btn-secondary btn-sm" onclick="demoOp('stop')">■ Stop</button>
  <button class="btn-secondary btn-sm" onclick="demoOp('undo')">Remove last</button>
  <button class="btn-secondary btn-sm" onclick="if (confirm('Delete all demo steps?')) demoOp('clear')">Clear</button>
</div>
</section>

<hr class="divider">

<section>
<h2>Speed &amp; joint calibration</h2>
<div id="scale-row">
  Safety speed scale:
  <select id="scale-sel" onchange="setScale(this.value)">
    <option value="0.05">5%</option><option value="0.1">10%</option><option value="0.15">15%</option>
    <option value="0.25">25%</option><option value="0.35">35%</option><option value="0.5">50%</option>
  </select>
</div>
<div class="cfg-row" style="margin-bottom:14px">
  <span class="cfg-name">Reference pose</span>
  <button class="btn-secondary btn-sm" onclick="setZero()">Set this position as 0</button>
</div>
<p class="hint">Jog the arm into the HOME pose, then press this. Worn, HOME is the arm hanging
behind you: <b>not swung left or right</b> (base 0), <b>link 1 tilted 30° back</b> from vertical
(shoulder 30), <b>the forearm link and wrist plumb</b> below it (elbow −30, wrist 0). Check with a
phone inclinometer on each link. Every joint will then read its HOME angle and all positions are
measured from here — no power-cycling and no moving the arm by hand. <b>This wipes any taught
positions</b>, since those are stored as steps from the old reference.</p>

<details class="card">
  <summary>Joint calibration (ratio / direction / backlash / torque)</summary>
  <div id="cfg-rows"></div>
  <div class="cfg-row"><span class="cfg-name">Wheel bit</span>
    <span id="revbit-txt">—</span>
    <button class="btn-secondary btn-sm" onclick="flipRevBit()">Flip</button></div>
  <p class="hint">Jog a joint with + : if it moves the wrong way physically, flip its direction here.
  If a big move aborts with "WRONG WAY", flip the wheel bit. Ratios are servo revs per output rev.
  Changing them does NOT invalidate taught positions. <b>Backlash</b> is the gear slack in servo steps,
  taken up on every reversal (base worm stage ~70). <b>Torque</b> is the servo's torque limit, 100-1000
  (= 10-100 %); it also caps speed. The base defaults to 350 to spare the printed worm gear; raise it
  only if the base stalls lifting the arm. <b>Base:</b> finish each base jog moving in the
  + direction so taught positions are taken on the same side of the gear slack.</p>
</details>
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
let teachActive = false, armed = false, faulted = false, busy = false;
let savedSet = new Set();
let cfgBuilt = false;
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function pollStatus() {
  try {
    const r = await fetch('/teach/status');
    const d = await r.json();

    document.getElementById('a-base').textContent = d.live.base_deg.toFixed(1) + '°';
    document.getElementById('a-sh').textContent   = d.live.sh_deg.toFixed(1)   + '°';
    document.getElementById('a-el').textContent   = d.live.el_deg.toFixed(1)   + '°';
    document.getElementById('a-fa').textContent   = d.live.fa_deg.toFixed(1)   + '°';

    teachActive = d.teach_active; armed = d.armed; faulted = d.fault; busy = d.busy;

    document.getElementById('fault-banner').style.display = faulted ? 'block' : 'none';
    document.getElementById('fault-text').textContent = d.fault_reason;
    document.getElementById('arm-banner').style.display = (!armed && !faulted) ? 'block' : 'none';
    document.getElementById('sig-text').textContent = d.sig_warn || '';

    const dot  = document.getElementById('status-dot');
    const stxt = document.getElementById('status-text');
    if (faulted)            { dot.className = 'danger'; stxt.textContent = '🛑 FAULT — torque cut'; }
    else if (!armed)        { dot.className = 'danger'; stxt.textContent = '⚠️ Not armed — confirm boot pose above'; }
    else if (teachActive)   { dot.className = 'active'; stxt.textContent = '🟢 Teach active — unpowered joints are free'; }
    else                    { dot.className = '';
      stxt.textContent = d.calibrated ? '🔒 Torque on — calibrated' : '🔒 Torque on — teach the remaining positions'; }
    if (d.move_msg) stxt.textContent += '  ⚠ ' + d.move_msg;
    if (busy && !faulted) stxt.textContent += '  (moving…)';

    renderDemo(d.demo, armed && !faulted && !teachActive);

    savedSet = new Set();
    const canMove = armed && !faulted;
    POSITIONS.forEach(pos => {
      const isSaved = d.saved[pos];
      if (isSaved) savedSet.add(pos);
      document.getElementById('card-' + pos).className = 'pos-card' + (isSaved ? ' saved' : '');
      const badge = document.getElementById('badge-' + pos);
      badge.textContent = isSaved ? '✓ Saved' : 'Not set';
      badge.className   = 'pos-badge' + (isSaved ? ' done' : '');
      document.getElementById('save-' + pos).disabled = !canMove;
      document.getElementById('prev-' + pos).disabled = !isSaved || teachActive || !canMove || busy;
    });
    document.querySelectorAll('.jog-btn').forEach(b => b.disabled = !canMove);

    const count = savedSet.size;
    document.getElementById('progress-bar').style.width = (count / 7 * 100) + '%';
    document.getElementById('progress-label').textContent =
      count + ' / 7 positions set' + (count === 7 ? ' — calibrated!' : '');

    document.getElementById('btn-done').disabled  = !teachActive || faulted;
    document.getElementById('btn-start').disabled = faulted || !armed;

    const sel = document.getElementById('scale-sel');
    if (document.activeElement !== sel) {
      let best = 0, bd = 9;
      [...sel.options].forEach((o, i) => { const dd = Math.abs(parseFloat(o.value) - d.scale); if (dd < bd) { bd = dd; best = i; } });
      sel.selectedIndex = best;
    }
    document.getElementById('revbit-txt').textContent = '0x' + d.revbit.toString(16).toUpperCase().padStart(4,'0');
    document.getElementById('grip-txt').textContent =
      'open ' + (d.grip_open < 0 ? '—' : d.grip_open) + ', closed ' + (d.grip_close < 0 ? '—' : d.grip_close);

    if (!cfgBuilt) {
      cfgBuilt = true;
      const box = document.getElementById('cfg-rows');
      d.joints.forEach((j, i) => {
        const row = document.createElement('div');
        row.className = 'cfg-row';
        row.innerHTML = '<span class="cfg-name">' + j.name + '</span>' +
          '<input id="cfg-ratio-' + i + '" type="number" step="0.001" value="' + j.ratio + '">' +
          '<select id="cfg-sign-' + i + '"><option value="1"' + (j.sign > 0 ? ' selected' : '') + '>+ dir</option>' +
          '<option value="-1"' + (j.sign < 0 ? ' selected' : '') + '>− dir</option></select>' +
          '<span class="cfg-lbl">backlash</span><input id="cfg-bl-' + i + '" type="number" step="1" min="0" value="' + j.backlash + '">' +
          '<span class="cfg-lbl">torque</span><input id="cfg-tq-' + i + '" type="number" step="10" min="100" max="1000" value="' + j.torque + '">' +
          '<button class="btn-secondary btn-sm" onclick="saveCfg(' + i + ')">Set</button>' +
          (j.ok ? '' : ' <span style="color:var(--danger)">no reply</span>');
        box.appendChild(row);
      });
    }
  } catch (e) {
    document.getElementById('status-text').textContent = '⚠️ Cannot reach arm — check WiFi';
    document.getElementById('status-dot').className = 'danger';
  }
}

async function post(url, body = {}) {
  const r = await fetch(url, { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body) });
  return r.json();
}

async function estop() {
  try { await post('/teach/estop'); } catch (e) {}
  toast('🛑 E-STOP sent', true);
}
async function resetFault() {
  const d = await post('/teach/fault_reset');
  toast(d.ok ? '✅ Fault cleared — holding position' : '❌ ' + d.error, !d.ok);
}
async function armNow(force = false) {
  const d = await post('/teach/arm', {force});
  if (d.ok) { toast('✅ Armed'); return; }
  if (d.error && d.error.startsWith('SIGNATURE:')) {
    if (confirm(d.error.substring(10) + '\n\nIf the arm really is in the same boot pose, arm anyway. Otherwise power-cycle it in the correct pose.\n\nArm anyway?'))
      return armNow(true);
    return;
  }
  toast('❌ ' + d.error, true);
}
async function setZero() {
  let d = await post('/teach/setzero', {});
  if (!d.ok && d.error && d.error.startsWith('SAVED:')) {
    if (!confirm(d.error.slice(6) + '.\n\nWipe them and set this pose as 0?')) return;
    d = await post('/teach/setzero', {force: true});
  }
  toast(d.ok ? '✅ This pose is now 0' : '❌ ' + d.error, !d.ok);
}
async function setScale(v) {
  const d = await post('/teach/scale', {scale: parseFloat(v)});
  toast(d.ok ? '⚙ Speed scale ' + Math.round(parseFloat(v) * 100) + '%' : '❌ ' + d.error, !d.ok);
}
async function saveCfg(i) {
  const ratio = parseFloat(document.getElementById('cfg-ratio-' + i).value);
  const sign  = parseInt(document.getElementById('cfg-sign-' + i).value);
  const backlash = parseInt(document.getElementById('cfg-bl-' + i).value);
  const torque   = parseInt(document.getElementById('cfg-tq-' + i).value);
  const d = await post('/teach/cfg', {joint: i, ratio, sign, backlash, torque});
  toast(d.ok ? '✅ Joint saved' : '❌ ' + d.error, !d.ok);
}
const GRIP_TXT = ['', ', then open', ', then close'];
function renderDemo(m, canPlay) {
  const list = document.getElementById('demo-list');
  list.innerHTML = m.steps.map((st, i) =>
    '<li' + (m.running && i === m.step ? ' style="color:var(--accent);font-weight:700"' : '') + '>' +
    'base ' + st[2].toFixed(0) + '°, sh ' + st[3].toFixed(0) + '°, el ' + st[4].toFixed(0) + '°, fa ' + st[5].toFixed(0) + '°' +
    GRIP_TXT[st[0]] + (st[1] ? ', wait ' + (st[1] / 1000).toFixed(1) + ' s' : '') + '</li>').join('');
  document.getElementById('demo-state').textContent = m.running
    ? 'Playing step ' + (m.step + 1) + ' of ' + m.n + ' (' + m.phase + ')'
    : (m.n + ' of ' + m.max + ' steps' + (m.msg ? ' · last run: ' + m.msg : ''));
  document.getElementById('demo-play').disabled = m.running || !canPlay || !m.n;
}
async function demoOp(op) {
  const body = { op };
  if (op === 'add') { body.grip = +document.getElementById('demo-grip').value; body.pause = +document.getElementById('demo-pause').value; }
  if (op === 'play' && !confirm('Play the demo sequence?\n\nSomeone must be at the 12 V cutoff.')) return;
  const d = await post('/teach/demo', body);
  toast(d.ok ? '✅ ' + ({add: 'Step added', undo: 'Last step removed', clear: 'Demo cleared', play: '▶ Playing', stop: '■ Stopped'})[op]
             : '❌ ' + d.error, !d.ok);
}
async function setGrip(which) {
  const d = await post('/teach/cfg', {grip: which});
  toast(d.ok ? '✅ Gripper ' + (which === 'open' ? 'open' : 'closed') + ' position saved' : '❌ ' + d.error, !d.ok);
}
async function flipRevBit() {
  const d = await post('/teach/cfg', {revbit: 1});
  toast(d.ok ? '✅ Wheel bit flipped' : '❌ ' + d.error, !d.ok);
}

async function startTeach() {
  if (!confirm('This frees ALL joints — the arm will DROP.\n\nSupport it with your other hand, then press OK. Torque goes off after a 3-second countdown.')) return;
  for (let i = 3; i > 0; i--) { toast('Releasing in ' + i + '…'); await sleep(1000); }
  const d = await post('/teach/start');
  toast(d.ok ? '🖐 Arm is free — move it to position' : '❌ ' + d.error, !d.ok);
}

async function savePos(pos) {
  const btn = document.getElementById('save-' + pos);
  btn.textContent = '...'; btn.disabled = true;
  const d = await post('/teach/save', {pos});
  toast(d.ok ? '✅ ' + pos.replace('_',' ') + ' saved!' : '❌ ' + d.error, !d.ok);
  btn.textContent = 'Set';
}

async function jog(joint, dir) {
  const step = parseFloat(document.getElementById('jog-step').value);
  const d = await post('/teach/jog', {joint, delta: step * dir});
  if (!d.ok) toast('❌ ' + d.error, true);
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
  if (d.ok && savedSet.size === 7) {
    document.getElementById('done-screen').style.display = 'block';
    document.querySelectorAll('section, #mode-row, #progress-bar-wrap, #progress-label, #live-bar').forEach(e => e.style.display = 'none');
  }
  toast(d.ok ? '🔒 Locked — holding position' : '❌ ' + d.error, !d.ok);
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

pollStatus();
setInterval(pollStatus, 500);
</script>
</body>
</html>
)=====";

// ============================================================
//  HTTP route handlers
// ============================================================
static void sendJson(int code, const String& s) { g_teachServer.send(code, "application/json", s); }
static void sendOk()                              { sendJson(200, "{\"ok\":true}"); }
static void sendErr(const String& e)              { sendJson(200, "{\"ok\":false,\"error\":\"" + jsonEsc(e) + "\"}"); }

static bool readBody(JsonDocument& doc) {
  if (!g_teachServer.hasArg("plain")) { sendJson(400, "{\"ok\":false,\"error\":\"no body\"}"); return false; }
  if (deserializeJson(doc, g_teachServer.arg("plain"))) { sendErr("bad json"); return false; }
  return true;
}

void handleTeachPage()   { g_teachServer.send_P(200, "text/html", TEACH_HTML); }

// ============================================================
//  Tests page (/test) — bench tests that don't belong in the teach flow.
//  Base spin: the base run as a free motor (wheel mode) at a chosen 10-100 %,
//  on until switched off. No angle limit.
// ============================================================
static const char TEST_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Arachnae Tests</title>
<style>
  :root { --bg:#0f0f13; --surface:#1a1a24; --card:#22222f; --border:#2e2e40; --accent:#00e5a0;
          --warn:#ff6b35; --danger:#e05252; --text:#e8e8f0; --muted:#6b6b80; --radius:14px; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
         min-height: 100vh; padding: 16px; max-width: 560px; margin: 0 auto; }
  #estop-bar { position: sticky; top: 0; z-index: 50; padding: 6px 0 10px; background: linear-gradient(var(--bg) 80%, transparent); }
  #btn-estop { width: 100%; padding: 16px; font-size: 1.1rem; letter-spacing: 0.15em; background: #ff1f1f; color: #fff; border: 2px solid #fff3; }
  button { border: none; border-radius: 10px; font-size: 0.85rem; font-weight: 600; cursor: pointer; padding: 10px 16px; }
  button:disabled { opacity: 0.35; cursor: not-allowed; }
  header { display: flex; align-items: baseline; justify-content: space-between; padding: 14px 0 10px; }
  header h1 { font-size: 1.3rem; letter-spacing: 0.12em; color: var(--accent); text-transform: uppercase; }
  header a { color: var(--muted); font-size: 0.85rem; text-decoration: none; }
  .banner { border-radius: var(--radius); padding: 12px 14px; margin-bottom: 12px; font-size: 0.88rem; display: none; }
  .banner p { color: var(--muted); margin: 4px 0 10px; }
  .banner a { color: var(--accent); }
  #fault-banner { background: #3a1214; border: 1px solid var(--danger); }
  #fault-banner button { width: 100%; background: var(--warn); color: #000; }
  #info-banner { background: #2c2410; border: 1px solid var(--warn); }
  .card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 16px; margin-bottom: 14px; }
  .card h2 { font-size: 1rem; margin-bottom: 2px; }
  .sub { color: var(--muted); font-size: 0.78rem; line-height: 1.4; margin-bottom: 14px; }
  .angle { font-size: 2.2rem; font-weight: 700; color: var(--accent); font-variant-numeric: tabular-nums; }
  .ends { display: flex; justify-content: space-between; color: var(--muted); font-size: 0.72rem; font-variant-numeric: tabular-nums; }
  .row { margin-top: 16px; }
  .row label { display: block; font-size: 0.75rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; margin-bottom: 8px; }
  input[type=range] { width: 100%; accent-color: var(--accent); height: 28px; }
  .speed-txt { font-size: 0.85rem; margin-top: 4px; font-variant-numeric: tabular-nums; }
  .speed-txt b { color: var(--accent); }
  .seg { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .seg button { background: var(--surface); color: var(--text); border: 1px solid var(--border); padding: 12px 8px; }
  .seg button.on { border-color: var(--accent); color: var(--accent); background: #00e5a014; }
  .switch-row { display: flex; align-items: center; gap: 14px; margin-top: 20px; }
  .switch { position: relative; width: 96px; height: 52px; flex-shrink: 0; border-radius: 26px; background: var(--border);
            border: none; padding: 0; transition: background 0.2s; }
  .switch::after { content: ""; position: absolute; top: 5px; left: 5px; width: 42px; height: 42px; border-radius: 50%;
                   background: var(--text); transition: left 0.2s; }
  .switch.on { background: var(--accent); }
  .switch.on::after { left: 49px; background: #000; }
  .switch.stopping { background: var(--warn); }
  .switch.stopping::after { left: 27px; }
  #state { font-size: 1.05rem; font-weight: 700; }
  #state-sub { color: var(--muted); font-size: 0.78rem; margin-top: 2px; }
  .stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-top: 16px; }
  .stat { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 8px; text-align: center; }
  .stat .k { font-size: 0.65rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; }
  .stat .v { font-size: 1.05rem; font-weight: 700; margin-top: 2px; font-variant-numeric: tabular-nums; }
  .v.hot { color: var(--warn); }
  .hint { color: var(--muted); font-size: 0.75rem; line-height: 1.45; margin-top: 14px; }
  #toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%) translateY(90px); background: var(--accent);
           color: #000; font-weight: 700; padding: 12px 22px; border-radius: 30px; font-size: 0.88rem; transition: transform 0.3s;
           pointer-events: none; z-index: 100; max-width: 92vw; }
  #toast.show { transform: translateX(-50%) translateY(0); }
  #toast.error { background: var(--danger); color: #fff; }
</style>
</head>
<body>

<div id="estop-bar"><button id="btn-estop" onclick="estop()">🛑 E-STOP</button></div>

<header><h1>Tests</h1><a href="/">&larr; Suite</a></header>

<div id="fault-banner" class="banner">
  <b>🛑 FAULT — all torque is OFF</b>
  <p id="fault-text"></p>
  <button onclick="resetFault()">Reset fault (arm will HOLD where it is)</button>
</div>
<div id="info-banner" class="banner"><b id="info-title"></b><p id="info-text"></p></div>

<div class="card">
  <h2>Base spin</h2>
  <div class="sub">Runs the base like a motor until you switch it off. <b>No angle limit</b>: it keeps turning
    whatever the base is doing. Keep the arm clear of people, and keep someone at the 12&nbsp;V cutoff.</div>

  <div class="angle" id="turns">—</div>
  <div class="ends"><span id="turns-sub">servo turns this spin</span><span id="base-turns">—</span></div>

  <div class="row">
    <label for="pct">Speed</label>
    <input type="range" id="pct" min="10" max="100" step="10" value="10" oninput="showSpeed()" onchange="speedChange()">
    <div class="speed-txt" id="speed-txt">—</div>
  </div>

  <div class="row">
    <label>Direction</label>
    <div class="seg">
      <button id="dir-neg" onclick="setDir(-1)">− (as base − jog)</button>
      <button id="dir-pos" class="on" onclick="setDir(1)">+ (as base + jog)</button>
    </div>
  </div>

  <div class="switch-row">
    <button class="switch" id="sw" onclick="toggle()" aria-label="Base spin on/off"></button>
    <div><div id="state">Off</div><div id="state-sub">&nbsp;</div></div>
  </div>

  <div class="stats">
    <div class="stat"><div class="k">Load</div><div class="v" id="load">—</div></div>
    <div class="stat"><div class="k">Temp</div><div class="v" id="temp">—</div></div>
    <div class="stat"><div class="k">Supply</div><div class="v" id="volt">—</div></div>
  </div>

  <p class="hint">Speed is a share of the servo's top wheel speed. It ignores the teach page's speed scale, and
    it can be changed while spinning; to change direction, stop first. Base turns are worked out from the base
    ratio on the Teach page. The base's torque limit (Teach page, default 350) also caps its top speed, so
    high percentages may run slower than asked. Still active while spinning: E-STOP, the stall watchdog (no
    motion for 0.5&nbsp;s holds), and temperature above 65&nbsp;°C for about 2&nbsp;s trips the E-STOP.
    After a spin the base angle reading is meaningless: re-zero it on the Teach page before jogging the base.</p>
</div>

<div class="card">
  <h2>Computer vision</h2>
  <div class="sub">Run <code>cv_pipeline_visual_servo.py --preview</code> on the laptop (dry run). This panel shows what
    it sends and what the arm would do with it. Nothing here moves the arm except <b>Approach only</b>.</div>

  <div id="cv-state" style="font-weight:700">Waiting for the pipeline…</div>
  <div class="stats">
    <div class="stat"><div class="k">Detected</div><div class="v" id="cv-rate">—</div></div>
    <div class="stat"><div class="k">Confidence</div><div class="v" id="cv-conf">—</div></div>
    <div class="stat"><div class="k">Fill</div><div class="v" id="cv-fill">—</div></div>
    <div class="stat"><div class="k">Depth est.</div><div class="v" id="cv-depth">—</div></div>
    <div class="stat"><div class="k">Pipeline</div><div class="v" id="cv-fps">—</div></div>
    <div class="stat"><div class="k">Box (px)</div><div class="v" id="cv-box" style="font-size:0.8rem">—</div></div>
  </div>

  <div class="row"><label>Target, camera/tool frame (mm)</label>
    <div class="speed-txt" id="cv-tip">—</div></div>
  <div class="row"><label>Target, body frame (mm, from the base mount)</label>
    <div class="speed-txt" id="cv-body">—</div></div>
  <div class="row"><label>Target, arm base frame (mm)</label>
    <div class="speed-txt" id="cv-base">—</div></div>
  <div class="row"><label>What the grab would do</label>
    <div class="speed-txt" id="cv-ik">—</div>
    <div class="speed-txt" id="cv-appr" style="margin-top:6px">—</div></div>

  <div class="row"><label>Measurements</label>
    <div class="seg">
      <button onclick="cvRecord()" id="cv-rec">Record (3 s average)</button>
      <button onclick="cvClear()">Clear</button>
    </div>
    <input id="cv-note" placeholder="note, e.g. 'tape 300 mm, 100 left'" style="width:100%;margin-top:8px;padding:9px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text)">
    <textarea id="cv-log" readonly rows="6" style="width:100%;margin-top:8px;padding:9px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font:0.72rem ui-monospace,monospace"></textarea>
    <p class="hint">Each record averages 3 s of detections. Select all of the text above and paste it into the calibration notes.</p>
  </div>

  <div class="row"><label>Proof video (records the laptop's OpenCV preview)</label>
    <div style="display:flex;gap:8px">
      <button onclick="cvCapture()" id="cap-go" style="flex:1">CV PROOF CAPTURE</button>
      <input id="cap-secs" type="number" min="3" max="120" value="20" title="seconds"
        style="width:76px;padding:9px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text)">
    </div>
    <div class="speed-txt" id="cap-state">—</div>
    <p class="hint">Needs <code>cvagent</code> running on the laptop. It opens the preview if it isn't open, records only
      that window for the chosen seconds (3–120) and saves it as <code>Pictures/opencv_proof.mp4</code>. It never moves the arm.</p>
  </div>

  <div class="row"><label>Camera image (saved on the arm)</label>
    <div id="cam-rows"></div>
  </div>

  <div class="row"><label>Reach test (after the 7 positions are taught)</label>
    <div class="seg">
      <button onclick="cvApproach()" id="cv-go" disabled>Approach only</button>
      <button onclick="post('/test/hold').then(()=>toast('Holding'))">Hold</button>
    </div>
    <button onclick="goHome()" style="width:100%;margin-top:8px;background:var(--surface);color:var(--text);border:1px solid var(--border)">Go to taught Home</button>
    <p class="hint"><b>Approach only</b> moves the arm to the point <span id="cv-clr">80</span>&nbsp;mm above the
      detected bottle and stops. It doesn't lower or grip, and it doesn't follow the bottle if it moves. It uses the
      speed scale from the Teach page. Someone must be at the 12&nbsp;V cutoff.</p>
  </div>
</div>

<div id="toast"></div>

<script>
let dir = 1, S = null, pending = false;

function el(id) { return document.getElementById(id); }
function toast(msg, err) {
  const t = el('toast'); t.textContent = msg; t.className = 'show' + (err ? ' error' : '');
  clearTimeout(toast.h); toast.h = setTimeout(() => t.className = '', 3000);
}
async function post(url, body) {
  const r = await fetch(url, { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body || {}) });
  return r.json();
}
function outDps(steps) { return S ? steps * 360 / (4096 * S.base.ratio) : 0; }

function showSpeed() {
  if (!S) return;
  const pct = +el('pct').value;
  const steps = Math.min(Math.floor(3400 * pct / 100), S.base.max_speed);
  el('speed-txt').innerHTML = '<b>' + pct + '%</b> &nbsp;·&nbsp; ' + steps + ' steps/s at the servo (' +
    (steps * 60 / 4096).toFixed(0) + ' rpm) &nbsp;·&nbsp; ≈ ' + outDps(steps).toFixed(1) + '°/s at the base';
}
async function speedChange() {
  if (!S || !S.spin.active || S.spin.stopping || pending) return;
  pending = true;
  try {
    const d = await post('/test/base/spin', { dir: S.spin.dir, pct: +el('pct').value });
    if (!d.ok) toast('❌ ' + d.error, true);
  } catch (e) { toast('❌ No reply from the arm', true); }
  pending = false;
  poll();
}
function setDir(d) {
  if (S && S.spin.active) return;
  dir = d;
  el('dir-pos').className = d > 0 ? 'on' : ''; el('dir-neg').className = d < 0 ? 'on' : '';
  showSpeed();
}

async function toggle() {
  if (pending || !S) return;
  pending = true;
  try {
    if (S.spin.active) {
      await post('/test/base/stop');
    } else {
      const d = await post('/test/base/spin', { dir, pct: +el('pct').value });
      if (!d.ok) toast('❌ ' + d.error, true);
    }
  } catch (e) { toast('❌ No reply from the arm', true); }
  pending = false;
  poll();
}
async function estop() { try { await post('/teach/estop'); } catch (e) {} toast('🛑 E-STOP sent', true); }
async function resetFault() {
  const d = await post('/teach/fault_reset');
  toast(d.ok ? '✅ Fault cleared — holding position' : '❌ ' + d.error, !d.ok);
}

async function poll() {
  try {
    S = await (await fetch('/test/status', { cache: 'no-store' })).json();
  } catch (e) {
    el('state').textContent = 'Arm unreachable'; el('state-sub').textContent = 'check WiFi'; return;
  }
  const b = S.base, sp = S.spin;
  const turns = sp.steps / 4096;
  el('turns').textContent = (turns >= 0 ? '+' : '−') + Math.abs(turns).toFixed(2);
  el('turns-sub').textContent = sp.active ? 'servo turns this spin' : 'servo turns, last spin';
  el('base-turns').textContent = '≈ ' + (turns / b.ratio).toFixed(3) + ' base turns at ratio ' + b.ratio.toFixed(1);

  el('fault-banner').style.display = S.fault ? 'block' : 'none';
  el('fault-text').textContent = S.fault_reason;
  let info = '';
  if (!S.fault && !S.armed) info = ['Not armed', 'Arm the arm on the <a href="/teach">Teach</a> page first.'];
  else if (!S.fault && S.teach) info = ['Teach mode is on', 'Press Lock &amp; Save on the <a href="/teach">Teach</a> page first.'];
  else if (!S.fault && S.auto) info = ['Auto mode is on', 'Switch the CV pipeline to manual first.'];
  el('info-banner').style.display = info ? 'block' : 'none';
  if (info) { el('info-title').textContent = info[0]; el('info-text').innerHTML = info[1]; }

  const canStart = S.armed && !S.fault && !S.teach && !S.auto && !S.busy;
  const sw = el('sw');
  sw.className = 'switch' + (sp.active ? (sp.stopping ? ' stopping' : ' on') : '');
  sw.disabled = !(sp.active ? !sp.stopping : canStart);
  el('pct').disabled = sp.active && sp.stopping;
  el('dir-pos').disabled = el('dir-neg').disabled = sp.active;
  if (sp.active) {
    el('state').textContent = sp.stopping ? 'Stopping…' : 'Spinning ' + (sp.dir > 0 ? '+' : '−') + ' at ' + sp.pct + '%';
    el('state-sub').textContent = sp.speed + ' steps/s · ≈ ' + outDps(sp.speed).toFixed(1) + '°/s';
  } else {
    el('state').textContent = 'Off';
    el('state-sub').textContent = S.move_msg ? '⚠ ' + S.move_msg : (S.busy ? 'arm is moving…' : ' ');
  }
  el('load').textContent = b.load < 0 ? '—' : b.load;
  el('load').className = 'v' + (b.load > 600 ? ' hot' : '');
  el('temp').textContent = b.temp < 0 ? '—' : b.temp + '°C';
  el('temp').className = 'v' + (b.temp > 55 ? ' hot' : '');
  el('volt').textContent = b.volt < 0 ? '—' : b.volt.toFixed(1) + ' V';
  if (!(sp.active && document.activeElement === el('pct'))) showSpeed();
  renderCv();
}

// ── Computer vision panel ───────────────────────────────────
const CAM_FIELDS = [
  ['brightness', 'Brightness', [-2,-1,0,1,2]], ['contrast', 'Contrast', [-2,-1,0,1,2]],
  ['saturation', 'Saturation', [-2,-1,0,1,2]], ['ae_level', 'Exposure', [-2,-1,0,1,2]],
  ['wb_mode', 'White balance', [[0,'auto'],[1,'sunny'],[2,'cloudy'],[3,'office'],[4,'home']]],
  ['hmirror', 'Mirror', [[0,'off'],[1,'on']]], ['vflip', 'Flip', [[0,'off'],[1,'on']]] ];
let camBuilt = false, recs = [], sampling = null;
const f0 = v => v.toFixed(0), sgn = v => (v >= 0 ? '+' : '') + v.toFixed(0);
const IK_TXT = { OK: 'reachable', OUT_OF_REACH: 'too far / too close for the links', JOINT_LIMIT: 'outside a joint limit',
  BODY_COLLISION: 'arm would hit the body', OUTSIDE_ENVELOPE: 'outside the taught envelope', NO_SOLUTION: 'no solution' };

function buildCam(cam) {
  const box = el('cam-rows');
  CAM_FIELDS.forEach(([k, name, opts]) => {
    const row = document.createElement('div');
    row.style.cssText = 'display:flex;align-items:center;gap:10px;margin-bottom:6px;font-size:0.85rem';
    const sel = document.createElement('select');
    sel.style.cssText = 'flex:1;padding:7px;border-radius:8px;background:var(--surface);color:var(--text);border:1px solid var(--border)';
    opts.forEach(o => { const [v, t] = Array.isArray(o) ? o : [o, (o > 0 ? '+' : '') + o];
      const op = document.createElement('option'); op.value = v; op.textContent = t; sel.appendChild(op); });
    sel.value = cam[k];
    sel.onchange = async () => { const d = await post('/test/cam', { [k]: +sel.value });
      toast(d.ok ? '📷 ' + name + ' set' : '❌ ' + d.error, !d.ok); };
    row.innerHTML = '<span style="width:110px;color:var(--muted)">' + name + '</span>';
    row.appendChild(sel); box.appendChild(row);
  });
}

function renderCv() {
  if (!camBuilt && S.cam) { camBuilt = true; buildCam(S.cam); }
  renderCapture();
  el('cv-clr').textContent = S.clearance;
  const c = S.cv;
  const fresh = c.valid && c.age_ms < 1500;
  el('cv-state').textContent = !c.valid ? 'Waiting for the pipeline…'
    : !fresh ? 'Pipeline silent for ' + (c.age_ms / 1000).toFixed(0) + ' s'
    : (c.det ? '🍾 Bottle detected' : 'No bottle in view') + (c.auto ? '  ·  pipeline in AUTO' : '  ·  dry run');
  el('cv-rate').textContent = c.valid ? c.det_pct + '%' : '—';
  el('cv-fps').textContent = c.valid ? c.fps.toFixed(1) + ' fps' : '—';
  if (!c.valid || c.conf === undefined) return;
  const stale = !c.det ? ' (last seen)' : '';
  el('cv-conf').textContent = c.conf.toFixed(2);
  el('cv-conf').className = 'v' + (c.conf < S.conf_min ? ' hot' : '');
  el('cv-fill').textContent = c.fill.toFixed(3) + (c.grip ? ' ✋' : '');
  el('cv-depth').textContent = f0(c.depth) + ' mm';
  el('cv-box').textContent = f0(c.px) + ',' + f0(c.py) + ' ' + f0(c.bw) + '×' + f0(c.bh);
  el('cv-tip').innerHTML = '<b>' + f0(c.tip[0]) + '</b> forward · <b>' + sgn(c.tip[1]) + '</b> left · <b>' + sgn(c.tip[2]) + '</b> up' + stale;
  el('cv-body').innerHTML = '<b>' + f0(c.body[0]) + '</b> forward · <b>' + sgn(c.body[1]) + '</b> left · <b>' + sgn(c.body[2]) + '</b> up' + stale;
  el('cv-base').innerHTML = 'x <b>' + f0(c.base[0]) + '</b> · y <b>' + sgn(c.base[1]) + '</b> · z <b>' + sgn(c.base[2]) + '</b>' +
    (c.moving ? ' <span style="color:var(--warn)">(arm was moving)</span>' : '') + stale;
  const ang = a => 'base ' + f0(a[0]) + '° sh ' + f0(a[1]) + '° el ' + f0(a[2]) + '° fa ' + f0(a[3]) + '°';
  el('cv-ik').innerHTML = 'Target: <b style="color:' + (c.ik === 'OK' ? 'var(--accent)' : 'var(--warn)') + '">' + IK_TXT[c.ik] + '</b>' +
    (c.ik === 'OK' ? ' at pitch ' + c.pitch + '° (' + ang(c.sol) + ')' : '');
  el('cv-appr').innerHTML = 'Approach point: <b style="color:' + (c.ik_appr === 'OK' ? 'var(--accent)' : 'var(--warn)') + '">' + IK_TXT[c.ik_appr] + '</b>' +
    (c.ik_appr === 'OK' ? ' at pitch ' + c.pitch_appr + '° (' + ang(c.sol_appr) + ')' : '');
  el('cv-go').disabled = !(S.calibrated && S.armed && !S.fault && !S.busy && fresh && c.det && c.ik_appr === 'OK');
  if (sampling && c.det && fresh && c.age_ms < 450) sampling.push({ tip: c.tip, base: c.base, body: c.body, fill: c.fill, depth: c.depth,
                                                               conf: c.conf, px: c.px, py: c.py, bh: c.bh, deg: S.base.deg });
}

function stats(arr) {
  const m = arr.reduce((a, b) => a + b, 0) / arr.length;
  const sd = Math.sqrt(arr.reduce((a, b) => a + (b - m) * (b - m), 0) / arr.length);
  return [m, sd];
}
async function cvRecord() {
  if (sampling) return;
  sampling = []; el('cv-rec').disabled = true; el('cv-rec').textContent = 'Recording…';
  await new Promise(r => setTimeout(r, 3000));
  const s = sampling; sampling = null;
  el('cv-rec').disabled = false; el('cv-rec').textContent = 'Record (3 s average)';
  if (s.length < 3) { toast('❌ Too few detections in 3 s (' + s.length + ')', true); return; }
  const col = f => stats(s.map(f));
  const r = { n: s.length, note: el('cv-note').value, fill: col(x => x.fill), depth: col(x => x.depth), conf: col(x => x.conf),
    px: col(x => x.px), py: col(x => x.py), bh: col(x => x.bh),
    tip: [0,1,2].map(i => col(x => x.tip[i])), base: [0,1,2].map(i => col(x => x.base[i])),
    body: [0,1,2].map(i => col(x => x.body[i])) };
  recs.push(r); renderRecs();
}
function cvClear() { recs = []; renderRecs(); }
function renderRecs() {
  const p = (m, d) => m.toFixed(d || 0);
  const lines = ['# fill depth conf px,py bh | tip fwd,left,up (±sd) | base x,y,z (±sd) | body fwd,left,up (±sd) | note'];
  recs.forEach((r, i) => lines.push('#' + (i + 1) + ' n=' + r.n + '  fill=' + p(r.fill[0], 3) + ' depth=' + p(r.depth[0]) +
    ' conf=' + p(r.conf[0], 2) + ' c=' + p(r.px[0]) + ',' + p(r.py[0]) + ' bh=' + p(r.bh[0]) +
    ' | tip ' + r.tip.map(t => p(t[0]) + '±' + p(t[1])).join(',') +
    ' | base ' + r.base.map(t => p(t[0]) + '±' + p(t[1])).join(',') +
    ' | body ' + r.body.map(t => p(t[0]) + '±' + p(t[1])).join(',') + (r.note ? ' | ' + r.note : '')));
  if (S) lines.push('# cam ' + JSON.stringify(S.cam));
  el('cv-log').value = lines.join('\n');
}
const CAP_TXT = { idle: 'idle', requested: 'requested', starting: 'starting', recording: 'recording',
  finishing: 'finishing', saved: '✅ saved', busy: 'busy', error: '❌ error' };
function renderCapture() {
  const c = S.capture;
  if (!c) return;
  const live = ['requested', 'starting', 'recording', 'finishing'].includes(c.state);
  el('cap-state').innerHTML = !c.agent && !live ? 'Capture agent not running on the laptop (start it with <code>cvagent</code>)'
    : '<b>' + (CAP_TXT[c.state] || c.state) + '</b>' + (c.msg ? ' ' + c.msg.replace(/</g, '&lt;') : '') +
      (c.file ? ' · ' + c.file.replace(/</g, '&lt;') : '');
  el('cap-go').disabled = live && c.age_ms < 15000;
}
async function cvCapture() {
  const secs = Math.max(3, Math.min(120, Math.round(+el('cap-secs').value || 20)));
  el('cap-secs').value = secs;
  const d = await post('/test/cv/capture', { secs });
  toast(d.ok ? '🎥 Capture requested (' + secs + ' s)' : '❌ ' + d.error, !d.ok);
}
async function cvApproach() {
  if (!confirm('Move the arm to ' + S.clearance + ' mm above the detected bottle?\n\nSomeone must be at the 12 V cutoff.')) return;
  const d = await post('/test/cv/approach');
  toast(d.ok ? '➡️ Approaching' : '❌ ' + d.error, !d.ok);
}
async function goHome() {
  const d = await post('/teach/preview', { pos: 'home' });
  toast(d.ok ? '🏠 Going home' : '❌ ' + d.error, !d.ok);
}

poll();
setInterval(poll, 400);
</script>
</body>
</html>
)=====";

// ============================================================
//  Arachnae Suite — the landing page at "/".
//
//  One address to remember. Links are built from the browser's own
//  location, so this works identically on the phone hotspot (whatever
//  subnet it picked today) and on the arm's own AP at 192.168.4.1.
//  Live status is polled so you can see at a glance whether the arm is
//  armed/faulted before you open anything.
// ============================================================
static const char SUITE_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Arachnae Suite</title>
<style>
 :root{--bg:#0e1116;--surface:#171c24;--border:#2a3240;--text:#e6edf3;--muted:#8b949e;--accent:#3fb950;--danger:#f85149;}
 *{box-sizing:border-box}
 body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,sans-serif;padding:18px;}
 h1{font-size:1.5rem;margin:0 0 2px;letter-spacing:.5px}
 .sub{color:var(--muted);font-size:.82rem;margin-bottom:16px}
 .status{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:12px 14px;margin-bottom:18px;font-size:.88rem}
 .pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:.76rem;font-weight:600;margin-right:6px}
 .ok{background:rgba(63,185,80,.15);color:var(--accent)}
 .bad{background:rgba(248,81,73,.15);color:var(--danger)}
 .dim{background:rgba(139,148,158,.15);color:var(--muted)}
 a.card{display:block;background:var(--surface);border:1px solid var(--border);border-radius:12px;
        padding:15px 16px;margin-bottom:11px;text-decoration:none;color:var(--text)}
 a.card:active{transform:scale(.99)}
 .t{font-size:1.05rem;font-weight:600;margin-bottom:3px}
 .d{color:var(--muted);font-size:.8rem;line-height:1.35}
 .off{opacity:.5}
 footer{color:var(--muted);font-size:.72rem;margin-top:20px;text-align:center;line-height:1.5}
</style></head><body>
<h1>ARACHNAE</h1>
<div class="sub" id="host">&nbsp;</div>

<div class="status" id="status">Connecting&hellip;</div>

<a class="card" href="/teach">
  <div class="t">Teach &amp; Control</div>
  <div class="d">Arm, jog joints, set the zero reference, save the 7 positions, speed scale and joint calibration.</div>
</a>

<a class="card" id="cam" href="#">
  <div class="t">Camera</div>
  <div class="d">Live MJPEG feed from the OV2640.</div>
</a>

<a class="card" href="/test">
  <div class="t">Tests</div>
  <div class="d">Bench tests. Base spin: run the base like a motor at 10-100 % speed until you stop it, no angle limit.</div>
</a>

<a class="card" href="/teach/diag">
  <div class="t">Servo Diagnostics</div>
  <div class="d">Raw per-servo registers: mode, torque, goal vs present position, load, voltage, temperature.</div>
</a>

<a class="card off" id="cv" href="#">
  <div class="t">Computer Vision <span class="pill dim">host-side</span></div>
  <div class="d">Object detection runs on a laptop, not the ESP32. Start <code>cv_pipeline.py</code> there;
      it reads the camera feed above and sends targets over the WebSocket.</div>
</a>

<footer id="foot"></footer>

<script>
const h = location.hostname;
document.getElementById('host').textContent = h;
document.getElementById('cam').href = 'http://' + h + ':81/stream';
document.getElementById('foot').innerHTML =
  'stream http://' + h + ':81/stream<br>websocket ws://' + h + ':8765';

function pill(t, cls){ return '<span class="pill ' + cls + '">' + t + '</span>'; }

async function poll(){
  try{
    const d = await (await fetch('/teach/status', {cache:'no-store'})).json();
    let s = '';
    s += d.fault ? pill('FAULT','bad') : pill('no fault','ok');
    s += d.armed ? pill('ARMED','ok')  : pill('disarmed','dim');
    s += d.calibrated ? pill('calibrated','ok') : pill('not calibrated','dim');
    s += pill(Math.round(d.scale*100) + '% speed','dim');
    const l = d.live;
    s += '<div style="margin-top:9px;color:var(--muted);font-size:.8rem">'
       + 'base ' + l.base_deg.toFixed(1) + '&deg; &nbsp; shoulder ' + l.sh_deg.toFixed(1)
       + '&deg; &nbsp; elbow ' + l.el_deg.toFixed(1) + '&deg; &nbsp; forearm ' + l.fa_deg.toFixed(1) + '&deg;</div>';
    if (d.fault && d.fault_reason)
      s += '<div style="margin-top:7px;color:var(--danger);font-size:.8rem">' + d.fault_reason + '</div>';
    document.getElementById('status').innerHTML = s;
  }catch(e){
    document.getElementById('status').innerHTML = pill('unreachable','bad');
  }
}
poll(); setInterval(poll, 1500);
</script>
</body></html>
)=====";

void handleSuitePage() { g_teachServer.send_P(200, "text/html", SUITE_HTML); }
void handleTeachStatus() { sendJson(200, teachStatusJSON()); }

// Raw servo register read-back. Bring-up diagnostic only: reports what the
// servo ITSELF says its mode / torque / goal registers are, so a command the
// firmware believes it sent can be told apart from one the servo accepted.
void handleTeachDiag() {
  String s = "{\"scale\":" + String(g_safetyScale, 2) + ",\"servos\":[";
  for (int i = 0; i < N_JOINTS + 1; i++) {
    uint8_t id = ARM_IDS[i];
    if (i) s += ",";
    s += "{\"id\":" + String(id);
    s += ",\"mode\":"     + String(busFor(id).readByte(id, REG_OPERATING_MODE));
    s += ",\"torque\":"   + String(busFor(id).readByte(id, REG_TORQUE_ENABLE));
    s += ",\"tq_limit\":" + String(busFor(id).readWord(id, REG_TORQUE_LIMIT));
    s += ",\"accel\":"    + String(busFor(id).readByte(id, REG_GOAL_ACCEL));
    s += ",\"goal_pos\":" + String(busFor(id).readWord(id, REG_GOAL_POSITION));
    s += ",\"goal_spd\":" + String(busFor(id).readWord(id, REG_GOAL_SPEED));
    s += ",\"pos\":"      + String(busFor(id).readWord(id, REG_PRESENT_POS));
    s += ",\"load\":"     + String(busFor(id).readWord(id, REG_PRESENT_LOAD));
    s += ",\"volt\":"     + String(busFor(id).readByte(id, REG_PRESENT_VOLTAGE));
    s += ",\"temp\":"     + String(busFor(id).readByte(id, REG_PRESENT_TEMP));
    s += "}";
  }
  s += "]}";
  sendJson(200, s);
}

void handleTeachEstop() {
  emergencyStop("E-STOP pressed (teach UI)");
  sendOk();
}

void handleTeachFaultReset() {
  if (!g_faultActive) { sendOk(); return; }
  clearFault();
  if (g_onFaultReset) g_onFaultReset();
  sendOk();
}

void handleTeachArm() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  String e = armNow(doc["force"] | false);
  if (e.length()) sendErr(e); else sendOk();
}

void handleTeachStart() {
  if (g_faultActive) { sendErr("fault active"); return; }
  demoStop("stopped: teach mode");
  if (!g_armed)      { sendErr("arm first (confirm boot pose)"); return; }
  teachModeStart();
  sendOk();
}

void handleTeachSetZero() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  String e = setZeroHere(doc["force"] | false);
  if (e.length()) sendErr(e); else sendOk();
}

void handleTeachSave() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  String e = savePosition(doc["pos"] | "");
  if (e.length()) sendErr(e); else sendOk();
}

void handleTeachPreview() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (demoRunning()) { sendErr("the demo is playing - stop it first"); return; }
  if (g_teachActive) { sendErr("lock the arm first (Lock & Save)"); return; }
  if (!allJointsIdle()) { sendErr("arm is still moving"); return; }
  bool ok = moveToTaught(doc["pos"] | "", APPROACH_SPEED);
  if (ok) sendOk(); else sendErr("not saved, not armed, or fault active");
}

void handleTeachJog() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (demoRunning()) { sendErr("the demo is playing - stop it first"); return; }
  bool ok = jogJoint(doc["joint"] | "", doc["delta"] | 0.0f);
  if (ok) sendOk(); else sendErr("busy, not armed, fault, or bad joint");
}

void handleTeachDone() {
  if (g_faultActive) { sendErr("fault active"); return; }
  teachModeEnd();
  sendOk();
}

void handleTeachScale() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  float sc = doc["scale"] | -1.0f;
  if (sc < 0.02f || sc > 1.0f) { sendErr("scale must be 0.02 - 1.0"); return; }
  g_safetyScale = sc;
  applyAccel();
  sendOk();
}

void handleTeachCfg() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (doc["grip"].is<const char*>()) {
    const char* g = doc["grip"];
    if (!strcmp(g, "clear")) { g_gripOpen = g_gripClose = -1; saveJointConfig(); sendOk(); return; }
    int32_t gp = gripperPos();
    if (gp < 0) { sendErr("gripper not responding"); return; }
    if (gp < WRAP_GUARD_STEPS || gp > SERVO_STEPS_PER_REV - 1 - WRAP_GUARD_STEPS) {
      sendErr("gripper is at the encoder wrap - jog it a little and try again"); return;
    }
    if      (!strcmp(g, "open"))  g_gripOpen  = (int16_t)gp;
    else if (!strcmp(g, "close")) g_gripClose = (int16_t)gp;
    else { sendErr("grip must be open, close or clear"); return; }
    saveJointConfig();
    sendOk();
    return;
  }
  if (doc["revbit"].is<int>()) {
    g_wheelRevBit = (g_wheelRevBit == 0x8000) ? 0x0400 : 0x8000;
    saveJointConfig();
    sendOk();
    return;
  }
  String e = setJointCalibration(doc["joint"] | -1, doc["ratio"] | 0.0f, doc["sign"] | 0,
                                 doc["backlash"] | -1, doc["torque"] | -1);
  if (e.length()) sendErr(e); else sendOk();
}

// {"op":"add","grip":0|1|2,"pause":ms} | {"op":"undo"} | {"op":"clear"} | {"op":"play"} | {"op":"stop"}
void handleTeachDemo() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  const char* op = doc["op"] | "";
  String e;
  if      (!strcmp(op, "add"))   e = demoAddStep(doc["grip"] | 0, doc["pause"] | 500);
  else if (!strcmp(op, "undo"))  e = demoRemoveLast();
  else if (!strcmp(op, "clear")) { if (demoRunning()) e = "the demo is playing"; else clearDemo(); }
  else if (!strcmp(op, "play"))  e = demoPlay();
  else if (!strcmp(op, "stop"))  demoStop("stopped");
  else e = "op must be add, undo, clear, play or stop";
  if (e.length()) sendErr(e); else sendOk();
}

void handleTeachClear() {
  clearAllTaught();
  sendOk();
}

// ============================================================
//  CV observation — what the laptop pipeline currently sees.
//
//  The pipeline sends {"type":"cv_obs",...} ~5x/s in dry-run AND auto
//  mode. It is NEVER acted on: the firmware only converts the tool-frame
//  point to the base frame with the live joint angles and runs the same
//  IK the grab FSM would (target and approach point), so the whole chain
//  camera -> YOLO -> depth -> tipToBase -> IK can be checked without the
//  arm moving. The only motion is the explicit "approach only" test.
// ============================================================
struct CvObs {
  bool     valid = false;       // at least one message received
  uint32_t ms = 0;              // millis() at receipt
  bool     det = false;         // a bottle was detected in that frame
  bool     autoPipe = false;    // pipeline was started with --auto
  bool     grip = false;        // pipeline says fill >= GRIP_FILL_RATIO
  bool     whileMoving = false; // joints were moving at receipt -> base point unreliable
  int      ndet = 0;
  float    conf = 0, fill = 0, depth = 0, px = 0, py = 0, bw = 0, bh = 0, fps = 0, inferMs = 0;
  Vec3     tip = {0, 0, 0}, base = {0, 0, 0}, appr = {0, 0, 0};   // base/appr: ARM frame
  IKResult ik = IKResult::NO_SOLUTION, ikAppr = IKResult::NO_SOLUTION;
  float    pitch = 0, pitchAppr = 0;
  JointAngles sol = {0, 0, 0, 0}, solAppr = {0, 0, 0, 0};
  uint64_t hist = 0;            // detection flags of the last 64 frames (bit 0 = newest)
  uint8_t  histN = 0;
};
CvObs g_cvObs;

void cvObserve(JsonDocument& doc) {
  CvObs& o = g_cvObs;
  o.valid = true;
  o.ms = millis();
  o.det = doc["det"] | false;
  o.autoPipe = doc["auto"] | false;
  o.fps = doc["fps"] | 0.0f;
  o.inferMs = doc["infer_ms"] | 0.0f;
  o.hist = (o.hist << 1) | (o.det ? 1 : 0);
  if (o.histN < 64) o.histN++;
  if (!o.det) return;   // keep the last detection's numbers for reference
  o.ndet = doc["n"] | 1;
  o.conf = doc["confidence"] | 0.0f;  o.fill = doc["fill"] | 0.0f;  o.depth = doc["depth"] | 0.0f;
  o.px = doc["px"] | 0.0f;  o.py = doc["py"] | 0.0f;  o.bw = doc["bw"] | 0.0f;  o.bh = doc["bh"] | 0.0f;
  o.grip = doc["grip"] | false;
  o.tip = { doc["x"] | 0.0f, doc["y"] | 0.0f, doc["z"] | 0.0f };
  o.whileMoving = !allJointsIdle();
  o.base = tipToBase(currentAngles(), o.tip.x, o.tip.y, o.tip.z);
  o.ik     = solveWithPitches(o.base.x, o.base.y, o.base.z, o.sol, o.pitch);
  o.appr   = o.base + upInArm() * APPROACH_CLEARANCE_MM;   // straight up, like the FSM
  o.ikAppr = solveWithPitches(o.appr.x, o.appr.y, o.appr.z, o.solAppr, o.pitchAppr);
}

static int cvDetectPct() {
  if (!g_cvObs.histN) return 0;
  int n = 0;
  for (int i = 0; i < g_cvObs.histN; i++) n += (g_cvObs.hist >> i) & 1;
  return n * 100 / g_cvObs.histN;
}

static String anglesJson(const JointAngles& a) {
  char b[96];
  snprintf(b, sizeof(b), "[%.1f,%.1f,%.1f,%.1f]", degrees(a.base), degrees(a.shoulder), degrees(a.elbow), degrees(a.forearm));
  return b;
}

String cvStatusJson() {
  const CvObs& o = g_cvObs;
  if (!o.valid) return "{\"valid\":false}";
  String s; s.reserve(720);
  char b[224];
  snprintf(b, sizeof(b), "{\"valid\":true,\"age_ms\":%lu,\"det\":%s,\"auto\":%s,\"fps\":%.1f,\"infer_ms\":%.0f,\"det_pct\":%d,\"frames\":%d,",
           (unsigned long)(millis() - o.ms), o.det ? "true" : "false", o.autoPipe ? "true" : "false",
           o.fps, o.inferMs, cvDetectPct(), (int)o.histN);
  s += b;
  snprintf(b, sizeof(b), "\"n\":%d,\"conf\":%.2f,\"fill\":%.3f,\"depth\":%.0f,\"px\":%.0f,\"py\":%.0f,\"bw\":%.0f,\"bh\":%.0f,\"grip\":%s,\"moving\":%s,",
           o.ndet, o.conf, o.fill, o.depth, o.px, o.py, o.bw, o.bh, o.grip ? "true" : "false", o.whileMoving ? "true" : "false");
  s += b;
  Vec3 body = armToBody(o.base);
  snprintf(b, sizeof(b), "\"tip\":[%.0f,%.0f,%.0f],\"base\":[%.0f,%.0f,%.0f],\"body\":[%.0f,%.0f,%.0f],",
           o.tip.x, o.tip.y, o.tip.z, o.base.x, o.base.y, o.base.z, body.x, body.y, body.z);
  s += b;
  s += "\"ik\":\"";      s += ikResultStr(o.ik);     s += "\",\"pitch\":" + String(o.pitch, 0);
  s += ",\"sol\":"      + anglesJson(o.sol);
  s += ",\"ik_appr\":\""; s += ikResultStr(o.ikAppr); s += "\",\"pitch_appr\":" + String(o.pitchAppr, 0);
  s += ",\"sol_appr\":" + anglesJson(o.solAppr) + "}";
  return s;
}

// ============================================================
//  CV proof capture — the ESP32 can't start programs on the laptop, so
//  /test/cv/capture only sends a trigger to the capture agent
//  (Final_Tools/cv_capture_agent.py, connected over the WebSocket), which
//  records the OpenCV preview and reports each stage back with
//  cv_capture_status. A recording trigger only: it never moves the arm,
//  arms it, or touches auto mode.
// ============================================================
struct CvCapture {
  int16_t  agent = -1;          // WebSocket client num of the capture agent, -1 = none
  String   state = "idle", file, msg;
  uint32_t ms = 0;              // millis() of the last state change
};
CvCapture g_cvCap;

void cvCaptureStatus(JsonDocument& doc) {
  g_cvCap.state = String((const char*)(doc["state"] | "?")).substring(0, 24);
  g_cvCap.file  = String((const char*)(doc["file"]  | "")).substring(0, 96);
  g_cvCap.msg   = String((const char*)(doc["msg"]   | "")).substring(0, 160);
  g_cvCap.ms    = millis();
}

String cvCaptureJson() {
  const CvCapture& c = g_cvCap;
  char b[48];
  snprintf(b, sizeof(b), "{\"agent\":%s,\"age_ms\":%lu,", c.agent >= 0 ? "true" : "false",
           (unsigned long)(millis() - c.ms));
  return String(b) + "\"state\":\"" + jsonEsc(c.state) + "\",\"file\":\"" + jsonEsc(c.file) +
         "\",\"msg\":\"" + jsonEsc(c.msg) + "\"}";
}

// ============================================================
//  Camera image settings — persisted in NVS namespace "arachnae_c" and
//  re-applied at boot, so a lighting fix found on /test sticks. The
//  first real frame was dark and green-tinted; the pipeline's depth
//  and detection both depend on a sane image.
// ============================================================
struct CamSettings {
  int8_t brightness = 0, contrast = 0, saturation = 0, aeLevel = 0;   // -2..2
  int8_t wbMode = 0;                                                  // 0 auto, 1 sunny, 2 cloudy, 3 office, 4 home
  int8_t hmirror = 0, vflip = 0;
};
CamSettings g_cam;

void camApply() {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return;
  s->set_brightness(s, g_cam.brightness);
  s->set_contrast(s, g_cam.contrast);
  s->set_saturation(s, g_cam.saturation);
  s->set_ae_level(s, g_cam.aeLevel);
  s->set_wb_mode(s, g_cam.wbMode);
  s->set_hmirror(s, g_cam.hmirror);
  s->set_vflip(s, g_cam.vflip);
}

void camLoadAndApply() {
  Preferences p;
  p.begin("arachnae_c", true);
  g_cam.brightness = p.getChar("br", 0);  g_cam.contrast = p.getChar("co", 0);
  g_cam.saturation = p.getChar("sa", 0);  g_cam.aeLevel  = p.getChar("ae", 0);
  g_cam.wbMode     = p.getChar("wb", 0);  g_cam.hmirror  = p.getChar("hm", 0);
  g_cam.vflip      = p.getChar("vf", 0);
  p.end();
  camApply();
}

static void camSave() {
  Preferences p;
  p.begin("arachnae_c", false);
  p.putChar("br", g_cam.brightness);  p.putChar("co", g_cam.contrast);
  p.putChar("sa", g_cam.saturation);  p.putChar("ae", g_cam.aeLevel);
  p.putChar("wb", g_cam.wbMode);      p.putChar("hm", g_cam.hmirror);
  p.putChar("vf", g_cam.vflip);
  p.end();
}

static String camJson() {
  char b[160];
  snprintf(b, sizeof(b), "{\"brightness\":%d,\"contrast\":%d,\"saturation\":%d,\"ae_level\":%d,\"wb_mode\":%d,\"hmirror\":%d,\"vflip\":%d}",
           g_cam.brightness, g_cam.contrast, g_cam.saturation, g_cam.aeLevel, g_cam.wbMode, g_cam.hmirror, g_cam.vflip);
  return b;
}

// ── /test — bench tests ──────────────────────────────────────
void handleTestPage() { g_teachServer.send_P(200, "text/html", TEST_HTML); }

void handleTestCam() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  auto take = [&](const char* k, int8_t& v, int lo, int hi) {
    if (!doc[k].is<int>()) return;
    v = (int8_t)constrain((int)doc[k], lo, hi);
  };
  take("brightness", g_cam.brightness, -2, 2);  take("contrast", g_cam.contrast, -2, 2);
  take("saturation", g_cam.saturation, -2, 2);  take("ae_level", g_cam.aeLevel, -2, 2);
  take("wb_mode", g_cam.wbMode, 0, 4);          take("hmirror", g_cam.hmirror, 0, 1);
  take("vflip", g_cam.vflip, 0, 1);
  camApply();
  camSave();
  sendOk();
}

// The one CV-driven motion on /test: drive to the APPROACH point
// (APPROACH_CLEARANCE_MM above the observed target) and stop there. No
// descend, no grip. Uses the target frozen at the press — it does not track.
void handleTestCvApproach() {
  const CvObs& o = g_cvObs;
  if (!motionAllowed())       { sendErr("not armed, fault active, or teach mode on"); return; }
  if (g_autoMode)             { sendErr("auto mode is on"); return; }
  if (demoRunning())          { sendErr("the demo is playing"); return; }
  if (!allJointsIdle())       { sendErr("arm is still moving"); return; }
  if (!g_taught.calibrated)   { sendErr("teach all 7 positions first - the IK envelope comes from them"); return; }
  if (!o.valid || !o.det || millis() - o.ms > 1000) { sendErr("no fresh detection (need one < 1 s old)"); return; }
  if (o.conf < CV_CONFIDENCE_THRESHOLD) { sendErr("confidence below CV_CONFIDENCE_THRESHOLD"); return; }
  if (o.whileMoving)          { sendErr("that detection was taken while the arm moved - wait for a new one"); return; }
  if (o.ikAppr != IKResult::OK) { sendErr(String("approach point has no IK solution: ") + ikResultStr(o.ikAppr)); return; }
  char buf[112];
  snprintf(buf, sizeof(buf), "[Test] CV approach to (%.0f, %.0f, %.0f) pitch %.0f",
           o.appr.x, o.appr.y, o.appr.z, o.pitchAppr);
  wsLog(buf);
  if (!armMoveToAngles(o.solAppr, APPROACH_SPEED)) { sendErr("move refused"); return; }
  sendOk();
}

void handleTestCvCapture() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (g_cvCap.agent < 0) { sendErr("capture agent not running on the laptop"); return; }
  int secs = constrain((int)(doc["secs"] | 20), 3, 120);
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"type\":\"cv_capture\",\"secs\":%d,\"t\":%lu}", secs, (unsigned long)millis());
  wsSendTo((uint8_t)g_cvCap.agent, buf);
  g_cvCap.state = "requested"; g_cvCap.file = ""; g_cvCap.msg = String(secs) + " s"; g_cvCap.ms = millis();
  sendOk();
}

void handleTestHold() {
  demoStop("stopped (hold)");
  abortAllMotion();
  sendOk();
}

// Live readout for the test page. Reads the base's load / temp / voltage
// straight off the bus (three reads per poll, same cost as one safety-monitor
// slot) so a spin can be watched for strain while it runs.
void handleTestStatus() {
  const JointTracker& b = g_j[0];
  int16_t load = g_bus.readWord(b.id, REG_PRESENT_LOAD);
  int16_t temp = g_bus.readByte(b.id, REG_PRESENT_TEMP);
  int16_t volt = g_bus.readByte(b.id, REG_PRESENT_VOLTAGE);
  String s; s.reserve(520);
  char buf[200];
  snprintf(buf, sizeof(buf), "{\"armed\":%s,\"fault\":%s,\"teach\":%s,\"auto\":%s,\"busy\":%s,",
           g_armed ? "true" : "false", g_faultActive ? "true" : "false", g_teachActive ? "true" : "false",
           g_autoMode ? "true" : "false", allJointsIdle() ? "false" : "true");
  s += buf;
  s += "\"fault_reason\":\"" + jsonEsc(g_faultReason) + "\",\"move_msg\":\"" + jsonEsc(g_moveMsg) + "\",";
  snprintf(buf, sizeof(buf), "\"base\":{\"deg\":%.1f,\"min\":%.1f,\"max\":%.1f,\"ratio\":%.3f,\"mv\":%d,"
           "\"load\":%d,\"temp\":%d,\"volt\":%.1f,\"max_speed\":%d},",
           jDeg(b), BASE_MIN_DEG, BASE_MAX_DEG, b.ratio, (int)b.mv,
           load >= 0 ? (load & 0x03FF) : -1, temp, volt >= 0 ? volt * 0.1f : -1.0f, wheelMagMax());
  s += buf;
  snprintf(buf, sizeof(buf), "\"spin\":{\"active\":%s,\"stopping\":%s,\"dir\":%d,\"pct\":%d,\"speed\":%u,\"steps\":%ld},",
           g_baseSpin.active ? "true" : "false", g_baseSpin.stopping ? "true" : "false",
           (int)g_baseSpin.dir, (int)g_baseSpin.pct, (unsigned)g_baseSpin.speed, (long)g_baseSpin.steps);
  s += buf;
  snprintf(buf, sizeof(buf), "\"calibrated\":%s,\"grip_cal\":%s,\"conf_min\":%.2f,\"clearance\":%.0f,",
           g_taught.calibrated ? "true" : "false", gripperCalibrated() ? "true" : "false",
           CV_CONFIDENCE_THRESHOLD, APPROACH_CLEARANCE_MM);
  s += buf;
  s += "\"cam\":" + camJson() + ",\"cv\":" + cvStatusJson() + ",\"capture\":" + cvCaptureJson() + "}";
  sendJson(200, s);
}

void handleTestBaseSpin() {
  JsonDocument doc;
  if (!readBody(doc)) return;
  if (g_autoMode) { sendErr("auto mode is on - switch to manual first"); return; }
  if (demoRunning()) { sendErr("the demo is playing - stop it first"); return; }
  String e = baseSpinStart(doc["dir"] | 0, doc["pct"] | 0);
  if (e.length()) sendErr(e); else sendOk();
}

void handleTestBaseStop() {
  baseSpinStop();
  sendOk();
}

// ============================================================
//  Start the teach HTTP server (call from setup(), after WiFi is up)
// ============================================================
void teachServerSetup() {
  g_teachServer.on("/",                  HTTP_GET,  handleSuitePage);
  g_teachServer.on("/teach",             HTTP_GET,  handleTeachPage);
  g_teachServer.on("/teach/status",      HTTP_GET,  handleTeachStatus);
  g_teachServer.on("/teach/diag",        HTTP_GET,  handleTeachDiag);
  g_teachServer.on("/teach/estop",       HTTP_POST, handleTeachEstop);
  g_teachServer.on("/teach/fault_reset", HTTP_POST, handleTeachFaultReset);
  g_teachServer.on("/teach/arm",         HTTP_POST, handleTeachArm);
  g_teachServer.on("/teach/start",       HTTP_POST, handleTeachStart);
  g_teachServer.on("/teach/setzero",     HTTP_POST, handleTeachSetZero);
  g_teachServer.on("/teach/save",        HTTP_POST, handleTeachSave);
  g_teachServer.on("/teach/preview",     HTTP_POST, handleTeachPreview);
  g_teachServer.on("/teach/jog",         HTTP_POST, handleTeachJog);
  g_teachServer.on("/teach/done",        HTTP_POST, handleTeachDone);
  g_teachServer.on("/teach/scale",       HTTP_POST, handleTeachScale);
  g_teachServer.on("/teach/cfg",         HTTP_POST, handleTeachCfg);
  g_teachServer.on("/teach/clear",       HTTP_POST, handleTeachClear);
  g_teachServer.on("/teach/demo",        HTTP_POST, handleTeachDemo);
  g_teachServer.on("/test",              HTTP_GET,  handleTestPage);
  g_teachServer.on("/test/status",       HTTP_GET,  handleTestStatus);
  g_teachServer.on("/test/base/spin",    HTTP_POST, handleTestBaseSpin);
  g_teachServer.on("/test/base/stop",    HTTP_POST, handleTestBaseStop);
  g_teachServer.on("/test/cam",          HTTP_POST, handleTestCam);
  g_teachServer.on("/test/cv/approach",  HTTP_POST, handleTestCvApproach);
  g_teachServer.on("/test/cv/capture",   HTTP_POST, handleTestCvCapture);
  g_teachServer.on("/test/hold",         HTTP_POST, handleTestHold);
  g_teachServer.begin();
}

void teachServerLoop() {
  g_teachServer.handleClient();
}

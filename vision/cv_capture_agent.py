#!/usr/bin/env python3
"""
ARACHNAE — CV proof capture agent (runs on the laptop).

The arm's /test page has a CV PROOF CAPTURE button, but the ESP32 can't start
programs on the laptop. This agent connects to the arm's WebSocket, says
{"type":"agent_hello","role":"capture"}, and when the button sends
{"type":"cv_capture","secs":N} it:

  1. finds the OpenCV preview window ("ARACHNAE CV") through niri, or starts
     cv_pipeline_visual_servo.py --preview (dry run, never --auto) if it isn't open,
  2. focuses + fullscreens it and records the monitor with gpu-screen-recorder
     (niri on Wayland has no per-window capture),
  3. restores the window (or stops the pipeline it started), and
  4. saves captures/opencv_proof.mp4 + opencv_proof.txt (an earlier take is kept,
     renamed opencv_proof_<its date>.mp4).

The same capture is started by the RECORD button on the agent's own page,
http://127.0.0.1:8770 (opened in the browser at start), which needs no firmware
change. Every stage is reported back with {"type":"cv_capture_status","state","file","msg"},
which /test shows. This agent never sends anything that can move the arm.

    .venv/bin/python cv_capture_agent.py              # button page + /test button
    .venv/bin/python cv_capture_agent.py --now 20     # one 20 s capture, no button
"""

import argparse
import asyncio
import atexit
import http.server
import json
import os
import queue
import signal
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import websockets

HERE       = Path(__file__).resolve().parent
PIPELINE   = HERE / "cv_pipeline_visual_servo.py"
PYTHON     = HERE / ".venv" / "bin" / "python"
OUT_DIR    = HERE / "captures"                      # vision/captures/ (gitignored)
WINDOW     = "ARACHNAE CV"                         # cv2.imshow name in the pipeline
MONITOR    = "eDP-1"
MIN_BYTES_PER_S = 12_000                          # a blank window records ~6 KB/s
WINDOW_TIMEOUT_S = 25
FIRST_FRAME_LINE = "[CV] preview: first frame shown"   # printed by the pipeline
WARMUP_S   = 2.0                                   # let detections start before recording
START_PAD_S = 0.4                                  # gpu-screen-recorder needs ~0.3 s to start
OUT_NAME   = "opencv_proof"                        # captures/opencv_proof.mp4 + .txt

# Only messages of these types are ever sent to the arm.
SAFE_TYPES = {"agent_hello", "cv_capture_status"}


# Child processes run in their own sessions (so a stray Ctrl-C in the terminal
# can't half-kill them); this makes sure none outlives the agent.
_children = set()


def _reap_children():
    for proc in list(_children):
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGINT)
                proc.wait(timeout=8)
            except (ProcessLookupError, subprocess.TimeoutExpired):
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass


atexit.register(_reap_children)


def log(msg):
    print(f"[{datetime.now():%H:%M:%S}] {msg}", flush=True)


class CaptureError(Exception):
    pass


# ── niri ─────────────────────────────────────────────────────
def niri(*args, json_out=False):
    cmd = ["niri", "msg"] + (["--json"] if json_out else []) + list(args)
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
    if r.returncode != 0:
        raise CaptureError(f"niri {' '.join(args)} failed: {r.stderr.strip() or r.returncode}")
    return json.loads(r.stdout) if json_out else r.stdout


def find_window():
    for w in niri("windows", json_out=True):
        if w.get("title") == WINDOW:
            return w
    return None


def get_window(wid):
    for w in niri("windows", json_out=True):
        if w.get("id") == wid:
            return w
    return None


def monitor_size():
    out = niri("outputs", json_out=True).get(MONITOR) or {}
    lg = out.get("logical") or {}
    return lg.get("width", 1920), lg.get("height", 1080)


def is_fullscreen(w, size):
    ws = (w.get("layout") or {}).get("window_size") or [0, 0]
    return ws[0] >= size[0] and ws[1] >= size[1]


# ── pipeline ─────────────────────────────────────────────────
def pipeline_cmd(args):
    cmd = [str(PYTHON), str(PIPELINE), "--cam", args.cam, "--arm", args.arm, "--preview"]
    cmd += args.pipeline_arg or []
    # Hard rule: the proof capture is a dry run. Never let the arm move.
    # argparse accepts any unambiguous prefix, so --au / --aut count too.
    if any(len(a.split("=")[0]) > 3 and "--auto".startswith(a.split("=")[0]) for a in cmd):
        raise CaptureError("refusing to start the pipeline with --auto")
    return cmd


class Pipeline:
    """The pipeline this agent started (None-like if it reused an open one)."""

    def __init__(self, cmd):
        self.cmd = cmd
        self.lines = deque(maxlen=20000)               # (time, line)
        env = dict(os.environ, QT_QPA_PLATFORM="xcb", DISPLAY=":1", PYTHONUNBUFFERED="1")
        self.proc = subprocess.Popen(cmd, cwd=HERE, env=env, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, bufsize=1,
                                     start_new_session=True)
        _children.add(self.proc)
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self):
        for line in self.proc.stdout:
            line = line.rstrip()
            self.lines.append((time.time(), line))

    def between(self, t0, t1):
        return [l for t, l in list(self.lines) if t0 <= t <= t1]

    def seen(self, text):
        return any(text in l for _, l in list(self.lines))

    def tail(self, n=5):
        return [l for _, l in list(self.lines)[-n:]]

    def stop(self):
        if self.proc.poll() is not None:
            return
        try:
            os.killpg(self.proc.pid, signal.SIGINT)
            self.proc.wait(timeout=8)
        except (subprocess.TimeoutExpired, ProcessLookupError):
            try:
                os.killpg(self.proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


# ── capture ──────────────────────────────────────────────────
def keep_previous_take(mp4):
    """opencv_proof.mp4 is always the newest take; an older one (and its .txt)
    is renamed opencv_proof_<its date>.mp4 instead of being overwritten."""
    if not mp4.exists():
        return
    old = datetime.fromtimestamp(mp4.stat().st_mtime)
    for src in (mp4, mp4.with_suffix(".txt")):
        if src.exists():
            dst = src.with_name(f"{mp4.stem}_{old:%Y-%m-%d_%H-%M-%S}{src.suffix}")
            src.rename(dst)
            log(f"kept the previous take as {dst.name}")


def probe_duration(path):
    try:
        r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                            "-of", "default=nw=1:nk=1", str(path)],
                           capture_output=True, text=True, timeout=10)
        return float(r.stdout.strip())
    except (ValueError, OSError, subprocess.TimeoutExpired):
        return None


def capture(secs, args, report, build="unknown"):
    """Record the CV preview for `secs` seconds. Returns the saved mp4 path."""
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    pipe = None
    wid = None
    toggled = False
    prev_focus = None
    try:
        try:
            prev_focus = (niri("focused-window", json_out=True) or {}).get("id")
        except CaptureError:
            pass

        # 1. find or start the preview
        report("starting", msg="looking for the preview window")
        w = find_window()
        if w is None:
            cmd = pipeline_cmd(args)
            report("starting", msg="opening the CV preview")
            log("starting pipeline: " + " ".join(cmd))
            pipe = Pipeline(cmd)
            # The window exists from startup (namedWindow), so wait for the
            # pipeline to say a camera frame is actually on it.
            t0 = time.time()
            while w is None or not pipe.seen(FIRST_FRAME_LINE):
                if pipe.proc.poll() is not None:
                    raise CaptureError("pipeline exited: " + " | ".join(pipe.tail(3)))
                if time.time() - t0 > WINDOW_TIMEOUT_S:
                    cam = [l for _, l in list(pipe.lines) if l.startswith("[CAM]")]
                    why = (f"no camera frames in {WINDOW_TIMEOUT_S} s - is {args.cam} up?"
                           + (f" ({cam[-1][:80]})" if cam else "")
                           if w is not None else
                           f"preview window never appeared in {WINDOW_TIMEOUT_S} s")
                    if w is not None and not pipe.seen("[CV] WebSocket connected"):
                        why = f"pipeline can't reach the arm at {args.arm}, so it shows no frames"
                    raise CaptureError(why)
                time.sleep(0.5)
                if w is None:
                    w = find_window()
            report("starting", msg="preview open, waiting for detections")
            time.sleep(WARMUP_S)
        else:
            log(f"reusing open preview window id {w['id']}")
        wid = w["id"]

        # 2. make it the only thing on screen
        size = monitor_size()
        niri("action", "focus-window", "--id", str(wid))
        if not is_fullscreen(w, size):
            niri("action", "fullscreen-window", "--id", str(wid))
            toggled = True
        time.sleep(1.0)                               # let the animation finish
        w = get_window(wid)
        if w is None:
            raise CaptureError("preview window closed before recording")
        if not is_fullscreen(w, size):
            log(f"warning: window size {w['layout'].get('window_size')} after fullscreen")

        # 3. record the monitor
        stamp = datetime.now()
        mp4 = out_dir / f"{OUT_NAME}.mp4"
        part = out_dir / f".{OUT_NAME}.part.mp4"
        rec_cmd = ["gpu-screen-recorder", "-w", MONITOR, "-f", "60", "-k", "h264",
                   "-q", "very_high", "-c", "mp4", "-cursor", "no", "-o", str(part)]
        log("recording: " + " ".join(rec_cmd))
        rec_log = []
        rec = subprocess.Popen(rec_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, start_new_session=True)
        _children.add(rec)
        threading.Thread(target=lambda: rec_log.extend(rec.stdout), daemon=True).start()
        t_rec0 = time.time()
        try:
            for k in range(secs):
                report("recording", msg=f"{k}/{secs} s")
                end = t_rec0 + k + 1 + (START_PAD_S if k == secs - 1 else 0)
                while time.time() < end:
                    if rec.poll() is not None:
                        raise CaptureError("recorder exited early: " +
                                           " | ".join(l.strip() for l in rec_log[-3:]))
                    if pipe and pipe.proc.poll() is not None:
                        raise CaptureError("pipeline exited during the recording")
                    time.sleep(0.05)
        finally:
            t_rec1 = time.time()
            if rec.poll() is None:
                os.kill(rec.pid, signal.SIGINT)       # SIGINT finalises the mp4
                try:
                    rec.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    rec.kill()
                    rec.wait()
        report("finishing", msg="saving")

        # 4. check the file
        size_b = part.stat().st_size if part.exists() else 0
        min_bytes = MIN_BYTES_PER_S * secs
        if rec.returncode not in (0, None) and size_b < min_bytes:
            raise CaptureError(f"recorder failed (exit {rec.returncode})")
        if size_b < min_bytes:
            part.unlink(missing_ok=True)
            raise CaptureError(f"recording nearly empty ({size_b} bytes) - "
                               "was the preview showing camera frames?")
        dur = probe_duration(part)
        if dur is not None and dur < secs - 2:
            log(f"warning: recorded {dur:.1f} s, asked for {secs} s")
        keep_previous_take(mp4)
        part.rename(mp4)

        # 5. sidecar for the video editor
        mon_w, mon_h = size
        crop_w = mon_h * 4 // 3
        p_lines = pipe.between(t_rec0, t_rec1) if pipe else []
        txt = [
            f"CV proof capture  {stamp:%Y-%m-%d %H:%M:%S}",
            f"video       {mp4.name}  ({size_b / 1e6:.1f} MB, "
            + (f"{dur:.1f} s" if dur is not None else "duration unknown") + f", asked {secs} s)",
            f"recorded    monitor {MONITOR}, preview window fullscreen, no cursor, 60 fps h264",
            f"image area  the 4:3 camera image is pillarboxed in grey; to keep only it: "
            f"ffmpeg -vf crop={crop_w}:{mon_h}:{(mon_w - crop_w) // 2}:0",
            f"arm build   {build}",
            f"camera      {args.cam}",
            f"arm ws      {args.arm}",
            "pipeline    " + (" ".join(pipe.cmd) if pipe else
                              "already open (started by hand) - command and log not captured"),
            "mode        dry run (never --auto): the arm was not moved",
        ]
        try:
            src = PIPELINE.read_text()
            for key in ("DEFAULT_MODEL", "TARGET_CLASSES", "CONF_THRESHOLD"):
                line = next((l for l in src.splitlines() if l.startswith(key)), None)
                if line:
                    txt.append(f"default     {line.split('#')[0].strip()}")
        except OSError:
            pass
        if pipe:
            txt.append("")
            txt.append("pipeline output during the recording:")
            txt += p_lines or ["(none)"]
            startup = [l for t, l in list(pipe.lines) if t < t_rec0 and
                       l.startswith(("[YOLO]", "[CV] target classes", "[CV] GRIP"))]
            if startup:
                txt.append("")
                txt.append("pipeline startup:")
                txt += startup
        mp4.with_suffix(".txt").write_text("\n".join(txt) + "\n")
        log(f"saved {mp4}")
        return mp4
    finally:
        if pipe:
            pipe.stop()
        elif wid is not None and toggled:
            try:
                if get_window(wid) is not None:
                    niri("action", "fullscreen-window", "--id", str(wid))
            except CaptureError as e:
                log(f"could not un-fullscreen: {e}")
        if prev_focus is not None and prev_focus != wid:
            try:
                niri("action", "focus-window", "--id", str(prev_focus))
            except CaptureError:
                pass


# ── RECORD button page ───────────────────────────────────────
BUTTON_HTML = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OpenCV Proof Recorder</title>
<style>
:root { --bg:#f4f5f7; --card:#fff; --text:#15171c; --muted:#5d6470; --border:#d9dce2; --accent:#d62839; --ok:#0a7f55; --warn:#b25e00; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg:#0f1115; --card:#181b21; --text:#eceef2; --muted:#9aa1ad; --border:#2b3039; --accent:#ff4d5e; --ok:#3ddc97; --warn:#ffb454; } }
:root[data-theme="dark"] { --bg:#0f1115; --card:#181b21; --text:#eceef2; --muted:#9aa1ad; --border:#2b3039; --accent:#ff4d5e; --ok:#3ddc97; --warn:#ffb454; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:16px/1.45 system-ui,sans-serif; padding:16px; }
.card { max-width:460px; margin:40px auto; background:var(--card); border:1px solid var(--border); border-radius:14px; padding:24px; }
h1 { font-size:1.15rem; margin:0 0 4px; } .sub { color:var(--muted); font-size:.85rem; margin:0 0 20px; }
.row { display:flex; gap:10px; align-items:stretch; }
#go { flex:1; font:700 1.1rem system-ui; letter-spacing:.04em; padding:18px; border:0; border-radius:10px;
      background:var(--accent); color:#fff; cursor:pointer; }
#go:disabled { opacity:.45; cursor:default; }
label { display:flex; flex-direction:column; font-size:.75rem; color:var(--muted); }
#secs { width:84px; flex:1; font-size:1.1rem; padding:8px; border-radius:10px; border:1px solid var(--border);
        background:var(--bg); color:var(--text); text-align:center; }
#state { margin-top:18px; font-weight:600; min-height:1.5em; } #state.ok { color:var(--ok); } #state.err { color:var(--accent); }
#arm { margin-top:6px; font-size:.85rem; } #arm.warn { color:var(--warn); }
code { font-size:.8rem; word-break:break-all; color:var(--muted); }
</style></head><body>
<div class="card">
  <h1>OpenCV proof recorder</h1>
  <p class="sub">Opens the CV preview (dry run, the arm never moves), records only that window, and saves it as
    <code id="out">opencv_proof.mp4</code></p>
  <div class="row">
    <button id="go" onclick="rec()">&#9679; RECORD</button>
    <label>seconds<input id="secs" type="number" min="3" max="120" value="20"></label>
  </div>
  <div id="state">&nbsp;</div>
  <div id="arm"></div>
</div>
<script>
const el = id => document.getElementById(id);
const TXT = { idle:'Ready', requested:'Starting\u2026', starting:'Starting', recording:'\u25CF Recording',
              finishing:'Saving\u2026', saved:'\u2705 Saved', error:'\u274C Error' };
async function rec() {
  const secs = Math.max(3, Math.min(120, Math.round(+el('secs').value || 20)));
  el('secs').value = secs; el('go').disabled = true;
  try {
    const d = await (await fetch('/record', { method:'POST', body: JSON.stringify({ secs }) })).json();
    if (!d.ok) { el('state').textContent = '\u274C ' + d.error; el('state').className = 'err'; }
  } catch (e) { el('state').textContent = '\u274C agent not running'; el('state').className = 'err'; }
  poll();
}
async function poll() {
  let s;
  try { s = await (await fetch('/status', { cache:'no-store' })).json(); }
  catch (e) { el('state').textContent = 'Agent not running \u2014 start it with cvagent'; el('state').className = 'err';
              el('go').disabled = true; return; }
  el('out').textContent = s.out;
  el('state').textContent = (TXT[s.state] || s.state) + (s.msg ? ' ' + s.msg : '') + (s.file ? ' \u00B7 ' + s.file : '');
  el('state').className = s.state === 'saved' ? 'ok' : s.state === 'error' ? 'err' : '';
  el('go').disabled = s.busy;
  el('arm').textContent = s.arm ? 'Arm connected' : 'Arm not connected \u2014 the preview needs the laptop on the ARACHNAE Wi-Fi';
  el('arm').className = s.arm ? '' : 'warn';
}
poll(); setInterval(poll, 500);
</script></body></html>
"""


# ── WebSocket agent ──────────────────────────────────────────
class Agent:
    def __init__(self, args):
        self.args = args
        self.out = queue.Queue()                  # status messages from the worker thread
        self.worker = None
        self.build = "unknown"
        self.last_status = None
        self.connected = False                    # WebSocket to the arm is up
        self.status = {"state": "idle", "file": "", "msg": ""}
        self.lock = threading.Lock()

    def report(self, state, file="", msg=""):
        log(f"{state}: {msg or file}")
        st = {"state": state, "file": file, "msg": msg}
        if state != "busy":
            self.status = st
        self.out.put(dict(type="cv_capture_status", **st))

    def _run(self, secs):
        try:
            mp4 = capture(secs, self.args, self.report, self.build)
            self.report("saved", file=mp4.name)
        except Exception as e:                      # report every failure to /test
            self.report("error", msg=str(e)[:150])

    def start(self, secs):
        with self.lock:
            if self.worker and self.worker.is_alive():
                self.report("busy", msg="a capture is already running")
                return False
            secs = max(3, min(120, int(secs)))
            self.status = {"state": "requested", "file": "", "msg": f"{secs} s"}
            self.worker = threading.Thread(target=self._run, args=(secs,), daemon=True)
            self.worker.start()
            return True

    def serve_button(self, port):
        """The RECORD button page on http://127.0.0.1:<port> (laptop only)."""
        agent = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def _send(self, code, body, ctype="application/json"):
                data = body.encode()
                self.send_response(code)
                self.send_header("Content-Type", ctype + "; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(data)

            def do_GET(self):
                if self.path == "/":
                    self._send(200, BUTTON_HTML, "text/html")
                elif self.path == "/status":
                    busy = bool(agent.worker and agent.worker.is_alive())
                    self._send(200, json.dumps(dict(agent.status, busy=busy, arm=agent.connected,
                                                    out=str(Path(agent.args.out) / f"{OUT_NAME}.mp4"))))
                else:
                    self._send(404, "{}")

            def do_POST(self):
                if self.path != "/record":
                    return self._send(404, "{}")
                try:
                    n = int(self.headers.get("Content-Length") or 0)
                    secs = int(json.loads(self.rfile.read(n) or b"{}").get("secs", 20))
                except (ValueError, TypeError):
                    return self._send(400, json.dumps({"ok": False, "error": "bad request"}))
                log(f"capture requested from the button page: {secs} s")
                ok = agent.start(secs)
                self._send(200, json.dumps({"ok": ok, "error": "" if ok else "a capture is already running"}))

        srv = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        return srv

    async def send(self, ws, obj):
        assert obj.get("type") in SAFE_TYPES, obj
        await ws.send(json.dumps(obj))

    async def run(self):
        while True:
            try:
                async with websockets.connect(self.args.arm, ping_interval=5, ping_timeout=10,
                                              open_timeout=5) as ws:
                    log(f"connected to {self.args.arm}")
                    self.connected = True
                    await self.send(ws, {"type": "agent_hello", "role": "capture"})
                    if self.last_status:
                        await self.send(ws, self.last_status)
                    while True:
                        while not self.out.empty():
                            self.last_status = self.out.get_nowait()
                            await self.send(ws, self.last_status)
                        try:
                            raw = await asyncio.wait_for(ws.recv(), timeout=0.2)
                        except asyncio.TimeoutError:
                            continue
                        try:
                            data = json.loads(raw)
                        except ValueError:
                            continue
                        t = data.get("type")
                        if t == "hello":
                            self.build = data.get("build", "unknown (pre-suite-6 firmware)")
                            log(f"arm build: {self.build}")
                        elif t == "cv_capture":
                            log(f"capture requested from /test: {data.get('secs')} s")
                            self.start(data.get("secs", 20))
            except (websockets.ConnectionClosed, OSError, asyncio.TimeoutError) as e:
                if self.connected:
                    log("lost the arm connection")
                self.connected = False
                log(f"arm not reachable ({e.__class__.__name__}: {e}) - retrying in 2 s")
                await asyncio.sleep(2)


async def fetch_build(url):
    try:
        async with websockets.connect(url, open_timeout=3) as ws:
            data = json.loads(await asyncio.wait_for(ws.recv(), timeout=3))
            return data.get("build", "unknown (pre-suite-6 firmware)")
    except Exception as e:
        return f"unknown (arm not reachable: {e.__class__.__name__})"


def main():
    ap = argparse.ArgumentParser(description="Record the CV preview when /test asks for it.")
    ap.add_argument("--arm", default="ws://192.168.4.1:8765", help="arm WebSocket")
    ap.add_argument("--cam", default="http://192.168.4.1:81/stream", help="camera stream for the pipeline")
    ap.add_argument("--now", type=int, metavar="SECS",
                    help="record one capture of SECS seconds straight away, then exit")
    ap.add_argument("--out", default=str(OUT_DIR), help=f"where to save (default {OUT_DIR})")
    ap.add_argument("--port", type=int, default=8770, help="button page port (127.0.0.1 only)")
    ap.add_argument("--no-browser", action="store_true", help="don't open the button page at start")
    ap.add_argument("--pipeline-arg", action="append", metavar="ARG",
                    help="extra argument for the pipeline (repeatable), e.g. --pipeline-arg=--conf=0.4")
    args = ap.parse_args()
    try:
        pipeline_cmd(args)                          # refuse --auto before doing anything
    except CaptureError as e:
        ap.error(str(e))

    if args.now:
        secs = max(3, min(120, args.now))
        build = asyncio.run(fetch_build(args.arm))
        try:
            mp4 = capture(secs, args, lambda s, file="", msg="": log(f"{s}: {msg or file}"), build)
            print(mp4)
        except CaptureError as e:
            log(f"ERROR: {e}")
            sys.exit(1)
        return

    agent = Agent(args)
    try:
        agent.serve_button(args.port)
    except OSError as e:
        ap.error(f"port {args.port} busy ({e}) - is cvagent already running?")
    url = f"http://127.0.0.1:{args.port}"
    log(f"CV capture agent - RECORD button at {url} (and /test once suite-6 is on the arm); "
        f"saving to {Path(args.out) / (OUT_NAME + '.mp4')}")
    if not args.no_browser:
        subprocess.Popen(["xdg-open", url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
    try:
        asyncio.run(agent.run())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
============================================================
 PROJET ARACHNAE — Phone-Side Computer Vision Pipeline
 VISUAL-SERVO VARIANT — copy of cv_pipeline.py.

 This is a copy of cv_pipeline.py with the depth/alignment model
 replaced. The original assumed a fixed real-world object height
 (BOTTLE_HEIGHT_MM) and back-computed metric depth from it — wrong
 for anything that isn't ~21cm tall. This version instead does
 classic visual servoing:
   - the object's PIXEL POSITION in the frame drives alignment
     (left/right, up/down) — same pinhole math as before,
   - how much of the frame the object's bounding box FILLS drives
     the approach distance and the "close enough to grip" decision,
     via one calibration measurement (GRIP_DISTANCE_MM / GRIP_FILL_RATIO)
     instead of an assumed object size.
 See estimate_depth_from_fill() below for the only real change.

 Runs on Android (Termux + pip) or any Python 3.9+ machine.

 Pipeline:
   1. Pull MJPEG stream from ESP32-CAM over WiFi
   2. Run YOLOv8s (ONNX, 480x640) to detect bottle-like classes (bottle/vase/wine_glass)
   3. Estimate depth from how much the bbox fills the frame
   4. Convert pixel bbox → world XYZ (mm)
   5. Send target over WebSocket to main ESP32

 Install (Termux / PC):
   pip install numpy opencv-python-headless \
               onnxruntime websockets requests

 YOLOv8n ONNX export (run once on PC):
   pip install ultralytics
   from ultralytics import YOLO
   YOLO('yolov8n.pt').export(format='onnx', imgsz=320)
   # produces yolov8n.onnx — copy to phone/same directory

 Before first use, CALIBRATE (see GRIP_DISTANCE_MM / GRIP_FILL_RATIO
 below): hold the real object at the distance you want the arm to
 stop and grip from, run with --preview, and read the fill ratio
 printed on the overlay/console at that distance.

 Usage:
   python cv_pipeline_visual_servo.py \
     --cam  http://192.168.x.x:81/stream \
     --arm  ws://192.168.x.x:8765 \
     --model yolov8n.onnx  [--auto]   # --auto = arm is allowed to move
============================================================
"""

import argparse
import asyncio
import json
import math
import threading
import time
from collections import deque
from pathlib import Path

import cv2
import numpy as np
import websockets

try:
    import onnxruntime as ort
    ORT_AVAILABLE = True
except ImportError:
    ORT_AVAILABLE = False
    print("[CV] onnxruntime not found — running mock mode")

# ── Physical constants (must match config.h) ─────────────────
# Camera intrinsics at VGA (640×480), MEASURED 2026-10-02 (docs/CV_TESTS.md
# A5-A7, fitted to the tape distances). The lens is much wider than the
# ~55° FOV first assumed (old FX 614): FX ~360 px = ~83° horizontal.
# CAM_CX is where a bottle STRAIGHT AHEAD OF THE GRIPPER lands (~290, not
# the image centre 320): it absorbs the camera's small yaw on its mount.
# Square pixels assumed (FY = FX). The firmware's CAM_* in config.h are
# unused (pixelToWorld has no callers) - this file is the source of truth.
CAM_FX  = 360.0
CAM_FY  = 360.0
CAM_CX  = 290.0
CAM_CY  = 240.0
CAM_W   = 640
CAM_H   = 480

# ── Visual-servo calibration [CALIBRATE — measure these, don't guess] ──
# Fill ratio = bbox_height_px / frame_height_px, i.e. how much of the
# camera frame the object's bounding box occupies vertically. It rises
# as the object gets closer, with no assumption about its real size.
#
# To calibrate: hold the object at the distance you want the arm to
# stop and grip from, run with --preview, and read the fill ratio
# printed for that frame. Enter both numbers here.
# Measured 2026-10-02 with the 295 mm green bottle: fill x distance-to-front
# was 138 mm (121-170) across 10 records, so depth = 138 / fill. 0.90 is
# the A4 "between the jaws" reading, where the bottle already overflows the
# frame - the closest range the fill model can measure. Other objects need
# their own calibration (fill scales with the object's height).
GRIP_DISTANCE_MM  = 153.0   # lens-to-bottle-front distance at GRIP_FILL_RATIO
GRIP_FILL_RATIO   = 0.90    # bbox_height / frame_height at that distance
APPROACH_MAX_MM   = 1000.0  # cap on the estimate when fill is tiny/noisy (IK rejects what it can't reach)

# Target = the bottle's axis, not its front surface the fill model measures.
BOTTLE_RADIUS_MM  = 40.0    # [VERIFY] half the demo bottle's diameter

# Where the camera lens sits in the TOOL frame (x fwd, y left, z up), relative
# to the grasp centre between the finger pads - the point the firmware's tool
# frame ("frame":"tip") should be, i.e. L3 = wrist -> grasp centre (~475 mm in
# the CAD). Up 25 mm measured (A8); back 72 mm from CAD Main.stp (wrist->lens
# 403, wrist->grasp centre ~475), consistent with A8's "27 mm behind the jaw
# root". [VERIFY] the 72 against the real gripper.
CAM_POS_FWD_MM    = -72.0
CAM_POS_LEFT_MM   = 0.0
CAM_POS_UP_MM     = 25.0

# COCO classes accepted as "the bottle". YOLOv8n labels bottles
# inconsistently: the real green glass demo bottle scored 0.84 as "vase"
# and only 0.14 as "bottle" (2026-10-02), so filtering on bottle alone
# threw away a solid detection. Each anchor's score is the best of these.
# Add "cup" (41) with --classes if a clear plastic bottle reads as a cup.
COCO_NAMES = ("person bicycle car motorcycle airplane bus train truck boat "
    "traffic_light fire_hydrant stop_sign parking_meter bench bird cat dog horse "
    "sheep cow elephant bear zebra giraffe backpack umbrella handbag tie suitcase "
    "frisbee skis snowboard sports_ball kite baseball_bat baseball_glove skateboard "
    "surfboard tennis_racket bottle wine_glass cup fork knife spoon bowl banana "
    "apple sandwich orange broccoli carrot hot_dog pizza donut cake chair couch "
    "potted_plant bed dining_table toilet tv laptop mouse remote keyboard "
    "cell_phone microwave oven toaster sink refrigerator book clock vase scissors "
    "teddy_bear hair_drier toothbrush").split()
TARGET_CLASSES = ["bottle", "vase", "wine_glass"]
BOTTLE_CLASS_ID = 39   # reported in every detection, whatever class fired

# Confidence threshold for drawing/reporting a detection. The firmware
# applies its own CV_CONFIDENCE_THRESHOLD (0.60) before auto mode acts.
CONF_THRESHOLD  = 0.30
NMS_THRESHOLD   = 0.45

# Default model: YOLOv8s exported at the camera's own 480x640, so VGA frames
# go in without being squashed into a square. Chosen 2026-10-02 against
# yolov8n/m, yolo11s/m, yolo26s/m and YOLO-World v2 on camera-like test
# images: with the whole bottle in frame it reached conf >= 0.6 on 7/8 green
# bottles (yolov8n 5/8) and held that under heavy JPEG/blur, at ~29 ms/frame
# on the laptop CPU. No model detects a bottle that overflows the frame (only
# the middle visible) - the grab FSM takes its target while scanning, at
# whole-bottle range, so that is the range that matters.
# Export: YOLO('yolov8s.pt').export(format='onnx', imgsz=(480, 640), simplify=True)
# The input size is read from the model, so --model yolov8n.onnx still works.
DEFAULT_MODEL = "yolov8s_480x640.onnx"

# Target sent to the arm in --auto only when the bottle was seen in at least
# STABLE_MIN of the last STABLE_WINDOW frames; position = median of those.
# The FSM acts on a single snapshot, so one bad frame must not become a target.
STABLE_WINDOW = 5
STABLE_MIN    = 3


# ============================================================
#  YOLOv8n ONNX inference wrapper
# ============================================================
class YoloDetector:
    def __init__(self, model_path: str):
        self.target_ids = [COCO_NAMES.index(c) for c in TARGET_CLASSES]
        self.top_other  = None
        if not ORT_AVAILABLE:
            self.session = None
            return
        opts = ort.SessionOptions()
        opts.graph_optimization_level = \
            ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        # Use QNN EP on Android if available (Snapdragon APU)
        providers = [p for p in ('QNNExecutionProvider', 'CPUExecutionProvider')
                     if p in ort.get_available_providers()]
        self.session = ort.InferenceSession(
            model_path, sess_options=opts, providers=providers)
        inp = self.session.get_inputs()[0]
        self.input_name = inp.name
        self.in_h, self.in_w = int(inp.shape[2]), int(inp.shape[3])
        print(f"[YOLO] Loaded {model_path} ({self.in_w}x{self.in_h}) | "
              f"provider: {self.session.get_providers()[0]}")

    def preprocess(self, frame: np.ndarray):
        """Resize + normalise frame to YOLO input tensor."""
        img = frame if frame.shape[:2] == (self.in_h, self.in_w) \
            else cv2.resize(frame, (self.in_w, self.in_h))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        img = img.astype(np.float32) / 255.0
        img = np.transpose(img, (2, 0, 1))   # HWC → CHW
        return np.expand_dims(img, axis=0)    # add batch dim

    def postprocess(self, output: np.ndarray,
                    orig_w: int, orig_h: int):
        """
        Parse YOLOv8 output (1, 84, 8400) into detections.
        Returns list of {bbox:[x1,y1,x2,y2], conf, class_id}
        scaled to original image dimensions.
        """
        pred = output[0]           # (84, 8400)
        pred = pred.T              # (8400, 84)

        cx, cy, bw, bh = pred[:, 0], pred[:, 1], \
                          pred[:, 2], pred[:, 3]
        class_scores   = pred[:, 4:]   # (8400, 80)

        # Strongest non-target thing in view, for the preview: shows what
        # the model thinks an undetected bottle is (e.g. "cup 0.40").
        all_best = class_scores.max(axis=1)
        k = int(np.argmax(all_best))
        top_id = int(class_scores[k].argmax())
        self.top_other = None
        if COCO_NAMES[top_id] not in TARGET_CLASSES and all_best[k] >= 0.20:
            s = [orig_w / self.in_w, orig_h / self.in_h]
            self.top_other = (COCO_NAMES[top_id], float(all_best[k]),
                              [float((cx[k] - bw[k] / 2) * s[0]), float((cy[k] - bh[k] / 2) * s[1]),
                               float((cx[k] + bw[k] / 2) * s[0]), float((cy[k] + bh[k] / 2) * s[1])])

        # Best score among the accepted bottle-like classes
        tsc       = class_scores[:, self.target_ids]          # (8400, T)
        t_arg     = np.argmax(tsc, axis=1)
        confidences = tsc[np.arange(len(t_arg)), t_arg]

        mask = confidences >= CONF_THRESHOLD
        if not np.any(mask):
            return []

        cx, cy   = cx[mask], cy[mask]
        bw, bh   = bw[mask], bh[mask]
        confs    = confidences[mask]
        labels   = [COCO_NAMES[self.target_ids[j]] for j in t_arg[mask]]

        # Scale from YOLO input coords to original image
        sx = orig_w / self.in_w
        sy = orig_h / self.in_h
        x1 = (cx - bw / 2) * sx
        y1 = (cy - bh / 2) * sy
        x2 = (cx + bw / 2) * sx
        y2 = (cy + bh / 2) * sy

        # NMS
        boxes = np.stack([x1, y1, x2 - x1, y2 - y1], axis=1)
        indices = cv2.dnn.NMSBoxes(
            boxes.tolist(), confs.tolist(),
            CONF_THRESHOLD, NMS_THRESHOLD)

        results = []
        if len(indices) > 0:
            for i in np.array(indices).flatten():
                results.append({
                    'bbox':  [float(x1[i]), float(y1[i]),
                               float(x2[i]), float(y2[i])],
                    'conf':  float(confs[i]),
                    'class': BOTTLE_CLASS_ID,
                    'label': labels[i]
                })
        return results

    def detect(self, frame: np.ndarray):
        """Full pipeline: frame → detections list."""
        if self.session is None:
            return self._mock_detect(frame)
        inp    = self.preprocess(frame)
        output = self.session.run(None, {self.input_name: inp})
        h, w   = frame.shape[:2]
        return self.postprocess(output[0], w, h)

    def _mock_detect(self, frame: np.ndarray):
        """Returns a fake centred detection for testing."""
        h, w = frame.shape[:2]
        return [{
            'bbox': [w*0.35, h*0.2, w*0.65, h*0.8],
            'conf': 0.92,
            'class': BOTTLE_CLASS_ID,
            'label': 'mock'
        }]


# ============================================================
#  Visual-servo depth estimate from how much the bbox fills the frame
#
#  fill = bbox_height_px / frame_height_px  (0 = tiny/far, 1 = fills frame)
#  fwd  = GRIP_DISTANCE_MM * GRIP_FILL_RATIO / fill, clamped to
#         [GRIP_DISTANCE_MM, APPROACH_MAX_MM] — monotonically decreasing
#         as the object gets bigger in-frame, reaching GRIP_DISTANCE_MM
#         exactly when fill == GRIP_FILL_RATIO (the calibrated grip pose).
#
#  No assumption about the object's real-world size — only requires the
#  one-time GRIP_DISTANCE_MM / GRIP_FILL_RATIO calibration above.
# ============================================================
def estimate_depth_from_fill(bbox_h_px: float, frame_h_px: float) -> float:
    fill = bbox_h_px / frame_h_px if frame_h_px > 0 else 0.0
    if fill < 1e-3:
        return APPROACH_MAX_MM
    fwd = GRIP_DISTANCE_MM * GRIP_FILL_RATIO / fill
    return float(min(max(fwd, GRIP_DISTANCE_MM), APPROACH_MAX_MM))


def in_grip_range(bbox_h_px: float, frame_h_px: float) -> bool:
    fill = bbox_h_px / frame_h_px if frame_h_px > 0 else 0.0
    return fill >= GRIP_FILL_RATIO


# ============================================================
#  Pixel + depth → TOOL-frame coordinates (mm)
#
#  Camera frame : X right, Y down, Z forward (standard pinhole).
#  Tool frame   : x FORWARD along the gripper, y LEFT, z UP.
#  The camera is mounted pitched DOWN by CAMERA_TILT_DOWN_DEG
#  relative to the gripper axis   [VERIFY on the real mount].
#
#  The firmware converts tool frame -> base frame itself using the
#  live joint angles (message field "frame":"tip").
#
#  (Bug fixed: the old version returned (right, -down, -forward),
#   i.e. it put the camera's RIGHT axis on the arm's forward axis.)
# ============================================================
# Measured 2026-10-02 (A7, bottle middle at lens height, gripper level): the
# camera points ~19° UP relative to the gripper, not 15° down as first guessed.
CAMERA_TILT_DOWN_DEG = -19.0

def pixel_to_tool(px: float, py: float, depth: float):
    nx = (px - CAM_CX) / CAM_FX
    ny = (py - CAM_CY) / CAM_FY
    x_c, y_c, z_c = nx * depth, ny * depth, depth       # right, down, forward
    t = math.radians(CAMERA_TILT_DOWN_DEG)
    fwd  =  z_c * math.cos(t) - y_c * math.sin(t)
    left = -x_c
    up   = -y_c * math.cos(t) - z_c * math.sin(t)
    # camera frame is centred on the lens; shift to the grasp centre
    return (float(fwd + CAM_POS_FWD_MM), float(left + CAM_POS_LEFT_MM),
            float(up + CAM_POS_UP_MM))


# ============================================================
#  MJPEG stream reader (runs in background thread)
# ============================================================
class MJPEGReader:
    def __init__(self, url: str):
        self.url    = url
        self.frame  = None
        self.seq    = 0          # bumped per decoded frame, so callers can skip repeats
        self.lock   = threading.Lock()
        self._run   = True
        self._thread = threading.Thread(
            target=self._read_loop, daemon=True)
        self._thread.start()

    def _read_loop(self):
        import urllib.request
        while self._run:
            try:
                req = urllib.request.urlopen(self.url, timeout=10)
                buf = bytes()
                while self._run:
                    chunk = req.read(4096)
                    if not chunk:
                        break
                    buf += chunk
                    # Find JPEG boundaries
                    a = buf.find(b'\xff\xd8')
                    b = buf.find(b'\xff\xd9')
                    if a != -1 and b != -1:
                        jpg = buf[a:b+2]
                        buf = buf[b+2:]
                        arr = np.frombuffer(jpg, dtype=np.uint8)
                        frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
                        if frame is not None:
                            with self.lock:
                                self.frame = frame
                                self.seq += 1
            except Exception as e:
                print(f"[CAM] Stream error: {e} — retrying in 2s")
                time.sleep(2)

    def get_frame(self, after: int = 0):
        """(frame, seq) of the newest frame, or (None, after) if there is
        none newer than `after`. Inference is often faster than the camera;
        re-running it on the same frame inflated fps/detection rate and let
        one frame count several times toward a 'stable' target."""
        with self.lock:
            if self.frame is None or self.seq == after:
                return None, after
            return self.frame.copy(), self.seq

    def stop(self):
        self._run = False


# ============================================================
#  Main async pipeline
# ============================================================
async def run_pipeline(cam_url: str, arm_ws_url: str,
                       model_path: str, show_preview: bool,
                       arm_motion: bool):
    detector = YoloDetector(model_path)
    save_dir = Path(__file__).resolve().parent / "captures"
    print(f"[CV] target classes: {', '.join(TARGET_CLASSES)}  conf >= {CONF_THRESHOLD}")
    reader   = MJPEGReader(cam_url)
    if show_preview:
        # Resizable, aspect-kept, no Qt toolbar/status bar: the capture agent
        # (cv_capture_agent.py) fullscreens this window to record it, and a bare
        # imshow window stays a fixed 640x480 in a grey field.
        cv2.namedWindow("ARACHNAE CV", cv2.WINDOW_NORMAL | cv2.WINDOW_KEEPRATIO | cv2.WINDOW_GUI_NORMAL)
    print(f"[CV] Connecting to arm at {arm_ws_url} ...")

    # The arm cancels auto mode and homes whenever this connection drops
    # (WStype_DISCONNECTED in the firmware), and the AP link does hiccup: a
    # ~10 s stall once killed a dry run with a keepalive timeout. Dry run
    # reconnects and carries on; --auto stops, so auto mode is never
    # silently re-enabled behind the arm's back.
    quit_requested = False
    first_shown    = False    # cv_capture_agent.py waits for this line before recording
    while not quit_requested:
        try:
            async with websockets.connect(arm_ws_url,
                                           ping_interval=5,
                                           ping_timeout=10) as ws:
                print("[CV] WebSocket connected")

                # DRY RUN by default: targets are printed but the arm is NOT put in
                # auto mode. Pass --auto to let the arm move.
                if arm_motion:
                    print("[CV] !!! ARM MOTION ENABLED — sending mode=auto !!!")
                    await ws.send(json.dumps({"type": "mode", "value": "auto"}))
                else:
                    print("[CV] DRY RUN — not enabling auto mode (use --auto)")

                frame_count   = 0
                last_send     = 0.0
                SEND_HZ       = 5.0   # max target sends per second
                grip_streak   = 0     # consecutive frames in grip range (debounce)
                recent        = deque(maxlen=STABLE_WINDOW)   # per frame: (x, y, z, conf) or None
                GRIP_STREAK_N = 3
                last_obs      = 0.0
                fps, t_prev   = 0.0, 0.0

                last_seq = 0
                while True:
                    frame, last_seq = reader.get_frame(last_seq)
                    if frame is None:
                        await asyncio.sleep(0.005)
                        continue

                    raw = frame.copy()          # unannotated, for 's' = save frame
                    frame_h = frame.shape[0]

                    # ── Detection ───────────────────────────────────
                    t_inf = time.time()
                    detections = detector.detect(frame)
                    infer_ms = (time.time() - t_inf) * 1000.0
                    frame_count += 1
                    now = time.time()
                    fps = 0.9 * fps + 0.1 * (1.0 / max(now - t_prev, 1e-3)) if t_prev else 0.0
                    t_prev = now
                    obs = {"type": "cv_obs", "det": False, "auto": arm_motion,
                           "fps": round(fps, 1), "infer_ms": round(infer_ms)}

                    recent.append(None)
                    if detections:
                        # Take highest-confidence bottle
                        best = max(detections, key=lambda d: d['conf'])
                        x1, y1, x2, y2 = best['bbox']
                        cx = (x1 + x2) / 2
                        cy = (y1 + y2) / 2
                        bh = y2 - y1
                        fill = bh / frame_h if frame_h > 0 else 0.0

                        depth_mm = estimate_depth_from_fill(bh, frame_h)   # lens -> bottle front
                        wx, wy, wz = pixel_to_tool(cx, cy, depth_mm + BOTTLE_RADIUS_MM)

                        if in_grip_range(bh, frame_h):
                            grip_streak += 1
                            if grip_streak == GRIP_STREAK_N:
                                print(f"[CV] IN GRIP RANGE (fill={fill:.2f} >= {GRIP_FILL_RATIO})")
                        else:
                            grip_streak = 0

                        # Observation for the arm's /test CV panel: sent in dry run
                        # too, and never acted on by the firmware.
                        obs.update({"det": True, "n": len(detections),
                                    "x": round(wx, 1), "y": round(wy, 1), "z": round(wz, 1),
                                    "confidence": round(best['conf'], 3), "fill": round(fill, 4),
                                    "depth": round(depth_mm), "px": round(cx), "py": round(cy),
                                    "bw": round(x2 - x1), "bh": round(bh),
                                    "grip": bool(in_grip_range(bh, frame_h))})

                        recent[-1] = (wx, wy, wz, best['conf'])
                        hits = [r for r in recent if r is not None]
                        stable = len(hits) >= STABLE_MIN

                        # ── Send to arm (rate-limited, stable detections only) ──
                        if stable and now - last_send >= 1.0 / SEND_HZ:
                            sx_, sy_, sz_, sc_ = np.median(np.array(hits), axis=0)
                            payload = {
                                "type":       "target",
                                "frame":      "tip",     # firmware transforms to base frame with FK
                                "x":          round(float(sx_), 1),
                                "y":          round(float(sy_), 1),
                                "z":          round(float(sz_), 1),
                                "confidence": round(float(sc_), 3),
                                "metric":     True
                            }
                            if arm_motion:
                                await ws.send(json.dumps(payload))
                            last_send = now
                            print(f"[CV] {'Sent' if arm_motion else 'DRY'} target (tool frame, median of {len(hits)}): "
                                  f"fwd={sx_:.0f} left={sy_:.0f} up={sz_:.0f}mm "
                                  f"fill={fill:.2f} conf={sc_:.2f}")

                        # ── Preview overlay ───────────────────────
                        if show_preview:
                            x1i, y1i, x2i, y2i = [int(v) for v in [x1,y1,x2,y2]]
                            color = (0, 200, 255) if in_grip_range(bh, frame_h) else (0, 255, 0)
                            cv2.rectangle(frame, (x1i, y1i), (x2i, y2i), color, 2)
                            cv2.putText(frame,
                                f"{best['label']} {best['conf']:.2f} fill={fill:.2f} d={depth_mm:.0f}mm",
                                (x1i, y1i - 8),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                                color, 1)

                    if now - last_obs >= 1.0 / SEND_HZ:
                        await ws.send(json.dumps(obs))
                        last_obs = now

                    if show_preview:
                        other = detector.top_other
                        if other:
                            ox1, oy1, ox2, oy2 = [int(v) for v in other[2]]
                            cv2.rectangle(frame, (ox1, oy1), (ox2, oy2), (150, 150, 150), 1)
                            cv2.putText(frame, f"(not target) {other[0]} {other[1]:.2f}",
                                        (ox1, max(oy2 - 6, 12)),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, (150, 150, 150), 1)
                        cv2.putText(frame, f"frame {frame_count}",
                                    (8, 16),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                                    (200, 200, 200), 1)
                        cv2.imshow("ARACHNAE CV", frame)
                        if not first_shown:
                            print("[CV] preview: first frame shown")
                            first_shown = True
                        key = cv2.waitKey(1) & 0xFF
                        if key == ord('q'):
                            quit_requested = True
                            break
                        if key == ord('s'):   # save the raw frame for offline analysis
                            save_dir.mkdir(exist_ok=True)
                            fn = save_dir / f"frame_{time.strftime('%H%M%S')}_{frame_count}.jpg"
                            cv2.imwrite(str(fn), raw)
                            print(f"[CV] saved {fn}")

                    # Handle incoming arm messages
                    try:
                        msg = await asyncio.wait_for(ws.recv(), timeout=0.001)
                        data = json.loads(msg)
                        if data.get("type") == "log":      # skip the 5 Hz status broadcasts
                            print(f"[ARM] {data.get('msg')}")
                    except asyncio.TimeoutError:
                        pass

                    await asyncio.sleep(0.01)

        except (websockets.ConnectionClosed, OSError) as e:
            if arm_motion:
                print(f"[CV] Lost the arm connection ({e}). The arm has left auto mode "
                      "and is going home. Not reconnecting in --auto: restart the pipeline.")
                break
            print(f"[CV] Lost the arm connection ({e}) - reconnecting in 1 s")
            await asyncio.sleep(1)

    reader.stop()
    cv2.destroyAllWindows()


# ============================================================
if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description="PROJET ARACHNAE — CV Pipeline")
    ap.add_argument("--cam",   default="http://192.168.4.1:81/stream",
                    help="MJPEG stream URL (port 81; 192.168.4.1 when the arm runs its own AP)")
    ap.add_argument("--arm",   default="ws://192.168.4.1:8765",
                    help="Arm WebSocket URL")
    ap.add_argument("--auto", action="store_true",
                    help="ENABLE ARM MOTION (sends mode=auto + targets). Default is dry-run.")
    ap.add_argument("--model", default=str(Path(__file__).resolve().parent / DEFAULT_MODEL),
                    help=f"YOLOv8 ONNX model path (default {DEFAULT_MODEL}; yolov8n.onnx is the old, faster one)")
    ap.add_argument("--preview", action="store_true",
                    help="Show OpenCV preview window (PC only)")
    # Calibration overrides, so values can be tried without editing the file.
    ap.add_argument("--grip-dist", type=float, help=f"GRIP_DISTANCE_MM (default {GRIP_DISTANCE_MM})")
    ap.add_argument("--grip-fill", type=float, help=f"GRIP_FILL_RATIO (default {GRIP_FILL_RATIO})")
    ap.add_argument("--tilt", type=float, help=f"CAMERA_TILT_DOWN_DEG (default {CAMERA_TILT_DOWN_DEG})")
    ap.add_argument("--conf", type=float, help=f"detection threshold (default {CONF_THRESHOLD})")
    ap.add_argument("--classes", help=f"comma-separated COCO classes to accept (default {','.join(TARGET_CLASSES)})")
    args = ap.parse_args()
    if args.conf is not None:      CONF_THRESHOLD = args.conf
    if args.classes:
        TARGET_CLASSES = [c.strip() for c in args.classes.split(",")]
        bad = [c for c in TARGET_CLASSES if c not in COCO_NAMES]
        if bad: ap.error(f"unknown class {bad}; names use underscores, e.g. wine_glass")
    if args.grip_dist is not None: GRIP_DISTANCE_MM = args.grip_dist
    if args.grip_fill is not None: GRIP_FILL_RATIO = args.grip_fill
    if args.tilt is not None:      CAMERA_TILT_DOWN_DEG = args.tilt
    print(f"[CV] GRIP_DISTANCE_MM={GRIP_DISTANCE_MM}  GRIP_FILL_RATIO={GRIP_FILL_RATIO}  "
          f"CAMERA_TILT_DOWN_DEG={CAMERA_TILT_DOWN_DEG}")

    if not Path(args.model).exists() and ORT_AVAILABLE:
        print(f"[CV] Model not found: {args.model}")
        print("[CV] Falling back to mock detector")

    asyncio.run(run_pipeline(
        args.cam, args.arm, args.model, args.preview, args.auto))

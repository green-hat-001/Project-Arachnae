#!/usr/bin/env python3
"""
============================================================
 PROJET ARACHNAE — Phone-Side Computer Vision Pipeline
 Runs on Android (Termux + pip) or any Python 3.9+ machine.

 Pipeline:
   1. Pull MJPEG stream from ESP32-CAM over WiFi
   2. Run YOLOv8n (ONNX) to detect 'bottle' class
   3. Estimate depth from bounding-box size heuristic
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

 Usage:
   python cv_pipeline.py \
     --cam  http://192.168.x.x/stream \
     --arm  ws://192.168.x.x:8765 \
     --model yolov8n.onnx
============================================================
"""

import argparse
import asyncio
import json
import math
import threading
import time
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
# Camera intrinsics — OV2640 on XIAO ESP32S3 Sense at SVGA (800×600)
# Estimated from ~55° horizontal FOV. Calibrate with a checkerboard
# for better depth accuracy (see README).
CAM_FX  = 768.0
CAM_FY  = 782.0
CAM_CX  = 400.0
CAM_CY  = 300.0
CAM_W   = 800
CAM_H   = 600

# Known bottle height (mm) — used for monocular depth estimate
BOTTLE_HEIGHT_MM = 210.0   # standard 500 mL water bottle ≈ 21 cm

# COCO class index for 'bottle'
BOTTLE_CLASS_ID = 39

# Confidence threshold
CONF_THRESHOLD  = 0.45
NMS_THRESHOLD   = 0.45

# YOLO input size — 640 gives better accuracy on the higher-res SVGA source.
# Export model with: YOLO('yolov8n.pt').export(format='onnx', imgsz=640)
# If inference is too slow on your phone, drop back to 320.
YOLO_INPUT_W = 640
YOLO_INPUT_H = 640


# ============================================================
#  YOLOv8n ONNX inference wrapper
# ============================================================
class YoloDetector:
    def __init__(self, model_path: str):
        if not ORT_AVAILABLE:
            self.session = None
            return
        providers = ['CPUExecutionProvider']
        opts = ort.SessionOptions()
        opts.graph_optimization_level = \
            ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        # Use QNN EP on Android if available (Snapdragon APU)
        try:
            providers = ['QNNExecutionProvider', 'CPUExecutionProvider']
        except Exception:
            pass
        self.session = ort.InferenceSession(
            model_path, sess_options=opts, providers=providers)
        self.input_name = self.session.get_inputs()[0].name
        print(f"[YOLO] Loaded {model_path} | "
              f"provider: {self.session.get_providers()[0]}")

    def preprocess(self, frame: np.ndarray):
        """Resize + normalise frame to YOLO input tensor."""
        img = cv2.resize(frame, (YOLO_INPUT_W, YOLO_INPUT_H))
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
        class_ids      = np.argmax(class_scores, axis=1)
        confidences    = class_scores[np.arange(len(class_ids)),
                                      class_ids]

        # Filter by class (bottle) and confidence
        mask = (class_ids == BOTTLE_CLASS_ID) & \
               (confidences >= CONF_THRESHOLD)
        if not np.any(mask):
            return []

        cx, cy   = cx[mask], cy[mask]
        bw, bh   = bw[mask], bh[mask]
        confs    = confidences[mask]

        # Scale from YOLO input coords to original image
        sx = orig_w / YOLO_INPUT_W
        sy = orig_h / YOLO_INPUT_H
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
                    'class': BOTTLE_CLASS_ID
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
            'class': BOTTLE_CLASS_ID
        }]


# ============================================================
#  Monocular depth estimation from bounding-box height
#
#  depth = (f_y × BOTTLE_HEIGHT_MM) / bbox_height_pixels
#
#  Accuracy: ±20% at <600mm, degrades at distance.
#  For better depth: add an HC-SR04 ultrasonic, or use
#  the stereo baseline from a second camera.
# ============================================================
def estimate_depth(bbox_h_px: float) -> float:
    if bbox_h_px < 1:
        return 9999.0
    return (CAM_FY * BOTTLE_HEIGHT_MM) / bbox_h_px


# ============================================================
#  Pixel + depth → world coordinates (mm)
#  Camera frame: X right, Y down, Z forward.
#  We assume camera is mounted on gripper pointing forward,
#  slightly downward at -15° pitch (adjustable).
# ============================================================
CAMERA_TILT_DEG = -15.0   # camera tilted down from horizontal

def pixel_to_world(px: float, py: float, depth: float):
    """Returns (x_mm, y_mm, z_mm) in arm base frame."""
    # Normalised image coords
    nx = (px - CAM_CX) / CAM_FX
    ny = (py - CAM_CY) / CAM_FY

    # Camera-frame 3D point
    p_cam = np.array([nx * depth, ny * depth, depth])

    # Rotation from camera to arm-tip frame (camera tilted down)
    tilt  = math.radians(CAMERA_TILT_DEG)
    Rcam  = np.array([
        [1,           0,            0],
        [0,  math.cos(tilt), -math.sin(tilt)],
        [0,  math.sin(tilt),  math.cos(tilt)]
    ])
    p_tip = Rcam @ p_cam

    # Note: transform from tip → base requires current joint
    # angles (FK). The ESP32 does this transform on its side
    # (pixelToWorld in kinematics.h). We send metric tip-frame
    # coords here; the arm adds its own FK offset.
    return float(p_tip[0]), float(-p_tip[1]), float(-p_tip[2])


# ============================================================
#  MJPEG stream reader (runs in background thread)
# ============================================================
class MJPEGReader:
    def __init__(self, url: str):
        self.url    = url
        self.frame  = None
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
            except Exception as e:
                print(f"[CAM] Stream error: {e} — retrying in 2s")
                time.sleep(2)

    def get_frame(self):
        with self.lock:
            return self.frame.copy() if self.frame is not None \
                   else None

    def stop(self):
        self._run = False


# ============================================================
#  Main async pipeline
# ============================================================
async def run_pipeline(cam_url: str, arm_ws_url: str,
                       model_path: str, show_preview: bool):
    detector = YoloDetector(model_path)
    reader   = MJPEGReader(cam_url)
    print(f"[CV] Connecting to arm at {arm_ws_url} ...")

    async with websockets.connect(arm_ws_url,
                                   ping_interval=5,
                                   ping_timeout=10) as ws:
        print("[CV] WebSocket connected")

        # Tell arm to enter auto mode
        await ws.send(json.dumps({"type": "mode", "value": "auto"}))

        frame_count = 0
        last_send   = 0.0
        SEND_HZ     = 5.0   # max target sends per second

        while True:
            frame = reader.get_frame()
            if frame is None:
                await asyncio.sleep(0.05)
                continue

            # ── Detection ───────────────────────────────────
            detections = detector.detect(frame)
            frame_count += 1

            if detections:
                # Take highest-confidence bottle
                best = max(detections, key=lambda d: d['conf'])
                x1, y1, x2, y2 = best['bbox']
                cx = (x1 + x2) / 2
                cy = (y1 + y2) / 2
                bh = y2 - y1

                depth_mm = estimate_depth(bh)
                wx, wy, wz = pixel_to_world(cx, cy, depth_mm)

                # ── Send to arm (rate-limited) ────────────
                now = time.time()
                if now - last_send >= 1.0 / SEND_HZ:
                    payload = {
                        "type":       "target",
                        "x":          round(wx, 1),
                        "y":          round(wy, 1),
                        "z":          round(wz, 1),
                        "confidence": round(best['conf'], 3),
                        "metric":     True
                    }
                    await ws.send(json.dumps(payload))
                    last_send = now
                    print(f"[CV] Sent target: "
                          f"x={wx:.0f} y={wy:.0f} z={wz:.0f}mm "
                          f"conf={best['conf']:.2f}")

                # ── Preview overlay ───────────────────────
                if show_preview:
                    x1i, y1i, x2i, y2i = [int(v) for v in [x1,y1,x2,y2]]
                    cv2.rectangle(frame, (x1i, y1i), (x2i, y2i),
                                  (0, 255, 0), 2)
                    cv2.putText(frame,
                        f"bottle {best['conf']:.2f} d={depth_mm:.0f}mm",
                        (x1i, y1i - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        (0, 255, 0), 1)

            if show_preview:
                cv2.putText(frame, f"frame {frame_count}",
                            (8, 16),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                            (200, 200, 200), 1)
                cv2.imshow("ARACHNAE CV", frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

            # Handle incoming arm messages
            try:
                msg = await asyncio.wait_for(ws.recv(), timeout=0.001)
                data = json.loads(msg)
                print(f"[ARM] {data}")
            except asyncio.TimeoutError:
                pass

            await asyncio.sleep(0.01)

    reader.stop()
    cv2.destroyAllWindows()


# ============================================================
if __name__ == "__main__":
    ap = argparse.ArgumentParser(
        description="PROJET ARACHNAE — CV Pipeline")
    ap.add_argument("--cam",   default="http://192.168.1.100/stream",
                    help="ESP32-CAM MJPEG stream URL")
    ap.add_argument("--arm",   default="ws://192.168.1.101:8765",
                    help="Main ESP32 WebSocket URL")
    ap.add_argument("--model", default="yolov8n.onnx",
                    help="YOLOv8n ONNX model path")
    ap.add_argument("--preview", action="store_true",
                    help="Show OpenCV preview window (PC only)")
    args = ap.parse_args()

    if not Path(args.model).exists() and ORT_AVAILABLE:
        print(f"[CV] Model not found: {args.model}")
        print("[CV] Falling back to mock detector")

    asyncio.run(run_pipeline(
        args.cam, args.arm, args.model, args.preview))

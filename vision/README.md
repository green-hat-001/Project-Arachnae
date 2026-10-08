# Vision

Laptop-side computer vision for the arm. How it works, the calibration and the findings are in
[`docs/VISION.md`](../docs/VISION.md).

| File | What |
|---|---|
| `cv_pipeline_visual_servo.py` | current pipeline: MJPEG stream → YOLOv8s → 3D point relative to the grasp centre → WebSocket target. Dry run unless `--auto`. |
| `cv_pipeline.py` | the older version (fixed-bottle-height depth) |
| `cv_capture_agent.py` | records proof footage of the pipeline's preview when `/test` asks for it; also serves its own record button on `http://127.0.0.1:8770` |
| `yolov8s_480x640.onnx` | default model, exported at the camera's aspect (LFS) |
| `yolov8n.onnx` | smaller, faster model (`--model yolov8n.onnx`) (LFS) |

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python cv_pipeline_visual_servo.py --cam http://192.168.4.1:81/stream --arm ws://192.168.4.1:8765 --preview
```

`cv_capture_agent.py` is written for my laptop (niri on Wayland, gpu-screen-recorder) and saves to `vision/captures/`.

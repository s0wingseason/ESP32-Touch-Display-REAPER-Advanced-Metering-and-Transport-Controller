"""
MeterBridge Screen Stream Viewer
=================================
Pulls BMP screenshots from the ESP32 (port 8099) and re-serves them
as an MJPEG stream you can watch in any browser or VLC.

Usage:
    python screen_stream.py
    python screen_stream.py --ip 192.168.1.180 --fps 12 --quality 80
    python screen_stream.py --fps 6 --quality 95   # slow but sharp

Target FPS options: 1 / 2 / 4 / 6 / 8 / 10 / 12
(Practical max is ~2-4 fps over WiFi BMP due to 768KB transfer size;
 higher settings are accepted but will be capped by network throughput)

Install deps once:
    pip install flask requests pillow

Open in browser:
    http://localhost:5000/
"""

import time
import io
import sys
import threading
import argparse
import requests
from PIL import Image

try:
    from flask import Flask, Response
except ImportError:
    print("[ERROR] Flask not installed. Run: pip install flask requests pillow")
    sys.exit(1)

# ── Config ──────────────────────────────────────────────────────────────────
DEFAULT_IP      = "192.168.1.180"
DEFAULT_FPS     = 4
DEFAULT_QUALITY = 75     # JPEG quality 10-95
DEFAULT_PORT    = 5000
SCREENSHOT_PORT = 8099   # ESP32 BMP server
TIMEOUT_S       = 8      # BMP is ~768KB; WiFi transfer takes 2-4s

# ── State ────────────────────────────────────────────────────────────────────
_frame_lock  = threading.Lock()
_last_jpeg   = None
_stats       = {"fetched": 0, "errors": 0, "avg_ms": 0}

app = Flask(__name__)

# ── Frame fetcher (background thread) ────────────────────────────────────────
def _fetch_loop(ip: str, fps: int, quality: int):
    global _last_jpeg
    interval = 1.0 / fps
    url      = f"http://{ip}:{SCREENSHOT_PORT}/"
    consecutive_errors = 0

    print(f"[STREAM] Fetching from {url} at {fps} fps, quality={quality}")
    while True:
        t0 = time.time()
        try:
            # Don't use Session — ESP32 sends Connection:close, so pooling
            # causes stale-connection timeouts on the next request.
            r    = requests.get(url, timeout=TIMEOUT_S)
            img  = Image.open(io.BytesIO(r.content))
            buf  = io.BytesIO()
            img.save(buf, format="JPEG", quality=quality)
            jpeg = buf.getvalue()
            elapsed_ms = int((time.time() - t0) * 1000)
            consecutive_errors = 0  # reset on success

            with _frame_lock:
                _last_jpeg           = jpeg
                _stats["fetched"]   += 1
                n = _stats["fetched"]
                _stats["avg_ms"] = (_stats["avg_ms"] * (n-1) + elapsed_ms) // n

        except Exception as e:
            consecutive_errors += 1
            with _frame_lock:
                _stats["errors"] += 1
            # Only log every 5th error to avoid spam
            if consecutive_errors <= 3 or consecutive_errors % 10 == 0:
                print(f"[STREAM] Fetch error ({consecutive_errors}): {e}")
            # Back off on repeated failures (device rebooting, WiFi down)
            if consecutive_errors > 3:
                time.sleep(min(5.0, consecutive_errors * 0.5))
                continue

        elapsed = time.time() - t0
        time.sleep(max(0.0, interval - elapsed))


# ── Flask routes ──────────────────────────────────────────────────────────────
@app.route("/")
def index():
    s = _stats
    return f"""<!DOCTYPE html>
<html>
<head>
  <title>MeterBridge Live View</title>
  <style>
    body {{ margin:0; background:#0a0a0a; display:flex; flex-direction:column;
           align-items:center; justify-content:center; min-height:100vh;
           font-family:monospace; color:#0f0; }}
    img {{ max-width:100%; border:1px solid #222; }}
    #info {{ margin-top:8px; font-size:12px; color:#555; }}
  </style>
</head>
<body>
  <img src="/stream" alt="MeterBridge display">
  <div id="info">
    MeterBridge @ {_stats.get("fetched",0)} frames &nbsp;|&nbsp;
    avg {_stats.get("avg_ms",0)} ms/frame &nbsp;|&nbsp;
    errors: {_stats.get("errors",0)}
    &nbsp;&mdash;&nbsp;
    <a href="/?fps=6"  style="color:#0f0">6fps</a> /
    <a href="/?fps=12" style="color:#0f0">12fps</a> /
    <a href="/?fps=24" style="color:#0f0">24fps</a>
  </div>
</body>
</html>"""


@app.route("/stream")
def stream():
    def _generate():
        while True:
            with _frame_lock:
                frame = _last_jpeg
            if frame:
                yield (b"--frame\r\n"
                       b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n")
            time.sleep(0.01)   # 100Hz poll of _last_jpeg
    return Response(_generate(),
                    mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/snapshot")
def snapshot():
    """Single JPEG download."""
    with _frame_lock:
        frame = _last_jpeg
    if frame:
        return Response(frame, mimetype="image/jpeg")
    return "No frame yet", 503


# ── Entry point ───────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="MeterBridge screen stream viewer")
    parser.add_argument("--ip",      default=DEFAULT_IP,
                        help=f"ESP32 IP address (default: {DEFAULT_IP})")
    parser.add_argument("--fps",     type=int, default=DEFAULT_FPS,
                        choices=[1, 2, 4, 6, 8, 10, 12],
                        help="Target frames per second (default 4; practical max ~4 over WiFi)")
    parser.add_argument("--quality", type=int, default=DEFAULT_QUALITY,
                        help="JPEG quality 10-95 (default 75)")
    parser.add_argument("--port",    type=int, default=DEFAULT_PORT,
                        help=f"Local HTTP port (default: {DEFAULT_PORT})")
    args = parser.parse_args()

    # Background fetch thread
    t = threading.Thread(target=_fetch_loop,
                         args=(args.ip, args.fps, args.quality),
                         daemon=True)
    t.start()

    print(f"\n  MeterBridge Live Stream")
    print(f"  ========================")
    print(f"  Device : http://{args.ip}:{SCREENSHOT_PORT}/")
    print(f"  Viewer : http://localhost:{args.port}/")
    print(f"  FPS    : {args.fps}  |  Quality: {args.quality}")
    print(f"\n  Open http://localhost:{args.port}/ in your browser\n")

    app.run(host="0.0.0.0", port=args.port, threaded=True)


if __name__ == "__main__":
    main()

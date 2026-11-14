"""Owner-authorized timed HD diagnostics at XMB, with OBS frame evidence.

Uses the existing guarded helper inbox, not arbitrary memory writes or app
navigation. Default sequence is stock recovery and progressive AVI control.
No capture-card live-register reads; those can themselves disturb capture.
"""
import argparse
from datetime import datetime, timezone
import ftplib
import hashlib
import html
import io
import json
from pathlib import Path
import re
import secrets
import shutil
import struct
import subprocess
import time
from urllib.request import urlopen
from urllib.error import HTTPError

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "app/source/hd540_protocol.h").read_text()
MAGIC = int(re.search(r"HD540_MAGIC UINT32_C\(0x([0-9a-fA-F]+)\)", HEADER)[1], 16)
READY = re.search(r'#define HD540_READY "([^"]+)"', HEADER)[1]
REQUEST = "/dev_hdd0/tmp/crt_hd540.request"
REPLY = "/dev_hdd0/tmp/crt_hd540.reply"
TERMINAL = {0, 7, 8, 9}
ACTIONS = {"roundtrip": 4, "720p": 6, "candidate": 2}
HELPER = "/dev_hdd0/plugins/vsh_240p.sprx"


def parse_slots(page):
    rows = re.findall(r'<tr>\s*<td[^>]*>\s*([0-6])\s*<td[^>]*>\s*([^<]*)(.*?)</tr>',
                      page, re.I | re.S)
    slots = {}
    for slot, name, rest in rows:
        paths = re.findall(r'href=["\']/loadprx\.ps3([^"\']+)', rest)
        slots[int(slot)] = {"name": html.unescape(name.strip()),
                            "path": html.unescape(paths[0]) if paths else None}
    if set(slots) != set(range(7)):
        raise RuntimeError("Unrecognized live VSH slot listing; no plugin load")
    return slots


def image_stats(rgb):
    # Exclude stream borders. Report objective pixel evidence, not readability.
    h, w = rgb.shape[:2]
    crop = rgb[h // 10:h * 9 // 10, w // 10:w * 9 // 10].astype(float)
    luma = crop @ np.array([0.2126, 0.7152, 0.0722])
    mean, spread = float(luma.mean()), float(luma.std())
    lit = float((luma > 12).mean())
    return {"mean_luma": round(mean, 3), "std_luma": round(spread, 3),
            "lit_fraction": round(lit, 5),
            "picture_detected": bool(mean > 8 and spread > 2 and lit > .04)}


def summarize_active(rows):
    active = [r for r in rows if r["state"] == 3]
    if not active:
        return {"samples": 0, "picture_fraction": None, "picture_detected": False}
    # Allow two seconds for the OBS pipeline after helper readiness.
    mature = [r for r in active if r["elapsed"] >= active[0]["elapsed"] + 2]
    good = sum(r["picture_detected"] for r in mature)
    return {"samples": len(mature), "picture_fraction": good / len(mature) if mature else None,
            "picture_detected": bool(len(mature) >= 3 and good / len(mature) >= .8)}


class Capture:
    def __init__(self, folder):
        self.folder = folder
        self.latest = folder / "latest.jpg"
        self.log = (folder / "ffmpeg.log").open("wb")
        self.process = subprocess.Popen([
            shutil.which("ffmpeg") or "ffmpeg", "-hide_banner", "-nostdin",
            "-loglevel", "warning", "-f", "dshow", "-rtbufsize", "128M",
            "-video_size", "2560x1440", "-framerate", "60", "-pixel_format", "nv12",
            "-i", "video=OBS Virtual Camera", "-an", "-vf", "fps=4,scale=640:360",
            "-q:v", "4", "-update", "1", "-atomic_writing", "1", "-y", str(self.latest)
        ], stdout=subprocess.DEVNULL, stderr=self.log,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def frame(self, destination):
        if self.process.poll() is not None:
            raise RuntimeError("OBS recorder stopped; see ffmpeg.log")
        before = self.latest.stat()
        if time.time() - before.st_mtime > 3:
            raise RuntimeError("OBS frame is stale; stopping tests")
        data = self.latest.read_bytes()
        with Image.open(io.BytesIO(data)) as im:
            rgb = np.asarray(im.convert("RGB"))
        destination.write_bytes(data)
        return image_stats(rgb), before.st_mtime_ns

    def start(self):
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if self.latest.exists():
                time.sleep(1)
                return
            if self.process.poll() is not None:
                break
            time.sleep(.25)
        raise RuntimeError("No OBS frames; start OBS Virtual Camera and see ffmpeg.log")

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
        self.log.close()


class Console:
    def __init__(self, host, folder):
        self.host, self.folder = host, folder
        self.ftp = ftplib.FTP()
        self.ftp.connect(host, 21, timeout=5)
        self.ftp.login()
        self.pending = None

    def read(self, path, optional=False):
        out = io.BytesIO()
        try:
            self.ftp.retrbinary("RETR " + path, out.write)
        except ftplib.error_perm as error:
            if optional and str(error).startswith("550"):
                return None
            raise
        return out.getvalue()

    def reply(self):
        data = self.read(REPLY, True)
        if not data:
            return None
        # Helper writes its reply in place; retry a partial poll, never accept it.
        if len(data) != 16:
            return None
        magic, seq, state, detail = struct.unpack(">4I", data)
        if magic != MAGIC or state > 10:
            raise RuntimeError("Unexpected helper reply ABI/state")
        return {"sequence": seq, "state": state, "detail": detail}

    def idle(self, check_reply=True):
        for path in ("/dev_hdd0/tmp/240p_app.active", "/dev_hdd0/tmp/240p_enforcer.on"):
            if self.read(path, True) is not None:
                raise RuntimeError("Exit app and disarm SD enforcement first: " + path)
        reply = self.reply() if check_reply else None
        if reply and (reply["state"] not in TERMINAL or reply["state"] == 9):
            raise RuntimeError("Helper busy or recovery failed; no new test sent")

    def verify_files(self):
        helper = ROOT / "build/hd540/plugin/vsh_240p.sprx"
        if not helper.exists():
            raise RuntimeError("Build the matching experimental helper first")
        expected = hashlib.sha256(helper.read_bytes()).digest()
        for path in ("/dev_hdd0/plugins/vsh_240p.sprx",
                     "/dev_hdd0/game/PS240P001/USRDIR/vsh_240p.sprx"):
            if hashlib.sha256(self.read(path)).digest() != expected:
                raise RuntimeError("Installed helper differs from local build")

    def slots(self):
        with urlopen(f"http://{self.host}/vshplugin.ps3mapi", timeout=8) as response:
            page = response.read().decode(errors="replace")
        return parse_slots(page)

    def ensure_helper(self):
        slots = self.slots()
        resident = [s for s in slots.values() if s["name"] == "vsh_240p"]
        # No resident project helper means prior ready/reply files may be
        # from another ABI before a verified deployment/restart. Loading the
        # checked helper discards those files before it starts polling.
        self.idle(check_reply=bool(resident))
        self.verify_files()
        if resident:
            if len(resident) != 1 or resident[0]["path"] != HELPER:
                raise RuntimeError("Unexpected resident helper identity; refusing replacement")
            return
        free = [s for s in range(2, 7) if slots[s]["name"] == "NULL" and slots[s]["path"] is None]
        if not free:
            raise RuntimeError("No empty project slot; no plugin unloaded")
        old = self.read("/dev_hdd0/tmp/vsh_240p.log", True)
        print(f"Loading verified helper into empty VSH slot {free[0]}", flush=True)
        with urlopen(f"http://{self.host}/vshplugin.ps3mapi?load_slot={free[0]}&prx={HELPER}",
                     timeout=15) as response:
            (self.folder / "helper-load-response.html").write_bytes(response.read())
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            time.sleep(.5)
            if self.read(READY, True) is not None and self.read("/dev_hdd0/tmp/vsh_240p.log", True) != old:
                if self.slots()[free[0]] == {"name": "vsh_240p", "path": HELPER}:
                    print("Fresh helper startup and live slot verified", flush=True)
                    return
        raise RuntimeError("Helper load not verified; no timing tests sent")

    def preflight(self):
        self.idle()
        self.verify_files()
        resident = [s for s in self.slots().values() if s["name"] == "vsh_240p"]
        if resident != [{"name": "vsh_240p", "path": HELPER}] or self.read(READY, True) is None:
            raise RuntimeError("Matching resident helper not ready")

    def publish(self, action, cancel=False):
        if not cancel:
            self.preflight()
        old = self.read(REQUEST, True)
        reply = self.reply()
        if old and not cancel:
            if len(old) != 16 or not reply or struct.unpack(">4I", old)[:2] != (MAGIC, reply["sequence"]):
                raise RuntimeError("Previous inbox is not acknowledged; no replacement")
        seq = secrets.randbelow(0xFFFFFFFE) + 1
        if old and len(old) == 16 and seq == struct.unpack(">4I", old)[1]:
            seq = seq % 0xFFFFFFFF + 1
        data = struct.pack(">4I", MAGIC, seq, action, 0 if cancel else 1)
        tmp, backup = f"{REQUEST}.auto-{seq}.tmp", f"{REQUEST}.auto-{seq}.prev"
        if self.read(tmp, True) is not None or self.read(backup, True) is not None:
            raise RuntimeError("Unique inbox staging path already exists")
        self.ftp.storbinary("STOR " + tmp, io.BytesIO(data))
        if self.read(tmp) != data:
            raise RuntimeError("Inbox staged readback mismatch")
        if self.read(REQUEST, True) != old:
            raise RuntimeError("Inbox changed while staging; no replacement")
        if old is not None:
            (self.folder / f"inbox-before-{seq}.bin").write_bytes(old)
            self.ftp.rename(REQUEST, backup)
        if not cancel:
            self.pending = seq  # Rename may succeed even if its FTP acknowledgement is lost.
        try:
            self.ftp.rename(tmp, REQUEST)
        except ftplib.error_perm:
            if old is not None:
                self.ftp.rename(backup, REQUEST)
            if not cancel:
                self.pending = None
            raise
        if self.read(REQUEST) != data:
            raise RuntimeError("Published inbox readback mismatch")
        return seq

    def logs(self, prefix):
        for remote, name in (("/dev_hdd0/tmp/ps3_240p.log", "app.log"),
                             ("/dev_hdd0/tmp/vsh_240p.log", "helper.log"), (REPLY, "reply.bin")):
            data = self.read(remote, True)
            if data is not None:
                (self.folder / f"{prefix}-{name}").write_bytes(data)

    def cancel_pending(self):
        if self.pending is None:
            return
        reply = self.reply()
        if reply and reply["state"] == 9:
            raise RuntimeError("Helper recovery failed; reboot required, no further tests")
        if reply and reply["sequence"] == self.pending and reply["state"] in TERMINAL:
            return
        self.publish(3, cancel=True)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            reply = self.reply()
            # Cancellation keeps the original test sequence in helper replies.
            if reply and reply["sequence"] == self.pending and reply["state"] in TERMINAL:
                return
            time.sleep(.5)
        raise RuntimeError("Cancellation sent but recovery unconfirmed; no further tests")

    def close(self):
        self.ftp.close()


def reboot(console):
    """One owner-authorized normal VSH restart; never reset settings/firmware."""
    console.preflight()
    host, folder = console.host, console.folder
    old_log = console.read("/dev_hdd0/tmp/vsh_240p.log")
    console.logs("before-reboot")
    console.close()
    print("Requesting one webMAN VSH restart", flush=True)
    try:
        with urlopen(f"http://{host}/reboot.ps3?vsh", timeout=10) as response:
            body = response.read(8192)
            (folder / "reboot-response.html").write_bytes(body)
            print(f"Restart HTTP status {response.status}", flush=True)
    except HTTPError as error:
        raise RuntimeError(f"Restart request rejected: HTTP {error.code}") from error
    except OSError as error:
        print(f"Restart HTTP connection closed: {error}", flush=True)
        # A reboot may close HTTP. Require observed disconnect/reconnect below.
    deadline = time.monotonic() + 150
    while time.monotonic() < deadline:
        candidate = None
        try:
            candidate = Console(host, folder)
            slots = candidate.slots()
            # A fast restart may evade TCP polling. A disappeared resident
            # helper, or a new worker-start identity, is stronger evidence.
            current_log = candidate.read("/dev_hdd0/tmp/vsh_240p.log", True)
            fresh = bool(current_log and old_log and current_log.splitlines()[0] != old_log.splitlines()[0])
            if not any(s["name"] == "vsh_240p" for s in slots.values()) or fresh:
                candidate.ensure_helper()
                candidate.preflight()
                print("Restart complete; matching live helper ready", flush=True)
                return candidate
        except (OSError, ftplib.Error, RuntimeError):
            pass
        if candidate:
            candidate.close()
        time.sleep(1)
    raise RuntimeError("Reboot readiness not verified; no tests sent")


def run_test(console, capture, name, folder):
    seq = console.publish(ACTIONS[name])
    print(f"{name}: sent sequence {seq}", flush=True)
    start, rows, previous, last_stamp = time.monotonic(), [], None, None
    result = {"name": name, "sequence": seq, "frames": rows, "readability": "not_assessed",
              "physical_scan_type": "not_measured", "physical_refresh_hz": None}
    deadline = start + 65
    while time.monotonic() < deadline:
        reply = console.reply()
        if reply and reply["sequence"] == seq:
            state = reply["state"]
            if state != previous:
                print(f"{name}: state {state}, detail {reply['detail']}", flush=True)
                previous = state
            elapsed = time.monotonic() - start
            path = folder / f"{name}-{len(rows):03d}-state{state}.jpg"
            stats, stamp = capture.frame(path)
            if stamp != last_stamp:
                rows.append(dict(elapsed=round(elapsed, 3), state=state, frame=path.name, **stats))
                last_stamp = stamp
            if state in TERMINAL:
                if state != 9:
                    console.pending = None
                result.update(reply)
                break
        time.sleep(.5)
    else:
        raise RuntimeError(name + ": timed out; cancellation/recovery required")
    result["active"] = summarize_active(rows)
    recovered = []
    for index in range(8):
        time.sleep(.5)
        stats, _ = capture.frame(folder / f"{name}-recovery-{index:02d}.jpg")
        recovered.append(stats["picture_detected"])
    result["recovery_picture_detected"] = sum(recovered) >= 6
    result["passed"] = bool(result["state"] == 7 and result["detail"] == 0
                            and result["recovery_picture_detected"]
                            and (name == "roundtrip" or result["active"]["picture_detected"]))
    console.logs(name)
    (folder / f"{name}.json").write_text(json.dumps(result, indent=2))
    print(f"{name}: {'PASS' if result['passed'] else 'FAIL'}; "
          f"active={result['active']}, recovery picture={result['recovery_picture_detected']}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.0.169")
    parser.add_argument("--candidate", action="store_true", help="Also test candidate after both controls pass")
    parser.add_argument("--reboot", action="store_true", help="Normal webMAN VSH restart before controls")
    args = parser.parse_args()
    folder = ROOT / "local_only/hd540/automatic" / datetime.now().strftime("%Y%m%d-%H%M%S")
    folder.mkdir(parents=True, exist_ok=False)
    report = {"time": datetime.now(timezone.utc).isoformat(), "tests": [], "status": "starting"}
    console = capture = None
    try:
        console = Console(args.host, folder)
        console.ensure_helper()
        console.preflight()
        if args.reboot:
            console = reboot(console)
        capture = Capture(folder)
        capture.start()
        baseline, _ = capture.frame(folder / "baseline.jpg")
        report["baseline"] = baseline
        if not baseline["picture_detected"]:
            raise RuntimeError("Baseline picture absent/flat; no tests sent")
        for name in ["roundtrip", "720p"] + (["candidate"] if args.candidate else []):
            result = run_test(console, capture, name, folder)
            report["tests"].append(result)
            if not result["passed"]:
                raise RuntimeError(name + " failed; stopping sequence")
        report["status"] = "passed_pixel_and_helper_checks"
    except (Exception, KeyboardInterrupt) as error:
        report["status"], report["error"] = "stopped", str(error) or "interrupted"
        print("STOP: " + report["error"], flush=True)
    finally:
        if console:
            try:
                console.cancel_pending()
                if console.ftp.sock is not None:
                    console.logs("final")
            except Exception as error:
                report["recovery_error"] = f"{type(error).__name__}: {error}"
                report["status"] = "recovery_unconfirmed"
            console.close()
        if capture:
            capture.close()
        (folder / "report.json").write_text(json.dumps(report, indent=2))
        print("Evidence: " + str(folder), flush=True)
    return 0 if report["status"] == "passed_pixel_and_helper_checks" else 1


if __name__ == "__main__":
    raise SystemExit(main())

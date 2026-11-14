"""Collect independent pre-DAC PCM snapshots from the resident audio diagnostic.

Requires an already-loaded, matching developer helper. Sends only the
read-only collection action: no mode change, install, LV1 operation or
microphone recording. Reports snippets, never a continuous audio waveform.
"""
import argparse
from datetime import datetime
import ftplib
import hashlib
import io
import json
from pathlib import Path
import secrets
import struct
import time
from urllib.request import urlopen
from hd540_auto_test import parse_slots

ROOT = Path(__file__).resolve().parents[1]
BASE = "/dev_hdd0/tmp/crt_ps1_audio"
MAGIC = 0x41554431
RECORD = struct.Struct(">6I2Q")
RECORD_BYTES = RECORD.size + 16384


def analyze(data, sequence):
    import numpy as np
    if len(data) != 64 * RECORD_BYTES:
        raise ValueError("Incomplete resident snapshot container")
    rows = []
    pid = None
    for i in range(64):
        offset = i * RECORD_BYTES
        magic, seq, process, before, after, number, started, ended = RECORD.unpack_from(data, offset)
        if magic != 0x41554452 or seq != sequence or number != i or before >= 8 or after >= 8 or ended < started:
            raise ValueError("Invalid resident snapshot metadata")
        if pid is not None and process != pid:
            raise ValueError("Mixed process identities in snapshot container")
        pid = process
        samples = np.frombuffer(data, dtype=">f4", count=4096, offset=offset + RECORD.size).reshape(-1, 2).astype(float)
        if not np.isfinite(samples).all() or np.max(np.abs(samples)) > 16:
            raise ValueError("Invalid float PCM in resident snapshot")
        duration = ended - started
        rows.append(dict(number=i, started_us=started, duration_us=duration,
                         index_before=before, index_after=after,
                         consumer_stable=before == after,
                         exceeds_nominal_48khz_ring_period=duration >= 8 * 256 / 48000 * 1e6,
                         rms=np.sqrt(np.mean(samples ** 2, axis=0)).tolist(),
                         peak=np.max(np.abs(samples), axis=0).tolist(),
                         nonzero=float(np.mean(samples != 0))))
    return dict(sequence=sequence, pid=pid, scope="64 independent pre-DAC ring snippets; not continuous audio or analog pitch proof",
                note="Consumer stability is not proof that concurrent producers left every sample unchanged",
                bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                max_read_us=max(r["duration_us"] for r in rows),
                consumer_stable_records=sum(r["consumer_stable"] for r in rows), rows=rows)


def collect(host):
    import ipaddress
    host = str(ipaddress.IPv4Address(host))
    folder = ROOT / "local_only/ps1_audio" / datetime.now().strftime("resident-%Y%m%d-%H%M%S")
    folder.mkdir(parents=True, exist_ok=False)
    with ftplib.FTP(host, timeout=8) as ftp:
        ftp.login()
        def read(path, optional=False):
            out = io.BytesIO()
            try:
                ftp.retrbinary("RETR " + path, out.write)
            except ftplib.error_perm as exc:
                if optional and str(exc).startswith("550"):
                    return None
                raise
            return out.getvalue()
        with urlopen(f"http://{host}/vshplugin.ps3mapi", timeout=8) as response:
            slots = parse_slots(response.read().decode("utf-8", errors="replace"))
        matching = [s for s in slots.values() if s["name"] == "crt_ps1_audio" and s["path"] == BASE + ".sprx"]
        if len(matching) != 1 or read(BASE + ".v2.ready", True) is None:
            raise ValueError("Resident v2 audio diagnostic not confirmed")
        local = ROOT / "build/ps1-audio/plugin/crt_ps1_audio.sprx"
        if not local.exists() or read(BASE + ".sprx") != local.read_bytes():
            raise ValueError("Resident diagnostic file differs from local checked build")
        oldq, oldr = read(BASE + ".request", True), read(BASE + ".reply", True)
        if oldq is not None:
            if oldr is None or len(oldr) != 16 or len(oldq) != 16 or oldq[:8] != oldr[:8] or struct.unpack(">4I", oldr)[2] not in (5, 6):
                raise ValueError("Previous diagnostic request is not acknowledged as finished")
        sequence = secrets.randbelow(0xFFFFFFFE) + 1
        request = struct.pack(">4I", MAGIC, sequence, 4, 0)
        staging = BASE + f".request.stage.{sequence}"
        ftp.storbinary("STOR " + staging, io.BytesIO(request))
        if read(staging) != request:
            raise ValueError("Staged request verification failed")
        if oldq is not None:
            ftp.rename(BASE + ".request", BASE + f".request.{sequence}.prev")
        ftp.rename(staging, BASE + ".request")
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            reply = read(BASE + ".reply", True)
            if reply and len(reply) == 16:
                magic, seq, state, detail = struct.unpack(">4I", reply)
                if magic == MAGIC and seq == sequence:
                    if state != 5 or detail:
                        raise ValueError(f"Collection refused: state={state}, detail=0x{detail:08x}")
                    break
            time.sleep(.2)
        else:
            raise ValueError("Collection acknowledgement timed out; no replacement request sent")
        data = read(BASE + f".{sequence}.pcm")
        (folder / "snapshots.pcm").write_bytes(data)
        (folder / "helper.log").write_bytes(read(BASE + ".log"))
    report = analyze(data, sequence)
    (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "rows"}, indent=2))
    print(folder)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ip", required=True)
    collect(parser.parse_args().ip)

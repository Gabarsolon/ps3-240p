"""Read-only PS1 emulator audio-ring snapshots through webMAN PS3MAPI.

Supports the fingerprinted 4.93 ps1_emu only. No hooks, memory writes, LV1
operations, settings changes or microphone recording. Snapshots are before
the output DAC and are NOT a continuous recording or analog-pitch proof.
Private PCM and raw memory stay in ignored local_only/ by default.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import ipaddress
import json
from pathlib import Path
import re
import socket
import struct
import time
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
INIT_VA = 0x87C90
INIT_SIZE = 0x1FC
INIT_SHA256 = "58aba84d826ead0d60c015ab83600fe5be13e83db0eaca2901da70e089cf16e0"
CONTEXT_POINTER = 0x17C348

def process_name(pid, label):
    prefix = f"{pid:08x}_"
    return label[len(prefix):] if label[:len(prefix)].lower() == prefix else label


class Reader:
    """Minimal GET-only PS3MAPI client; no arbitrary command API."""
    def __init__(self, host):
        self.host = str(ipaddress.IPv4Address(host))
        self.socket = socket.create_connection((self.host, 7887), timeout=5)
        self.socket.settimeout(5)
        self.stream = self.socket.makefile("rb")
        self._reply(220)
        self._reply(230)
        self._send("TYPE I", 200)

    def _reply(self, expected):
        data = self.stream.readline(4097)
        if len(data) > 4096 or not data.endswith(b"\r\n"):
            raise ValueError("Incomplete PS3MAPI reply")
        line = data.decode("ascii", errors="strict").rstrip("\r\n")
        if not line.startswith(str(expected) + " "):
            raise ValueError(f"PS3MAPI expected {expected}; received {line[:160]}")
        return line[4:]

    def _send(self, command, expected):
        self.socket.sendall(command.encode("ascii") + b"\r\n")
        return self._reply(expected)

    def processes(self):
        values = self._send("PROCESS GETALLPID", 200).split("|")
        rows = []
        for value in values:
            if not value:
                continue
            pid = int(value)
            if pid:
                rows.append((pid, process_name(pid, self._send(f"PROCESS GETNAME {pid}", 200))))
        return rows

    def read(self, pid, address, size):
        if not 0 < pid <= 0xFFFFFFFF or not 0x10000 <= address < 0x80000000 or not 0 < size <= 65536 or address + size > 0x80000000:
            raise ValueError("Refused unbounded process-memory read")
        response = self._send("PASV", 227)
        match = re.fullmatch(r"Entering Passive Mode \((\d+,\d+,\d+,\d+,\d+,\d+)\)", response)
        if not match:
            raise ValueError("Unexpected PS3MAPI passive response")
        parts = [int(v) for v in match[1].split(",")]
        if any(v > 255 for v in parts) or ".".join(map(str, parts[:4])) != self.host:
            raise ValueError("Passive endpoint differs from requested console")
        with socket.create_connection((self.host, parts[4] * 256 + parts[5]), timeout=5) as data:
            data.settimeout(5)
            self._send(f"MEMORY GET {pid} {address:X} {size}", 150)
            output = bytearray()
            while len(output) < size:
                chunk = data.recv(size - len(output))
                if not chunk:
                    raise ValueError("Short process-memory transfer")
                output += chunk
        self._reply(226)
        return bytes(output)

    def close(self):
        self.stream.close()
        self.socket.close()


class HTTPReader:
    """Slower GET-only fallback when the optional TCP server is disabled."""
    def __init__(self, host):
        self.host = str(ipaddress.IPv4Address(host))

    def processes(self):
        with urlopen(f"http://{self.host}/getmem.ps3mapi", timeout=5) as response:
            page = response.read().decode("latin-1")
        return [(int(pid, 16), process_name(int(pid, 16), label)) for pid, label in
                re.findall(r'<option[^>]*value=["\'](0x[0-9a-fA-F]+)["\'][^>]*>([^<\r\n]+)', page)
                if "_" in label]

    def read(self, pid, address, size):
        if not 0 < pid <= 0xFFFFFFFF or not 0x10000 <= address < 0x80000000 or not 0 < size <= 65536 or address + size > 0x80000000:
            raise ValueError("Refused unbounded process-memory read")
        data = bytearray()
        while len(data) < size:
            take = min(256, size - len(data))
            with urlopen(f"http://{self.host}/getmem.ps3mapi?proc=0x{pid:x}&addr={address + len(data):x}&len={take}", timeout=5) as response:
                page = response.read(256 * 1024)
            match = re.search(rb'<textarea[^>]*id=["\']output["\'][^>]*>([0-9A-Fa-f]+)</textarea>', page)
            if not match or len(match[1]) != take * 2:
                raise ValueError("Incomplete HTTP process-memory response")
            data += bytes.fromhex(match[1].decode("ascii"))
        return bytes(data)

    def close(self):
        pass


def port_config(raw):
    if len(raw) != 48:
        raise ValueError("Incomplete audio context")
    port, _, index, status, channels, blocks, size, audio, writer, _ = struct.unpack(">4I2Q4I", raw)
    # The emulator copies GetPortConfig BEFORE PortStart; its cached READY
    # status is not a fresh query of playback state. Cursor motion is separate.
    if port >= 8 or status not in (1, 2) or channels != 2 or blocks != 8 or size != 0x4000:
        raise ValueError("Unsupported or inactive PS1 audio ring")
    for address, length in ((index, 8), (audio, size)):
        if address < 0x10000 or address + length > 0x80000000 or address & 7:
            raise ValueError("Invalid audio ring pointer")
    if not audio <= writer < audio + size or (writer - audio) % 2048:
        raise ValueError("Invalid PS1 audio producer cursor")
    return dict(port=port, index=index, status=status, channels=channels, blocks=blocks,
                bytes=size, audio=audio, writer=writer)


def snapshot(client, folder, count, interval):
    import numpy as np
    matches = [(pid, name) for pid, name in client.processes() if name == "main_ps1_emu.self"]
    if len(matches) != 1:
        raise ValueError("Expected one running main_ps1_emu.self; no snapshot taken")
    pid, name = matches[0]
    identity = client.read(pid, INIT_VA, INIT_SIZE)
    if hashlib.sha256(identity).hexdigest() != INIT_SHA256:
        raise ValueError("PS1 audio initializer fingerprint mismatch; no ring read")
    context = int.from_bytes(client.read(pid, CONTEXT_POINTER, 4), "big")
    if not 0x10000 <= context <= 0x7FFFFFCF or context & 7:
        raise ValueError("Invalid PS1 audio context")
    context_raw = client.read(pid, context, 48)
    (folder / "context.bin").write_bytes(context_raw)
    config = port_config(context_raw)
    report = dict(time=datetime.now(timezone.utc).isoformat(), process=name, pid=pid,
                  initializer_sha256=INIT_SHA256, context=context, config=config,
                  scope="Noncontinuous, non-atomic pre-DAC snapshots; not physical output or pitch measurement",
                  consumer_comparison="Equal cursor values can hide complete ring wraps; equality does not establish coherence",
                  samples=[])
    started = time.monotonic()
    for number in range(count):
        before = time.monotonic()
        first = int.from_bytes(client.read(pid, config["index"], 8), "big")
        raw = client.read(pid, config["audio"], config["bytes"])
        second = int.from_bytes(client.read(pid, config["index"], 8), "big")
        if first >= config["blocks"] or second >= config["blocks"]:
            raise ValueError("Audio consumer cursor outside ring")
        (folder / f"ring-{number:03d}.f32be").write_bytes(raw)
        samples = np.frombuffer(raw, dtype=">f4").reshape(-1, 2).astype(float)
        finite = bool(np.isfinite(samples).all())
        if not finite or np.max(np.abs(samples)) > 16:
            raise ValueError("Ring does not contain bounded float PCM")
        duration = time.monotonic() - before
        report["samples"].append(dict(elapsed=time.monotonic() - started,
            read_seconds=duration, index_before=first, index_after=second,
            exceeds_nominal_48khz_ring_period=duration >= config["blocks"] * 256 / 48000,
            cursor_changed_during_read=first != second, sha256=hashlib.sha256(raw).hexdigest(),
            rms=np.sqrt(np.mean(samples ** 2, axis=0)).tolist(),
            peak=np.max(np.abs(samples), axis=0).tolist(),
            nonzero=float(np.mean(samples != 0))))
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        if interval:
            time.sleep(interval)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ip", required=True)
    parser.add_argument("--count", type=int, default=12)
    parser.add_argument("--interval", type=float, default=.05)
    parser.add_argument("--transport", choices=("tcp", "http"), default="tcp")
    args = parser.parse_args()
    if not 1 <= args.count <= 120 or not 0 <= args.interval <= 5:
        parser.error("count must be 1..120 and interval 0..5 seconds")
    folder = ROOT / "local_only/ps1_audio" / datetime.now().strftime("ring-%Y%m%d-%H%M%S")
    folder.mkdir(parents=True, exist_ok=False)
    client = (HTTPReader if args.transport == "http" else Reader)(args.ip)
    try:
        report = snapshot(client, folder, args.count, args.interval)
        print(json.dumps(dict(folder=str(folder), config=report["config"], samples=len(report["samples"]),
                              scope=report["scope"]), indent=2))
    finally:
        client.close()


if __name__ == "__main__":
    main()

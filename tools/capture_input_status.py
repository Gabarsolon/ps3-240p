"""Read HDMI input status from the owner's UltraSemi USB capture card.

Protocol: BertoldVdb/ms-tools mshal/hal_rom.go and hal_region.go.
Status addresses: FOSDEM/video-ms213x-status status.go, readFlaky/readMurderous.
See docs/CAPTURE_INPUT_STATUS.md for sources and interpretation limits.
Only the RAM-read opcode is sent; no firmware, EEPROM or RAM writes.
"""

import argparse
from datetime import datetime, timezone
import json
import math
import sys
import time


def _read_report(device, address, length):
    request = bytes([0, 0xB5, address >> 8, address & 255, 0, 0, 0, 0, 0])
    if device.send_feature_report(request) != len(request):
        raise RuntimeError("Incomplete HID read request")
    response = bytes(device.get_feature_report(0, 9))
    if len(response) != 9 or response[:4] != request[:4]:
        raise RuntimeError("Unexpected HID response; stopping")
    time.sleep(0.1)
    return response[4:4 + length]


def read_cached(device, address, length):
    # Continuous polling must never touch live HDMI registers.
    allowed = {0xF800: 1, 0xF660: 4, 0xF6E9: 1}
    if allowed.get(address) != length:
        raise ValueError("Read outside the cached-status allowlist")
    return _read_report(device, address, length)


def read_scan_once(device):
    # One report only; do not read the upstream 16-byte live register block.
    # These reads can interrupt capture on some hardware/firmware combinations.
    value = _read_report(device, 0xE180, 1)[0]
    return {
        "raw_scan_status": f"0x{value:02x}",
        "scan_type": {0x0F: "interlaced", 0x07: "progressive"}.get(value, "unknown"),
        "live_reported_signal_present": {0: False, 8: False, 7: True, 15: True}.get(value),
        "scan_type_source": "FOSDEM live status interpretation; firmware mapping unvalidated",
    }


def sample(device):
    size = read_cached(device, 0xF660, 4)
    signal = read_cached(device, 0xF6E9, 1)
    return {
        "time": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "reported_width": int.from_bytes(size[:2], "little"),
        "reported_height": int.from_bytes(size[2:], "little"),
        "reported_signal_present": signal[0] == 0,
        "raw_size": size.hex(),
        "raw_signal": signal.hex(),
        "scan_type": "unknown",
        "refresh_hz": None,
        "mapping_verified_for_this_firmware": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Print JSON (one object per sample)")
    parser.add_argument("--watch", type=float, metavar="SECONDS",
                        help="Repeat at this interval, at least 2 seconds; Ctrl+C stops")
    parser.add_argument("--scan-once", action="store_true",
                        help="One live interlace-flag query; may interrupt capture; cannot use --watch")
    args = parser.parse_args()
    if args.watch is not None and (not math.isfinite(args.watch) or args.watch < 2):
        parser.error("--watch must be at least 2 seconds")
    if args.scan_once and args.watch is not None:
        parser.error("--scan-once cannot be combined with --watch")
    try:
        import hid
    except ImportError:
        parser.exit(1, "Requires hidapi: python -m pip install hidapi\n")

    devices = [d for d in hid.enumerate(0x345F, 0x2131)
               if d.get("interface_number") == 4]
    if len(devices) != 1 or devices[0].get("release_number") != 0x3100:
        parser.exit(1, "Expected one UltraSemi 345f:2131 interface 4, release 3100; stopped.\n")

    device = hid.device()
    try:
        device.open_path(devices[0]["path"])
        if read_cached(device, 0xF800, 1) != b"\x00":
            raise RuntimeError("Unsupported chip-family register; stopped")
        if not args.json:
            print("Cached HDMI input report (exact firmware mapping unvalidated).", flush=True)
            print("540 lines may mean one 1080i field. Refresh is unknown.", flush=True)
            if args.scan_once:
                print("One live scan-status query; may briefly interrupt capture.", flush=True)
            if args.watch:
                print("Ctrl+C stops. Stop polling if capture becomes unstable.", flush=True)
        while True:
            result = sample(device)
            if args.scan_once:
                result.update(read_scan_once(device))
            if args.json:
                print(json.dumps(result), flush=True)
            else:
                signal = "present" if result["reported_signal_present"] else "absent"
                scan = result['scan_type']
                if args.scan_once:
                    scan += f" (raw {result['raw_scan_status']}; upstream interpretation)"
                print(f"{result['time']}  reported {result['reported_width']} x "
                      f"{result['reported_height']}  signal {signal}  "
                      f"scan {scan}  Hz unknown", flush=True)
            if args.watch is None:
                break
            time.sleep(args.watch)
    except KeyboardInterrupt:
        return 0
    except (OSError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1
    finally:
        device.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Offline 540p/1080i timing arithmetic; never connects to or configures a PS3.

Reference: docs/HD_CRT_540P.md. Integer totals are hypotheses for PS3 output,
not patch values. 74.175 MHz is the rounded firmware clock-table value.
"""

import argparse
from fractions import Fraction
import json


def timing(name, clock_mhz, htotal, vtotal, interlaced=False):
    clock = Fraction(str(clock_mhz)) * 1_000_000
    line_hz = clock / htotal
    frame_hz = line_hz / vtotal
    return {
        "name": name,
        "pixel_clock_mhz": float(clock / 1_000_000),
        "h_total": htotal,
        "v_total": vtotal,
        "interlaced": interlaced,
        "line_khz": float(line_hz / 1000),
        "frames_hz": float(frame_hz),
        "refresh_hz": float(frame_hz * (2 if interlaced else 1)),
    }


def report():
    modes = []
    for clock in ("74.250", "74.175"):
        modes.append(timing("Stock 1080i reference", clock, 2200, 1125, True))
        for total in (562, 563):
            modes.append(timing("540p arithmetic candidate", clock, 2200, total))
    targets = []
    for total in (562, 563):
        for label, refresh in (("60", Fraction(60)), ("60000/1001", Fraction(60000, 1001))):
            targets.append({
                "h_total": 2200,
                "v_total": total,
                "target_hz": label,
                "required_clock_mhz": float(2200 * total * refresh / 1_000_000),
            })
    return {"status": "Offline arithmetic only; PS3 540p output unverified", "modes": modes, "target_clocks": targets}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Print machine-readable results")
    args = parser.parse_args()
    data = report()
    if args.json:
        print(json.dumps(data, indent=2))
        return
    print(data["status"])
    print("\nMode                         Clock MHz   Htotal Vtotal   Line kHz  Refresh Hz")
    for mode in data["modes"]:
        print(f'{mode["name"]:28} {mode["pixel_clock_mhz"]:9.3f} '
              f'{mode["h_total"]:6} {mode["v_total"]:6} '
              f'{mode["line_khz"]:10.6f} {mode["refresh_hz"]:11.6f}')
    print("\nClock required at Htotal 2200 (PLL programmability is unverified):")
    for target in data["target_clocks"]:
        print(f'  {target["v_total"]} lines at {target["target_hz"]:>10} Hz: '
              f'{target["required_clock_mhz"]:.9f} MHz')


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compare equal battery-powered windows; never infer hours from voltage alone."""
import argparse
import csv
import json
import math
from pathlib import Path


def read_window(path):
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    if len(rows) < 2:
        raise ValueError("At least two samples are required")
    samples = []
    for row in rows:
        if row["usb_connected"].strip() != "0":
            raise ValueError("Window contains USB supply/charging; not a battery comparison")
        t = float(row["elapsed_s"])
        v = float(row["battery_voltage_v"])
        current = row.get("battery_current_ma", "").strip()
        i = float(current) if current else None
        if not math.isfinite(t) or not math.isfinite(v) or not 0 < v < 5:
            raise ValueError("Invalid time or battery voltage")
        if i is not None and (not math.isfinite(i) or i < 0):
            raise ValueError("Current must be finite positive discharge current in mA")
        if samples and t <= samples[-1][0]:
            raise ValueError("Sample times must increase")
        samples.append((t, v, i))
    duration = samples[-1][0] - samples[0][0]
    out = {"file": str(path), "duration_s": duration,
           "voltage_change_v": samples[-1][1] - samples[0][1]}
    if all(s[2] is None for s in samples):
        out["evidence"] = "voltage_only; cannot calculate battery life improvement"
        return out
    if any(s[2] is None for s in samples):
        raise ValueError("Partial current records cannot be compared")
    charge = energy = 0.0
    for a, b in zip(samples, samples[1:]):
        dt = b[0] - a[0]
        charge += (a[2] + b[2]) * 0.5 * dt / 3600
        energy += (a[1] * a[2] + b[1] * b[2]) * 0.5 * dt / 3600
    out.update(evidence="battery_side_current", mean_current_ma=charge * 3600 / duration,
               discharge_mah=charge, energy_mwh=energy)
    return out


def compare(old, new, tolerance=2):
    if tolerance < 0 or not math.isfinite(tolerance):
        raise ValueError("Duration tolerance must be finite and nonnegative")
    if abs(old["duration_s"] - new["duration_s"]) > tolerance:
        raise ValueError("Different durations: repeat equal-time windows")
    result = {"old": old, "new": new, "measured_extra_hours": None}
    if "mean_current_ma" in old and "mean_current_ma" in new:
        a, b = old["mean_current_ma"], new["mean_current_ma"]
        if a <= 0 or b <= 0:
            raise ValueError("Zero average current cannot establish a valid battery-life ratio")
        result.update(current_reduction_fraction=1 - b / a,
                      conditional_runtime_multiplier=a / b,
                      limitation="Estimate for the tested workload only; actual baseline runtime/capacity required for hours")
    else:
        result["limitation"] = "Voltage-only data does not establish runtime improvement"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("old")
    parser.add_argument("new")
    parser.add_argument("--duration-tolerance-s", type=float, default=2)
    args = parser.parse_args()
    try:
        result = compare(read_window(args.old), read_window(args.new), args.duration_tolerance_s)
    except (ValueError, KeyError) as error:
        parser.error(str(error))
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

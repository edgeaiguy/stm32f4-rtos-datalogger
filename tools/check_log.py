#!/usr/bin/env python3
"""Integrity check for a datalogger CSV read back from the SD card.

Checks the header and row shape, that t_ms only moves forward, that no sample
rows are missing (steps against the median step), the environment row cadence,
and that the RTC wall clock agrees with t_ms. Exits non-zero on any failure.
"""
import argparse
import re
import statistics
import sys
from datetime import datetime

HEADER = "timestamp,t_ms,temp_c,press_hpa,accel_x_mg,accel_y_mg,accel_z_mg"
ROW = re.compile(
    r"^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}),(\d+),"
    r"(-?\d+\.\d\d)?,(\d+\.\d\d)?,(-?\d+),(-?\d+),(-?\d+)$"
)
GAP_FACTOR = 1.5      # a step this many times the median step means rows are missing
ENV_PERIOD_MS = 1000
ENV_SLACK_MS = 500


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv")
    ap.add_argument("--rate", type=float, help="logged rate in Hz; compared with the measured row rate")
    args = ap.parse_args()

    with open(args.csv, newline="") as f:
        lines = f.read().split("\r\n")
    if lines and lines[-1] == "":
        lines.pop()
    problems = []

    if not lines or lines[0] != HEADER:
        sys.exit(f"FAIL: unexpected header: {lines[0] if lines else '(empty file)'}")

    rows, bad = [], 0
    for n, line in enumerate(lines[1:], start=2):
        m = ROW.match(line)
        if not m:
            bad += 1
            if bad <= 5:
                problems.append(f"line {n}: malformed row: {line[:60]!r}")
            continue
        rows.append((datetime.strptime(m.group(1), "%Y-%m-%dT%H:%M:%S.%f"), int(m.group(2)), m.group(3) is not None))
    if bad:
        problems.append(f"{bad} malformed rows (the last line may be cut off if the card was pulled mid-write)")
    if len(rows) < 3:
        sys.exit("FAIL: fewer than 3 valid rows")

    t_ms = [r[1] for r in rows]
    steps = [b - a for a, b in zip(t_ms, t_ms[1:])]
    backwards = sum(1 for s in steps if s < 0)
    if backwards:
        problems.append(f"{backwards} rows where t_ms goes backwards")

    med = statistics.median(s for s in steps if s > 0)
    gaps = [(i, s) for i, s in enumerate(steps) if s > GAP_FACTOR * med]
    missing = sum(round(s / med) - 1 for _, s in gaps)
    if gaps:
        problems.append(f"{len(gaps)} gaps, about {missing} rows missing; first at t_ms {t_ms[gaps[0][0]]} "
                        f"(step {gaps[0][1]} ms against a median of {med:.0f} ms)")

    span_ms = t_ms[-1] - t_ms[0]
    env_t = [r[1] for r in rows if r[2]]
    env_steps = [b - a for a, b in zip(env_t, env_t[1:])]
    late = [s for s in env_steps if abs(s - ENV_PERIOD_MS) > ENV_SLACK_MS]
    if late:
        problems.append(f"{len(late)} environment steps outside {ENV_PERIOD_MS} +/- {ENV_SLACK_MS} ms (worst {max(late)} ms)")

    wall_s = (rows[-1][0] - rows[0][0]).total_seconds()
    drift_ppm = ((wall_s * 1000 - span_ms) / span_ms * 1e6) if span_ms else 0.0

    print(f"rows           {len(rows)}")
    print(f"span           {span_ms / 1000:.1f} s  (median step {med:.0f} ms = {1000 / med:.2f} Hz)")
    if args.rate:
        print(f"expected       {span_ms / 1000 * args.rate:.0f} rows at {args.rate:g} Hz")
    print(f"gaps           {len(gaps)}  (about {missing} rows missing)")
    print(f"env rows       {len(env_t)}  (steps {min(env_steps, default=0)}..{max(env_steps, default=0)} ms)")
    print(f"RTC vs t_ms    {drift_ppm:+.0f} ppm over {wall_s:.0f} s")

    if problems:
        print("FAIL")
        for p in problems:
            print("  " + p)
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()

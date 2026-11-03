#!/usr/bin/env python3
"""Sample-period jitter and drift from a logic-analyzer CSV export.

Reads rising edges on one channel and reports period statistics. Jitter is the
spread of periods around their own mean (the scheduler's contribution); drift is
the mean against the nominal period (the sensor clock's contribution).
"""
import argparse
import csv
import math
import re
import sys

NUM = re.compile(r"^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$")
UNITS = {"s": 1.0, "ms": 1e-3, "us": 1e-6, "ns": 1e-9}


def rising_edges(path, column, unit):
    """Timestamps (seconds) where the chosen column goes 0 -> 1."""
    scale = UNITS[unit]
    edges, prev = [], None
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if len(row) <= column or not NUM.match(row[0].strip()) or not NUM.match(row[column].strip()):
                continue
            level = float(row[column]) > 0.5
            if prev is False and level:
                edges.append(float(row[0]) * scale)
            prev = level
    return edges


def pct(sorted_vals, p):
    """Nearest-rank percentile."""
    k = max(0, math.ceil(p / 100 * len(sorted_vals)) - 1)
    return sorted_vals[k]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", help="exported CSV: time in column 0, one column per channel")
    ap.add_argument("--column", type=int, default=1, help="channel column index (default 1)")
    ap.add_argument("--unit", choices=UNITS, default="s", help="time unit of column 0 (default s)")
    ap.add_argument("--rate", type=float, help="nominal rate in Hz, for the drift line")
    args = ap.parse_args()

    edges = rising_edges(args.csv, args.column, args.unit)
    if len(edges) < 3:
        sys.exit(f"need at least 3 rising edges, found {len(edges)}")

    periods = [b - a for a, b in zip(edges, edges[1:])]
    n = len(periods)
    mean = sum(periods) / n
    sigma = math.sqrt(sum((p - mean) ** 2 for p in periods) / n)
    dev = sorted(abs(p - mean) for p in periods)
    us = 1e6

    print(f"periods        {n}")
    print(f"mean           {mean * us:.1f} us ({1 / mean:.4f} Hz)")
    print(f"min / max      {min(periods) * us:.1f} / {max(periods) * us:.1f} us")
    print(f"sigma          {sigma * us:.1f} us")
    print(f"p99 |dev|      {pct(dev, 99) * us:.1f} us")
    print(f"peak-to-peak   {(max(periods) - min(periods)) * us:.1f} us")
    if args.rate:
        nominal = 1 / args.rate
        print(f"drift          {(mean - nominal) / nominal * 1e6:+.1f} ppm vs {args.rate:g} Hz")


if __name__ == "__main__":
    main()

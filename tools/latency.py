#!/usr/bin/env python3
"""INT1-to-task wake latency from a logic-analyzer CSV export.

Pairs each rising edge on the INT1 channel with the next rising edge on the
accel-read probe channel (PD0). The gap is interrupt entry, the context switch
and the first line of the task loop.
"""
import argparse
import bisect
import math
import sys

from jitter import UNITS, pct, rising_edges


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", help="exported CSV: time in column 0, one column per channel")
    ap.add_argument("--int1", type=int, default=4, help="INT1 channel column (default 4)")
    ap.add_argument("--read", type=int, default=1, help="PD0 read-start channel column (default 1)")
    ap.add_argument("--unit", choices=UNITS, default="s", help="time unit of column 0 (default s)")
    args = ap.parse_args()

    ints = rising_edges(args.csv, args.int1, args.unit)
    reads = rising_edges(args.csv, args.read, args.unit)
    if len(ints) < 3 or len(reads) < 3:
        sys.exit(f"need edges on both channels, found INT1 {len(ints)}, read {len(reads)}")

    lat = []
    for t in ints:
        i = bisect.bisect_right(reads, t)
        if i < len(reads):
            lat.append(reads[i] - t)
    lat.sort()
    n = len(lat)
    mean = sum(lat) / n
    sigma = math.sqrt(sum((v - mean) ** 2 for v in lat) / n)
    us = 1e6

    print(f"INT1 edges     {len(ints)}  (paired {n})")
    print(f"mean           {mean * us:.1f} us")
    print(f"sigma          {sigma * us:.1f} us")
    print(f"min / max      {lat[0] * us:.1f} / {lat[-1] * us:.1f} us")
    print(f"p50 / p99      {pct(lat, 50) * us:.1f} / {pct(lat, 99) * us:.1f} us")


if __name__ == "__main__":
    main()

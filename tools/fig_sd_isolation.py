#!/usr/bin/env python3
"""SD isolation figure: the worst SD stall in the m3 capture while samples keep arriving on time."""
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs

DATA = os.path.expanduser("~/src/edgeaiguy/docs/stm32f4-rtos-datalogger/data/rtos-m3-20hz.csv")
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-sd-isolation.png")
SAMPLE_COL, SD_COL = 1, 3
PRE_MS, POST_MS = 15.0, 25.0


def load():
    rows = list(csv.reader(open(DATA)))[1:]
    return [(float(r[0]), int(r[SAMPLE_COL]), int(r[SD_COL])) for r in rows]


def longest_sd_pulse(rows):
    best, start, prev = (0, 0, 0), None, rows[0]
    for t, _, sd in rows[1:]:
        if prev[2] == 0 and sd == 1:
            start = t
        if prev[2] == 1 and sd == 0 and start is not None and t - start > best[2]:
            best = (start, t, t - start)
        prev = (t, 0, sd)
    return best


def main():
    rows = load()
    s0, s1, dur = longest_sd_pulse(rows)
    lo, hi = s0 - PRE_MS * 1e-3, s1 + POST_MS * 1e-3
    edges = [t for (t, a, _), (_, b, _) in zip(rows[1:], rows) if b == 0 and a == 1 and lo <= t <= hi]
    periods = [(b - a) * 1e3 for a, b in zip(edges, edges[1:])]
    during = [e for e in edges if s0 <= e <= s1]

    fig = fs.new_figure(760)
    ax = fig.add_axes([0.06, 0.20, 0.90, 0.62], facecolor=fs.BG)
    ms = lambda t: (t - s0) * 1e3
    ax.axvspan(0, dur * 1e3, ymin=0.62, ymax=0.92, color=fs.AMBER, alpha=0.9)
    ax.text(dur * 1e3 / 2, 2.75, f"SD card busy: {dur * 1e3:.1f} ms", color=fs.BG, fontsize=18,
            fontweight="bold", ha="center", va="center")
    for e in edges:
        ax.vlines(ms(e), 0.05, 1.0, color=fs.HERO, linewidth=3)
    ax.text(-PRE_MS - 7.5, 0.55, "sample", color=fs.HERO, fontsize=16, va="center", ha="left")
    ax.set_xlim(-PRE_MS - 8, dur * 1e3 + POST_MS); ax.set_ylim(0, 3.2)
    ax.set_yticks([]); ax.tick_params(labelsize=13)
    ax.set_xlabel("time from the start of the SD stall (ms)", fontsize=14)
    for sp in ("top", "right", "left"):
        ax.spines[sp].set_visible(False)
    ax.annotate(f"{len(during)} samples arrive during the stall, spacing {min(periods):.2f} to {max(periods):.2f} ms",
                xy=(ms(during[len(during) // 2]), 1.05), xytext=(dur * 1e3 / 2, 1.6), color=fs.HERO,
                fontsize=17, ha="center", va="center",
                arrowprops=dict(arrowstyle="->", color=fs.HERO, lw=1.8))
    fs.bars(fig, "SD stalls do not delay sampling",
            "STM32F407 · FreeRTOS · 20 Hz logging · LA1010 1 MHz",
            "The slowest SD write in the capture holds only the logger; sampling never waits for it.")
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)
    print(len(edges), "samples in window; during stall", len(during), "periods", min(periods), max(periods))


main()

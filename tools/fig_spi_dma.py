#!/usr/bin/env python3
"""SPI2 SCK during the 512-byte data phase of a sector write: polling against DMA, same time scale."""
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs

DATA = os.path.expanduser("~/src/edgeaiguy/docs/stm32f4-rtos-datalogger/data")
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-spi-dma.png")
RUNS = (("rtos-m4-spi2-before-20hz.csv", "Polling: CPU clocks every byte", fs.AMBER),
        ("rtos-m5-spi2-after-write-dma-20hz.csv", "DMA: the CPU sleeps", fs.HERO))
WINDOW_US = 90.0


def load(name):
    rows = list(csv.reader(open(os.path.join(DATA, name))))[1:]
    return [(float(r[0]), int(r[1]), int(r[2])) for r in rows]


def data_phase(rows):
    """Times of SCK rising edges, grouped by byte, from the 0xFE token through 512 data bytes."""
    rises = [(t, m) for (t, s0, m), (_, s1, _) in zip(rows[1:], rows) if s0 == 0 and s1 == 1 or False]
    rises = [(rows[i][0], rows[i][2]) for i in range(1, len(rows)) if rows[i - 1][1] == 0 and rows[i][1] == 1]
    starts, vals = [], []
    for i in range(0, len(rises) - 7, 8):
        v = 0
        for k in range(8):
            v = (v << 1) | rises[i + k][1]
        starts.append(rises[i][0]); vals.append(v)
    i = vals.index(0xFE)
    return starts[i + 1:i + 513]


def main():
    fig = fs.new_figure(820)
    stats = []
    for n, (name, label, colour) in enumerate(RUNS):
        rows = load(name)
        starts = data_phase(rows)
        gaps = sorted(b - a for a, b in zip(starts, starts[1:]))
        med = gaps[len(gaps) // 2]
        i0 = next(i for i in range(20, 500)
                  if all(abs((starts[i + k + 1] - starts[i + k]) / med - 1) < 0.05 for k in range(8)))
        t0 = starts[i0]
        span_ms = (starts[-1] - starts[0]) * 1e3
        per_us = (starts[-1] - starts[0]) / 511 * 1e6
        stats.append((span_ms, per_us))
        ax = fig.add_axes([0.06, 0.55 - n * 0.30, 0.90, 0.20], facecolor=fs.BG)
        xs, ys, prev = [], [], None
        for t, s, _ in rows:
            x = (t - t0) * 1e6
            if x < -2 or x > WINDOW_US + 2:
                continue
            if prev is not None:
                xs.append(x); ys.append(prev)
            xs.append(x); ys.append(s); prev = s
        ax.step(xs, ys, where="post", color=colour, linewidth=2.2)
        ax.set_xlim(0, WINDOW_US); ax.set_ylim(-0.4, 2.5)
        ax.set_yticks([]); ax.tick_params(labelsize=13)
        for sp in ("top", "right", "left"):
            ax.spines[sp].set_visible(False)
        if n == 0:
            ax.set_xticks([])
            ax.spines["bottom"].set_visible(False)
        else:
            ax.set_xlabel("time into the data phase (µs)", fontsize=14)
        ax.text(0, 2.15, label, color=colour, fontsize=17, fontweight="bold", va="center")
        b = [(s - t0) * 1e6 for s in starts if 0 <= (s - t0) * 1e6 <= WINDOW_US]
        ax.annotate("", xy=(b[1], 1.2), xytext=(b[0], 1.2),
                    arrowprops=dict(arrowstyle="<->", color=fs.TEXT, lw=1.6))
        ax.text(b[1] + 2.5, 1.2, f"{(b[1] - b[0]):.1f} µs per byte",
                color=fs.TEXT, fontsize=14, ha="left", va="center")
        ax.text(WINDOW_US, 2.15, f"512 bytes: {span_ms:.2f} ms  ({512 / span_ms:.0f} KB/s)",
                color=fs.TEXT, fontsize=17, ha="right", va="center")
    fs.bars(fig, "SPI2 clock during one sector write",
            "STM32F407 · SPI2 4 MHz · LA1010 100 MSa/s · typical 90 µs window, same scale",
            f"The data phase drops from {stats[0][0]:.2f} ms to {stats[1][0]:.2f} ms: the bytes now go out back to back.")
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)


main()

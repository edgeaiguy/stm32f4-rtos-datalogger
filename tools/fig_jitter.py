#!/usr/bin/env python3
"""Sample-period deviation histograms: P3 bare-metal loop against the RTOS, same axes."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs
import jitter

DATA = os.path.expanduser("~/src/edgeaiguy/docs/stm32f4-rtos-datalogger/data")
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-jitter.png")
BIN_US = 25


def deviations(name):
    e = jitter.rising_edges(os.path.join(DATA, name), 1, "s")
    p = [(b - a) * 1e6 for a, b in zip(e, e[1:])]
    mean = sum(p) / len(p)
    return [x - mean for x in p], math.sqrt(sum((x - mean) ** 2 for x in p) / len(p))


def main():
    fig = fs.new_figure(900)
    ax = fig.add_axes([0.07, 0.17, 0.90, 0.69], facecolor=fs.BG)
    lo, hi = -3000, 33000
    for name, colour, label in (("p3-20hz.csv", fs.AMBER, "P3 bare-metal loop"),
                                ("rtos-m3-20hz.csv", fs.HERO, "RTOS, INT1-driven")):
        dev, sigma = deviations(name)
        n = (hi - lo) // BIN_US
        counts = [0] * n
        for d in dev:
            if lo <= d < hi:
                counts[int((d - lo) // BIN_US)] += 1
        xs = [lo + i * BIN_US for i in range(n) if counts[i]]
        ys = [counts[i] / len(dev) * 100 for i in range(n) if counts[i]]
        ax.plot([x + BIN_US / 2 for x in xs], ys, "o", ms=6, color=colour, alpha=0.9,
                label=f"{label}   sigma {sigma:,.1f} µs   p-p {max(dev) - min(dev):,.0f} µs")
    ax.set_yscale("log")
    ax.set_xlim(-3000, 33000)
    ax.set_ylim(0.005, 150)
    ax.set_xlabel("period minus its own mean (µs)", fontsize=14)
    ax.set_ylabel("share of periods (%, log)", fontsize=14)
    ax.tick_params(labelsize=13)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.grid(axis="y", color=fs.RAISED, linewidth=1)
    leg = ax.legend(loc="upper right", frameon=False, fontsize=15, markerscale=2)
    for t in leg.get_texts():
        t.set_color(fs.TEXT)
    fs.bars(fig, "Sample-period jitter, 20 Hz logging",
            "STM32F407 · 16 MHz HSI · LA1010 1 MHz · SD + console active",
            "The RTOS sample period stays within ±120 µs; the bare-metal loop wanders by milliseconds.")
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)


main()

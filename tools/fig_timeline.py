#!/usr/bin/env python3
"""Task-timeline figure from the m4 timeline capture: the accel task preempts the logger."""
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs

DATA = os.path.expanduser("~/src/edgeaiguy/docs/stm32f4-rtos-datalogger/data/rtos-m4-timeline-20hz.csv")
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-timeline.png")

# CSV column per signal
COL = {"INT1": 3, "PD0 read": 0, "accel": 4, "env": 5, "logger": 6, "SD write": 2, "busy": 1}
COLOUR = {"INT1": fs.TEXT, "PD0 read": fs.MUTED, "accel": fs.HERO, "env": fs.MUTED,
          "logger": fs.AMBER, "SD write": fs.MUTED, "busy": fs.SUBTLE}

T0 = 13.800        # main window start (s)
MAIN_MS = 20.0
ZOOM = (6.50, 8.00)  # ms, relative to T0


def load():
    rows = list(csv.reader(open(DATA)))[1:]
    return [float(r[0]) for r in rows], [[int(x) for x in r[1:]] for r in rows]


def steps(times, vals, col, lo, hi):
    """(x, y) step points in ms relative to T0 for one channel over [lo, hi] ms."""
    xs, ys, prev = [], [], None
    for t, v in zip(times, vals):
        x = (t - T0) * 1e3
        if x < lo:
            prev = v[col]
            continue
        if prev is None:
            prev = v[col]
        if not xs:
            xs.append(lo); ys.append(prev)
        if x > hi:
            break
        if v[col] != prev:
            xs.append(x); ys.append(prev)
            xs.append(x); ys.append(v[col])
            prev = v[col]
    xs.append(hi); ys.append(prev if prev is not None else 0)
    return xs, ys


PITCH = 1.5


def row_y(i, n):
    return (n - 1 - i) * PITCH


def draw(ax, times, vals, names, lo, hi, ytop=0.75):
    n = len(names)
    ax.set_facecolor(fs.BG)
    for i, name in enumerate(names):
        y0 = row_y(i, n)
        xs, ys = steps(times, vals, COL[name], lo, hi)
        ax.plot(xs, [y0 + 0.62 * v for v in ys], color=COLOUR[name], lw=2.2, solid_joinstyle="miter")
        ax.axhline(y0 - 0.18, color=fs.HAIRLINE, lw=1)
        ax.text(lo - (hi - lo) * 0.012, y0 + 0.3, name, ha="right", va="center", color=COLOUR[name], fontsize=15)
    ax.set_xlim(lo, hi)
    ax.set_ylim(-0.25, (n - 1) * PITCH + ytop)
    ax.set_yticks([])
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.tick_params(labelsize=14, length=4)


def bracket(ax, x0, x1, y, text, colour, size=19, dy=0.16):
    ax.annotate("", xy=(x0, y), xytext=(x1, y),
                arrowprops=dict(arrowstyle="|-|", color=colour, lw=2, shrinkA=0, shrinkB=0, mutation_scale=8))
    ax.text((x0 + x1) / 2, y + dy, text, ha="center", va="bottom", color=colour, fontsize=size, fontweight="bold")


def main():
    times, vals = load()
    fig = fs.new_figure(1300)
    W, H = 2300, 1300
    left, right = 190 / W, 1 - 50 / W

    main_names = ["INT1", "PD0 read", "accel", "env", "logger", "SD write", "busy"]
    ax1 = fig.add_axes([left, 590 / H, right - left, 600 / H])
    draw(ax1, times, vals, main_names, 0, MAIN_MS)
    ax1.set_xlabel("ms", fontsize=14, color=fs.MUTED, loc="right")

    # logger runs 6.045-7.021 and 7.468-18.056 ms; one bracket over the whole stretch
    ly = row_y(4, 7)
    bracket(ax1, 6.045, 18.056, ly + 0.8, "logger: 11.6 ms of CPU, preempted once", fs.AMBER, dy=0.1)
    ax1.text(4.45, row_y(3, 7) + 0.95, "env: BMP280 fetch, 3.1 ms", ha="center", va="center", color=fs.MUTED, fontsize=15)
    ax1.axvspan(ZOOM[0], ZOOM[1], color=fs.HERO, alpha=0.10, lw=0)

    zoom_names = ["INT1", "PD0 read", "accel", "logger"]
    ax2 = fig.add_axes([left, 170 / H, right - left, 330 / H])
    draw(ax2, times, vals, zoom_names, *ZOOM, ytop=1.15)
    ax2.set_xlabel("ms (zoom of the shaded band above)", fontsize=14, color=fs.MUTED, loc="right")
    iy = row_y(0, 4) + 0.62 + 0.2
    bracket(ax2, 6.949, 7.021, iy, "", fs.HERO)
    ax2.text(7.07, iy, "INT1 to accel running: 72 µs", color=fs.HERO, fontsize=23, fontweight="bold", va="center")
    ax2.text(7.52, row_y(2, 4) + 0.3, "accel 447 µs, then logger resumes", color=fs.MUTED, fontsize=16, va="center")

    fs.bars(fig, "Task timeline",
            "20 s capture · 10 MHz · INT1 on PD1, task pins PD4-PD6 · SD logging active",
            "The logger holds the CPU for over 11 ms, and the sampler still starts 72 µs after the interrupt.")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)
    print(os.path.abspath(OUT))


if __name__ == "__main__":
    main()

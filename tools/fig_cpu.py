#!/usr/bin/env python3
"""CPU share by task at 5, 20 and 100 Hz logging (from the m4 results table)."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-cpu.png")
# rate: (idle, accel, logger, env), percent
RUNS = {"5 Hz": (97.0, 0.7, 1.4, 0.3), "20 Hz": (92.9, 2.8, 3.4, 0.3), "100 Hz": (81.6, 3.1, 14.4, 0.3)}
SERIES = (("accel", fs.HERO), ("logger", fs.AMBER), ("env", fs.MUTED), ("other", fs.SUBTLE))


def main():
    fig = fs.new_figure(760)
    ax = fig.add_axes([0.09, 0.24, 0.86, 0.57], facecolor=fs.BG)
    for i, (rate, (idle, *tasks)) in enumerate(RUNS.items()):
        vals = tasks + [round(100 - idle - sum(tasks), 1)]
        left = 0
        for (name, colour), v in zip(SERIES, vals):
            ax.barh(i, v, left=left, height=0.55, color=colour, label=name if i == 0 else None)
            if v >= 1.0:
                ax.text(left + v / 2, i, f"{v:.1f}", ha="center", va="center", fontsize=15,
                        color=fs.BG, fontweight="bold")
            left += v
        ax.text(left + 0.3, i, f"busy {left:.1f}%   idle {idle:.1f}%", va="center", fontsize=15, color=fs.TEXT)
    ax.set_yticks(range(len(RUNS)), list(RUNS))
    ax.set_xlim(0, 26)
    ax.set_ylim(2.6, -0.5)
    ax.set_xlabel("CPU share (%)", fontsize=14)
    ax.tick_params(labelsize=14)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    leg = ax.legend(loc="upper right", frameon=False, fontsize=15, ncol=4, bbox_to_anchor=(1, 1.02))
    for t in leg.get_texts():
        t.set_color(fs.TEXT)
    fs.bars(fig, "CPU share by task",
            "FreeRTOS V11.3.2 · 16 MHz · -O0 · 5 min per rate · SD logging",
            "Logging grows with rate because every row is formatted and written through FatFs.")
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)


main()

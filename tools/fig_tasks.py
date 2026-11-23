#!/usr/bin/env python3
"""Task and queue diagram for the README."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import figstyle as fs
from matplotlib.patches import FancyBboxPatch

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures", "figure-tasks.png")
W, H = 2300, 900


def box(ax, x, y, w, h, title, sub, edge, fill=fs.RAISED, tc=fs.TEXT):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=0.12",
                                facecolor=fill, edgecolor=edge, linewidth=2.4))
    ax.text(x + w / 2, y + h * 0.64, title, ha="center", va="center", color=tc, fontsize=17, fontweight="bold")
    ax.text(x + w / 2, y + h * 0.28, sub, ha="center", va="center", color=fs.MUTED, fontsize=13)


def arrow(ax, p, q, colour=fs.MUTED):
    ax.annotate("", xy=q, xytext=p, arrowprops=dict(arrowstyle="-|>", color=colour, lw=2.2, shrinkA=0, shrinkB=0))


def main():
    fig = fs.new_figure(H)
    ax = fig.add_axes([0.0, 0.14, 1.0, 0.76], facecolor=fs.BG)
    ax.set_xlim(0, 23); ax.set_ylim(0, 7.4); ax.axis("off")
    h, g = 1.25, fs.HERO
    # lane A: sampling to the card
    ya = 5.3
    box(ax, 0.3, ya, 2.7, h, "ADXL345", "SPI1, INT1 at 100 Hz", fs.SUBTLE)
    box(ax, 3.9, ya, 2.5, h, "EXTI1 ISR", "NVIC 5, notifies", fs.SUBTLE)
    box(ax, 7.3, ya, 2.7, h, "accel_task", "priority 5", g, tc=g)
    box(ax, 11.0, ya, 2.4, h, "logger_q", "160 items", fs.SUBTLE)
    box(ax, 14.4, ya, 2.9, h, "logger_task", "priority 2, FatFs", fs.AMBER, tc=fs.AMBER)
    box(ax, 18.3, ya, 2.5, h, "SD card", "SPI2 + DMA1", fs.SUBTLE)
    for a, b in ((3.0, 3.9), (6.4, 7.3), (10.0, 11.0), (13.4, 14.4), (17.3, 18.3)):
        arrow(ax, (a, ya + h / 2), (b, ya + h / 2))
    ax.text(19.55, ya - 0.28, "DMA TC irq (6)\nwakes logger", ha="center", va="top", color=fs.MUTED, fontsize=12)
    arrow(ax, (19.55, ya - 0.1), (15.85, ya - 0.1), fs.AMBER)
    # lane B: the second consumer, directly below accel_task
    yb = 3.0
    box(ax, 7.3, yb, 2.7, h, "infer_q", "32 items", fs.SUBTLE)
    box(ax, 10.6, yb, 2.2, h, "infer_task", "priority 3, RMS", fs.HERO, tc=g)
    arrow(ax, (8.65, ya), (8.65, yb + h))
    arrow(ax, (10.0, yb + h / 2), (10.6, yb + h / 2))
    ax.text(8.85, 4.6, "one post per logged sample", ha="left", va="center", color=fs.MUTED, fontsize=12)
    # lane C: environment, joins the logger queue from below
    yc = 0.9
    box(ax, 0.3, yc, 2.7, h, "BMP280", "I2C1, 1 Hz", fs.SUBTLE)
    box(ax, 7.3, yc, 2.7, h, "env_task", "priority 4", fs.TEXT)
    arrow(ax, (3.0, yc + h / 2), (7.3, yc + h / 2))
    ax.plot([10.0, 13.0], [yc + h / 2] * 2, color=fs.MUTED, lw=2.2)
    arrow(ax, (13.0, yc + h / 2), (13.0, ya))
    ax.text(0.3, 0.3, "cli_task (1): UART RX stream buffer      console TX: mutex, stream buffer, TXE ISR      "
            "init_task (6): bring-up, then exits", color=fs.MUTED, fontsize=14, va="center")
    fs.bars(fig, "Tasks and queues", "STM32F407 · FreeRTOS V11.3.2 · static allocation",
            "Each bus has one owner task, and each consumer has its own queue, so a slow one only drops its own samples.")
    fig.savefig(OUT, dpi=fs.DPI, facecolor=fs.BG)


main()

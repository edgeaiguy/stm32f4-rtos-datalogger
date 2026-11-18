"""EdgeAIGuy figure style for matplotlib: brand colours, fonts and the title/caption bars."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager

BG, SURFACE, RAISED = "#0d0f0e", "#141716", "#1b1f1d"
HERO, AMBER, DANGER = "#3ddc84", "#f5a623", "#e5484d"
TEXT, MUTED, SUBTLE = "#e8ede9", "#8a9690", "#4d5c54"
HAIRLINE = (1, 1, 1, 0.07)

WIDTH_PX, DPI = 2300, 100


def _family(*names):
    have = {f.name for f in font_manager.fontManager.ttflist}
    for n in names:
        if n in have:
            return n
    return "DejaVu Sans"


MONO = _family("IBM Plex Mono", "DejaVu Sans Mono")
SANS = _family("IBM Plex Sans", "DejaVu Sans")


def new_figure(height_px):
    plt.rcParams.update({"font.family": MONO, "text.color": TEXT, "axes.edgecolor": SUBTLE,
                         "axes.labelcolor": MUTED, "xtick.color": MUTED, "ytick.color": MUTED})
    return plt.figure(figsize=(WIDTH_PX / DPI, height_px / DPI), dpi=DPI, facecolor=BG)


def bars(fig, title, config, caption, top_px=64, bottom_px=96):
    """Title bar (left title, right config line) and caption bar, in figure pixels."""
    w, h = fig.get_size_inches() * fig.dpi
    for y0, y1 in ((h - top_px, h), (0, bottom_px)):
        fig.patches.append(plt.Rectangle((0, y0 / h), 1, (y1 - y0) / h, transform=fig.transFigure,
                                         facecolor=SURFACE, edgecolor="none", zorder=0))
    fig.text(24 / w, 1 - top_px / 2 / h, title, color=TEXT, fontsize=16, va="center", fontweight="bold")
    fig.text(1 - 24 / w, 1 - top_px / 2 / h, config, color=MUTED, fontsize=14, va="center", ha="right")
    fig.text(24 / w, bottom_px / 2 / h, caption, color=TEXT, fontsize=21, va="center", family=SANS)

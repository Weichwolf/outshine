"""Separate morphological controls from equal-result growth search timings."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from blade_surface import Blade
from matplotlib.collections import LineCollection


def development_figure(report):
    folder = (
        Path(__file__).resolve().parents[3]
        / "build/experiments/vegetation/developing-leaves"
    )
    figure = plt.figure(figsize=(16, 10), layout="constrained")
    grid = figure.add_gridspec(2, 3, height_ratios=[3, 1])
    blade = Blade(**report["cases"][0]["blade"])
    t = np.linspace(0, 1, 257)
    width, _ = blade.width_profile(t)
    for index, (name, title) in enumerate(
        (
            ("frozen", "Alle Quellen in fertiger Form"),
            ("uniform", "Gleichförmiges Wachstum"),
            ("developed-0", "Ortsabhängige Längsdehnung"),
        )
    ):
        axis = figure.add_subplot(grid[0, index])
        with np.load(folder / f"{name}-veins.npz") as graph:
            p, parents, radii = graph["positions"], graph["parents"], graph["radii"]
            lines = np.stack((p[parents[1:]], p[1:]), axis=1)
        axis.add_collection(
            LineCollection(lines, linewidths=radii[1:] * 5000, colors="#236c52")
        )
        axis.plot(width, t * blade.length, color="#555555", lw=0.7)
        axis.plot(-width, t * blade.length, color="#555555", lw=0.7)
        axis.autoscale()
        axis.set_aspect("equal")
        axis.set_title(title)
        axis.set_xlabel("m")
    axis = figure.add_subplot(grid[1, :])
    for method, offset, label, colour in (
        ("dense", -0.18, "Dichte Suche", "#b98625"),
        ("tree", 0.18, "cKDTree", "#236c52"),
    ):
        times = np.array([c["seconds"][method] for c in report["cases"]]) * 1000
        median = np.median(times, axis=1)
        bars = axis.bar(
            np.arange(3) + offset,
            median,
            0.36,
            label=label,
            color=colour,
            yerr=[median - times.min(axis=1), times.max(axis=1) - median],
            capsize=4,
        )
        axis.bar_label(bars, fmt="%.0f", padding=4)
    axis.set_xticks(np.arange(3), ["Glatt", "Gelappt · 2 Quellen ungelöst", "Schmal"])
    axis.set_ylabel("Identisches Wachstum · lokale Laufzeit ms")
    axis.legend(frameon=False)
    axis.margins(y=0.25)
    figure.suptitle(
        "Blattwachstum steuert Aderarchitektur\nDrei Formproben; eigene geometrische Dehnung, keine botanisch validierten Arten"
    )
    return figure

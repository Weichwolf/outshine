"""Diffuse leaf front/back response and measured preparation/lookup costs."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def leaf_library_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4), layout="constrained")
    path = (
        Path(__file__).resolve().parents[3]
        / "build/experiments/vegetation/leaf-library/shader-vectors.npz"
    )
    labels = ("Chl 45 / Ant 0.2", "Chl 8 / Ant 0.2", "Chl 10 / Ant 8")
    with np.load(path) as vectors:
        for response, label in zip(vectors["response"], labels):
            luminance = response @ (0.2126, 0.7152, 0.0722)
            axes[0].plot(vectors["light_cosine"], luminance, label=label)
    axes[0].axvline(0, color="grey", lw=0.7)
    axes[0].set(
        xlabel="Light cosine: backlight < 0, frontlight > 0",
        ylabel="Linear response per unit incident radiance",
        title="Own diffuse thin-sheet reference",
    )
    axes[0].legend(fontsize=8)
    costs = report["costs"]
    values = costs["median_ms"]
    names = ("spectral_ms", "table_ms", "table_and_sheet_ms")
    bars = axes[1].bar(
        ("Spectral reference", "Prepared table", "Table + diffuse sheet"),
        [values[n] for n in names],
    )
    axes[1].set(
        yscale="log",
        ylabel="CPU median, ms",
        title=f"Same {costs['same_queries']} queries; five alternating runs",
    )
    for bar, name in zip(bars, names):
        axes[1].text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() * 1.1,
            f"{values[name]:.3f}",
            ha="center",
            fontsize=9,
        )
    return figure

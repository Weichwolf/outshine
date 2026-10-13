"""Equivalent cubic projection workloads and isolated joined-product stage costs."""

import matplotlib.pyplot as plt
import numpy as np


def wood_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    names = ("scalar", "batch", "bounded")
    samples = np.array([report["projection"]["samples_s"][n] for n in names]) * 1000
    median = np.median(samples, axis=1)
    error = np.stack((median - samples.min(axis=1), samples.max(axis=1) - median))
    bars = axes[0].bar(
        ["Einzelne Nullstellen", "Batch · alle Segmente", "Batch · nur Kandidaten"],
        median,
        color=["#b98625", "#82936e", "#236c52"],
        yerr=error,
        capsize=4,
    )
    axes[0].bar_label(bars, fmt="%.1f", padding=5)
    axes[0].tick_params(axis="x", rotation=12)
    axes[0].set_ylabel("Identische nächste Kurvenpunkte · CPU ms")
    axes[0].margins(y=0.22)
    row = report["seeds"][0]["work"]["stages"]
    keys = ("source_s", "collars_s", "normal_blend_s", "union_s", "bark_s")
    bars = axes[1].bar(
        np.arange(len(keys)), [row[k] * 1000 for k in keys], color="#236c52"
    )
    axes[1].bar_label(bars, fmt="%.1f", padding=5)
    axes[1].set_xticks(
        np.arange(len(keys)),
        ["Achsen", "Kragen", "Normalen", "Vereinigung", "Rinde"],
        rotation=18,
    )
    axes[1].set_ylabel("Seed 17 · getrennte Produktstufen · CPU ms")
    axes[1].margins(y=0.22)
    figure.suptitle(
        "Gemeinsame räumliche Holzfläche · lokale Astkragen und geerbte Rinde"
    )
    return figure

"""Verified physical placement workloads and exact variable-size graph requests."""

import matplotlib.pyplot as plt
import numpy as np


def placement_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    cases = report["costs"]
    labels = [
        f"{c['sites']:,} Standorte\n{'konstant' if len(set(c['spacing_profile'])) == 1 else 'Profil'}"
        for c in cases
    ]
    for method, delta, color in (
        ("scalar", -0.18, "#b98625"),
        ("batch", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in cases]) * 1000
        median = np.median(samples, axis=1)
        spans = np.array([median - samples.min(axis=1), samples.max(axis=1) - median])
        bars = axes[0].bar(
            np.arange(len(cases)) + delta,
            median,
            0.36,
            color=color,
            label="Skalare Referenz"
            if method == "scalar"
            else "Kompakte Batch-Auswertung",
            yerr=spans,
            capsize=4,
        )
        axes[0].bar_label(bars, fmt="%.2f", padding=5)
    axes[0].set_xticks(np.arange(len(cases)), labels)
    axes[0].set_ylabel("Gleiche vollständige Standortfolgen · CPU ms")
    axes[0].legend(frameon=False, fontsize=8)
    axes[0].margins(y=0.24)
    graph = report["checks"]["graph"]
    bars = axes[1].bar(
        ["Kapazitätsgrenze", "Tatsächliche Posen", "Neue Posen beim Treffer"],
        [graph["capacity"], graph["actual_poses"], graph["warm_new_poses"]],
        color=["#b98625", "#236c52", "#82936e"],
    )
    axes[1].bar_label(bars, padding=5)
    axes[1].set_ylabel("Variable Elternlängen · logische Posen")
    axes[1].tick_params(axis="x", rotation=10)
    axes[1].margins(y=0.24)
    figure.suptitle("Blattfolgen und Astquirle · physische Abstände vor Geometrie")
    return figure

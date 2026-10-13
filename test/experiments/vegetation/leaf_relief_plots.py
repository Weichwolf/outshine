"""Measured radius-dependent relief costs and centerline height response."""

import matplotlib.pyplot as plt
import numpy as np


def relief_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(16, 7.5), layout="constrained")
    for method, offset, label in (
        ("dense", -0.18, "Alle Segmente"),
        ("indexed", 0.18, "Räumliche Vorauswahl"),
    ):
        samples = np.array([c["seconds"][method] for c in report["cases"]]) * 1000
        median = np.median(samples, axis=1)
        bars = axes[0].bar(
            np.arange(3) + offset,
            median,
            0.36,
            label=label,
            yerr=[median - samples.min(axis=1), samples.max(axis=1) - median],
            capsize=4,
        )
        axes[0].bar_label(bars, fmt="%.0f", padding=4)
    axes[0].set_xticks(np.arange(3), ["Glatt", "Gelappt", "Schmal"])
    axes[0].set_ylabel("CPU ms · 128×128 Feldwerte · Median aus 3 Läufen")
    axes[0].legend()
    axes[0].margins(y=0.2)
    radius = np.linspace(0.01, 1, 100)
    axes[1].plot(radius, np.ones_like(radius), label="Bisheriges normiertes Relief")
    axes[1].plot(radius, radius, label="Radiusabhängige Hülle")
    axes[1].set(
        xlabel="Konstanter Aderradius / größter Aderradius",
        ylabel="Mittellinienhöhe / maximale Reliefhöhe",
        ylim=(0, 1.1),
    )
    axes[1].legend()
    figure.suptitle(
        "Blattrelief · dünne Adern werden flacher\nGleiche Geometrie; identische dichte/indexierte Ergebnisse. Endkarten separat in 512×512."
    )
    return figure

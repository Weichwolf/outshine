"""Comparable complete field build costs and explicit exported chart memory."""

import matplotlib.pyplot as plt
import numpy as np


def layer_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(10, 4.2), layout="constrained")
    x = np.arange(len(report["seeds"]))
    for method, offset, colour in (
        ("dense", -0.18, "#82936e"),
        ("indexed", 0.18, "#236c52"),
    ):
        rows = np.array([r["samples_s"][method] for r in report["seeds"]]) * 1000
        median = np.median(rows, axis=1)
        error = np.stack((median - rows.min(axis=1), rows.max(axis=1) - median))
        bars = axes[0].bar(
            x + offset, median, 0.36, color=colour, label=method, yerr=error, capsize=3
        )
        axes[0].bar_label(bars, fmt="%.1f", padding=5)
    bars = axes[1].bar(
        x,
        [r["work"]["indexed"]["attribute_bytes"] / 1048576 for r in report["seeds"]],
        color="#236c52",
    )
    axes[1].bar_label(bars, fmt="%.3f", padding=5)
    for axis in axes:
        axis.set_xticks(x, [f"Seed {r['seed']}" for r in report["seeds"]])
        axis.margins(y=0.25)
    axes[0].set_ylabel("Vier vollständige Rindenfelder · CPU ms")
    axes[1].set_ylabel("Exportierte Layerattribute · MiB")
    axes[0].legend(frameon=False)
    figure.suptitle("Rindenfelder getrennt auswerten · Ergebnis überblenden")
    return figure

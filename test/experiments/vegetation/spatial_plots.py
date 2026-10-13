"""Continuous frame quality and equivalent whole-graph placement costs."""

import matplotlib.pyplot as plt
import numpy as np


def spatial_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    fields = report["fields"]
    n = [c["intervals_per_segment"] for c in fields]
    axes[0].plot(n, [c["frame_error"] for c in fields], "o-", color="#236c52")
    axes[0].axhline(1e-5, ls="--", color="#b98625", label="Geprüfte Fehleranforderung")
    axes[0].set_xscale("log", base=2)
    axes[0].set_yscale("log")
    axes[0].set_xticks(n, [str(x) for x in n])
    axes[0].set_xlabel("Feste Rahmenintervalle je Kubiksegment")
    axes[0].set_ylabel("Maximaler Richtungsfehler gegen DOP853")
    axes[0].legend(frameon=False, fontsize=8)
    samples = (
        np.array([report["graph_cost"]["samples_s"][k] for k in ("False", "True")])
        * 1000
    )
    median = np.median(samples, axis=1)
    error = np.stack((median - samples.min(axis=1), samples.max(axis=1) - median))
    bars = axes[1].bar(
        ["Einzelne Rahmen", "Gemeinsame Batches"],
        median,
        color=["#b98625", "#236c52"],
        yerr=error,
        capsize=4,
    )
    axes[1].bar_label(bars, fmt="%.2f", padding=5)
    axes[1].margins(y=0.25)
    axes[1].set_ylabel("Gleiche vollständige Blattposen · CPU ms")
    figure.suptitle(
        "Räumliche Astachsen · LOD-unabhängige Anker und Differentialnormalen"
    )
    return figure

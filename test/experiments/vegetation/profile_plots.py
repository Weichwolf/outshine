"""Current profile curves and equal-query CPU costs."""

import matplotlib.pyplot as plt
import numpy as np
from profile_curve import BezierCurve
from profile_experiment import CURVES


def profile_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.5), layout="constrained")
    labels = ("Linear", "Gleitend", "Verdickung", "Wurzelanlauf")
    for (name, controls), label in zip(CURVES.items(), labels, strict=True):
        curve = BezierCurve(controls)
        t = np.linspace(0, 1, 256)
        axes[0].plot(t, curve.value(t), label=label, linewidth=2)
    axes[0].set(
        xlabel="Position am Ast [normiert]",
        ylabel="Radiusfaktor [dimensionslos]",
        title="Vier eigene Radiusprofile",
    )
    axes[0].legend(frameon=False, fontsize=9)
    cases = list(report["cases"].values())
    x = np.arange(len(cases))
    for offset, method, label in [
        (-0.25, "exact", "Brent einzeln"),
        (0, "batched_exact", "Chandrupatla-Batch"),
        (0.25, "lookup", "Vorbereitete Tabelle"),
    ]:
        values = [c["cost"][method]["median_ms"] for c in cases]
        axes[1].bar(x + offset, values, 0.23, label=label)
    axes[1].set_xticks(x, labels, rotation=15)
    axes[1].set_yscale("log")
    axes[1].set(
        ylabel="2.048 gleiche Abfragen [ms, Median]",
        title="Lokale Python-/SciPy-Kosten",
    )
    axes[1].legend(frameon=False, fontsize=9)
    for axis in axes:
        axis.spines[["top", "right"]].set_visible(False)
        axis.grid(axis="y", alpha=0.2)
        axis.set_axisbelow(True)
    figure.suptitle("Profile · Originalfunktionen numerisch geprüft", fontsize=14)
    return figure

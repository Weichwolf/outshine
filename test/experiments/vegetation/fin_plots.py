"""Matched complete fin transformations, including differential corner normals."""

import matplotlib.pyplot as plt
import numpy as np


def fin_figure(report):
    figure, axis = plt.subplots(figsize=(9, 4.8), layout="constrained")
    labels = [case["style"] for case in report["cases"]]
    for method, offset, colour in (
        ("solve", -0.18, "#b98625"),
        ("cofactor", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in report["cases"]]) * 1000
        medians = np.median(samples, axis=1)
        spans = np.array([medians - samples.min(axis=1), samples.max(axis=1) - medians])
        bars = axis.bar(
            np.arange(len(labels)) + offset,
            medians,
            0.36,
            color=colour,
            label="NumPy solve" if method == "solve" else "Kofaktor-Normalen",
            yerr=spans,
            capsize=3,
        )
        axis.bar_label(bars, fmt="%.2f", padding=4)
    axis.set_xticks(np.arange(len(labels)), labels)
    axis.set_ylabel("Vollständige Meshabbildung · CPU ms")
    axis.margins(y=0.3)
    axis.legend(frameon=False)
    figure.suptitle(
        "Rindenfortsätze · gleiche Positionen, UVs, Faces und Rindenfelder\n"
        "Je 4080 Dreiecke · fünf wechselnde Läufe · kein GPU-/Gerätenachweis"
    )
    return figure

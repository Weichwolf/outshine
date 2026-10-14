"""Same complete frond cross-section meshes, independent normal methods and reload."""

import matplotlib.pyplot as plt
import numpy as np


def frond_library_figure(report):
    figure, axis = plt.subplots(figsize=(9, 4.8), layout="constrained")
    labels = [f"k = {case['curvature_per_m']:g} m⁻¹" for case in report["cases"]]
    for method, offset, colour in (
        ("solve", -0.18, "#b98625"),
        ("cofactor", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in report["cases"]]) * 1000
        medians = np.median(samples, axis=1)
        bars = axis.bar(
            np.arange(len(labels)) + offset,
            medians,
            0.36,
            color=colour,
            label="NumPy solve" if method == "solve" else "Kofaktor-Normalen",
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axis.bar_label(bars, fmt="%.2f", padding=4)
    axis.set_xticks(np.arange(len(labels)), labels)
    axis.set_ylabel("Gleiche vollständige Meshabbildung · CPU ms")
    axis.margins(y=0.25)
    axis.legend(frameon=False)
    figure.suptitle(
        "Frond-Curl · Mittelachse, Sourcefaces/UVs/Organ-IDs erhalten\n"
        "25080 Dreiecke · fünf wechselnde Läufe · keine GPU-/Gerätenachweise"
    )
    return figure

"""Equal complete free-spine mesh outputs, batched versus individual points."""

import matplotlib.pyplot as plt
import numpy as np


def spatial_frond_figure(report):
    figure, axis = plt.subplots(figsize=(9, 4.8), layout="constrained")
    for method, offset, colour in (
        ("scalar", -0.18, "#b98625"),
        ("batch", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in report["cases"]]) * 1000
        medians = np.median(samples, axis=1)
        bars = axis.bar(
            np.arange(3) + offset,
            medians,
            0.36,
            color=colour,
            label="Einzelpunkte" if method == "scalar" else "Gebündelte Auswertung",
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axis.bar_label(bars, fmt="%.2f", padding=4)
    axis.set_xticks(
        np.arange(3), ("Planare Biegung", "Räumliche Biegung", "Wendende Achse")
    )
    axis.set_ylabel("Vollständige gleiche Meshabbildung · CPU ms · logarithmisch")
    axis.set_yscale("log")
    axis.margins(y=0.30)
    axis.legend(frameon=False)
    figure.suptitle(
        "Freie räumliche Frondführung · feste Mittelachse und lokale Formen\n"
        "6204 Dreiecke · fünf wechselnde Läufe · keine GPU-/Gerätenachweise"
    )
    return figure

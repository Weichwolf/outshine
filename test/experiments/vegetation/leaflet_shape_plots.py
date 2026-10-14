"""Complete equal-output local leaflet mesh costs, including unique shapes."""

import matplotlib.pyplot as plt
import numpy as np


def leaflet_shape_figure(report):
    figure, axis = plt.subplots(figsize=(9, 4.8), layout="constrained")
    labels = [
        f"{case['name']}\n{case['shape_variants']} Formvarianten"
        for case in report["cases"]
    ]
    for method, offset, colour in (
        ("individual", -0.18, "#b98625"),
        ("shared", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in report["cases"]]) * 1000
        medians = np.median(samples, axis=1)
        bars = axis.bar(
            np.arange(len(labels)) + offset,
            medians,
            0.36,
            color=colour,
            label="Einzelprototypen"
            if method == "individual"
            else "Geteilte Prototypen",
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axis.bar_label(bars, fmt="%.2f", padding=4)
    axis.set_xticks(np.arange(len(labels)), labels)
    axis.set_ylabel("Gleiche vollständige Quellflächen · CPU ms")
    axis.margins(y=0.25)
    axis.legend(frameon=False)
    figure.suptitle(
        "Lokale Fiederformen · Biegung, Wölbung, Drehung\n"
        "25080 Dreiecke · fünf wechselnde Läufe · individuelle Formen sparen keine Arbeit"
    )
    return figure

"""Complete and selective cold work versus shared warm canonical plant records."""

import matplotlib.pyplot as plt
import numpy as np


def plant_library_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(13, 4.8), layout="constrained")
    labels = ["Seed 17", "Seed 0", "Seed 2⁶⁴−1"]
    for axis, phase, title in zip(
        axes,
        ("cold_full", "cold_partial", "warm_partial"),
        (
            "64 Achsen + 571 Blätter",
            "Ein angefragter Zweig · frisch",
            "Ein angefragter Zweig · Treffer",
        ),
        strict=True,
    ):
        for method, offset, colour in (
            ("before", -0.18, "#b98625"),
            ("after", 0.18, "#236c52"),
        ):
            samples = (
                np.array([case["samples_s"][phase][method] for case in report["cases"]])
                * 1000
            )
            medians = np.median(samples, axis=1)
            bars = axis.bar(
                np.arange(3) + offset,
                medians,
                0.36,
                color=colour,
                label="Bisher"
                if method == "before"
                else "Bibliothek + unveränderliche Daten",
                yerr=np.array(
                    [medians - samples.min(axis=1), samples.max(axis=1) - medians]
                ),
                capsize=3,
            )
            axis.bar_label(
                bars,
                fmt="%.3f" if phase == "warm_partial" else "%.2f",
                padding=4,
                fontsize=8,
            )
        axis.set_xticks(np.arange(3), labels, rotation=15)
        axis.set_ylabel("Gleiche kanonische Ausgabe · CPU ms")
        axis.set_title(title)
        axis.margins(y=0.3)
    axes[0].legend(frameon=False, fontsize=8)
    figure.suptitle(
        "Vollständiger Pflanzenplan · IDs, Positionen und Originalmeshes exakt erhalten\n"
        "Fünf wechselnde Läufe · Aufbau im Kern langsamer; Bedarf begrenzt Arbeit, kein Gerätebeleg"
    )
    return figure

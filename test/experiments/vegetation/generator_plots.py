"""Visible generator connections and equivalent requested-subset pose costs."""

import matplotlib.pyplot as plt
import numpy as np


def generator_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    positions = {
        0: (0.1, 0.5),
        1: (0.3, 0.5),
        2: (0.55, 0.7),
        3: (0.86, 0.85),
        4: (0.86, 0.22),
    }
    labels = {0: "Wurzel", 1: "Stamm", 2: "Äste", 3: "Blätter", 4: "Fronds"}
    for parent, child in ((0, 1), (1, 2), (2, 3), (1, 4), (2, 4)):
        axes[0].annotate(
            "",
            xy=positions[child],
            xytext=positions[parent],
            arrowprops={"arrowstyle": "->", "color": "#b98625", "lw": 1.7},
        )
    for key, point in positions.items():
        count = report["summary"]["counts"][str(key)]
        axes[0].text(
            *point,
            f"{labels[key]}\n{count} Instanz(en)",
            ha="center",
            va="center",
            bbox={
                "boxstyle": "round,pad=.4",
                "facecolor": "#e1ebd5",
                "edgecolor": "#236c52",
            },
        )
    axes[0].set_xlim(0, 1.03)
    axes[0].set_ylim(0, 1.03)
    axes[0].set_title("Gemeinsame Hierarchie · eine Quelle je Form")
    axes[0].set_axis_off()
    names = ("all_then_select", "requested", "cached")
    samples = np.array(
        [
            [case["elapsed_s"] * 1000 for case in report["samples"][name]]
            for name in names
        ]
    )
    median = np.median(samples, axis=1)
    error = np.stack((median - samples.min(axis=1), samples.max(axis=1) - median))
    axes[1].bar(
        np.arange(3),
        median,
        color=("#b98625", "#236c52", "#82936e"),
        yerr=error,
        capsize=4,
    )
    axes[1].set_xticks(
        np.arange(3),
        ("Alles vor Teilwahl", "Nur angefordert", "Schon vorhanden"),
        rotation=12,
    )
    axes[1].set_yscale("log")
    axes[1].set_title("Identische sieben Frondinstanzen · ms (log)")
    for x, value in enumerate(median):
        axes[1].text(x, value, f"{value:.3f}", ha="center", va="bottom")
    figure.suptitle("Gemischter Generatorgraph · Bedarf vor Platzierungen und Meshing")
    return figure

"""Needle geometry costs, stable output counts, and observed approximation errors."""

import matplotlib.pyplot as plt
import numpy as np


def needle_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(19, 7), layout="constrained")
    x = np.arange(3)
    width = 0.35
    labels = ("Pinus · 2", "Pinus · 5", "Picea · 1")
    for offset, method, label in (
        (-width / 2, "scalar", "Einzeln · Ecknormalen"),
        (width / 2, "batch", "Gebündelt · Ecknormalen"),
    ):
        values = [
            1000 * np.median(c["geometry_seconds"][method]) for c in report["cases"]
        ]
        bars = axes[0].bar(x + offset, values, width, label=label)
        axes[0].bar_label(bars, fmt="%.2f", padding=4)
    axes[0].set(
        xticks=x,
        xticklabels=labels,
        ylabel="Lokale CPU ms · gleiches Nahmesh · 3 Läufe",
        yscale="log",
    )
    axes[0].legend(fontsize=8)
    axes[0].margins(y=0.2)
    for offset, detail, label in (
        (-width / 2, "near", "Nah"),
        (width / 2, "coarse", "Grob"),
    ):
        bars = axes[1].bar(
            x + offset,
            [c[detail + "_instanced_triangles"] for c in report["cases"]],
            width,
            label=label,
        )
        axes[1].bar_label(bars, fmt="%d", padding=4)
    axes[1].set(
        xticks=x,
        xticklabels=labels,
        ylabel="Dreiecke mit Instanzen · IDs/Anker unverändert",
    )
    axes[1].legend()
    axes[1].margins(y=0.2)
    x = np.arange(len(report["checks"]["cases"]))
    for offset, index, label in ((-width / 2, 0, "Nah"), (width / 2, 1, "Grob")):
        axes[2].bar(
            x + offset,
            [
                -100 * c["volume_relative_errors"][index]
                for c in report["checks"]["cases"]
            ],
            width,
            label=label,
        )
    axes[2].set(
        xticks=x,
        xticklabels=("Ellipse", "Twist", "Rhombus", "Gerade", "S2", "S3", "S5", "S8"),
        ylabel="Beobachtetes Volumendefizit · %",
    )
    axes[2].legend()
    figure.suptitle(
        "Räumliche Nadeln · Halbkreis-/Kreissektoren mit gemeinsamer Bündelbasis\nMaterial und LOD-Deckung offen; lokale CPU-Messung ohne GPU-Budgetbeleg"
    )
    return figure

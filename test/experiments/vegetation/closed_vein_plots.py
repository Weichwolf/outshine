"""Complete closed-growth cost and explicit unresolved-source accounting."""

import matplotlib.pyplot as plt
import numpy as np


def closed_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(16, 7.5), layout="constrained")
    for method, offset, label, colour in (
        ("dense", -0.18, "Vollständige Nachbarschaftsprüfung", "#b98625"),
        ("sectors", 0.18, "Exakte Sektor-Vorauswahl", "#236c52"),
    ):
        samples = np.array([c["seconds"][method] for c in report["cases"]]) * 1000
        median = np.median(samples, axis=1)
        bars = axes[0].bar(
            np.arange(3) + offset,
            median,
            0.36,
            label=label,
            color=colour,
            yerr=[median - samples.min(axis=1), samples.max(axis=1) - median],
            capsize=4,
        )
        axes[0].bar_label(bars, fmt="%.0f", padding=5)
    axes[0].set_xticks(np.arange(3), ["Glatte Form", "Gelappte Form", "Schmale Form"])
    axes[0].set_ylabel("Identisches Wachstum aus je 64 Quellen · lokale CPU ms")
    axes[0].legend(frameon=False, fontsize=9)
    axes[0].margins(y=0.25)
    axes[1].axis("off")
    table = axes[1].table(
        cellText=[
            [str(c["nodes"]), str(c["noded"]["cycles"]), str(c["remaining_sources"])]
            for c in report["cases"]
        ],
        rowLabels=["Glatt", "Gelappt", "Schmal"],
        colLabels=["Knoten", "Schleifen", "Ungelöste Quellen"],
        loc="center",
        cellLoc="center",
        bbox=[0.15, 0.33, 0.85, 0.33],
    )
    table.auto_set_font_size(False)
    table.set_fontsize(11)
    table[(2, 2)].set_facecolor("#efc1ac")
    axes[1].set_title("Nach Kreuzungsverknüpfung · je ein zusammenhängendes Netz")
    figure.suptitle(
        "Geschlossene Feinadern · drei Funktionsformen\n"
        "Wachstum: Median/Spannweite aus 3 Läufen; Materialaufbau und Einlesen separat",
        fontsize=15,
    )
    return figure

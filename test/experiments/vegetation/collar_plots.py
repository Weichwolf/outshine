"""Sampling alternatives and measured full collar pipeline stages."""

import matplotlib.pyplot as plt
import numpy as np


def collar_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    rows = report["cases"]
    x = np.arange(len(rows))
    for index, (method, title, colour) in enumerate(
        (("dense", "Dicht", "#687b89"), ("blocked", "Blockweise", "#337758"))
    ):
        axes[0].bar(
            x + (index - 0.5) * 0.32,
            [r["samplers"]["methods"][method]["median_ms"] for r in rows],
            width=0.3,
            label=title,
            color=colour,
        )
    bottom = np.zeros(len(rows))
    stages = (
        (
            "Erzeugung",
            [
                r["samplers"]["methods"][r["samplers"]["selected"]]["median_ms"]
                for r in rows
            ],
            "#337758",
        ),
        ("Normalen", [r["shading_ms"] for r in rows], "#b99046"),
        ("Vereinigung/Ausgabe", [r["union_ms"] for r in rows], "#687b89"),
    )
    for title, values, colour in stages:
        axes[1].bar(x, values, bottom=bottom, label=title, color=colour)
        bottom += values
    axes[0].set(
        title="63 lokale Hüllen · identische Meshes",
        ylabel="Lokale Zeit [ms, Median aus drei Läufen]",
    )
    axes[1].set(
        title="Vollständige Anschlüsse · gemischte Messbasis",
        ylabel="Lokale Zeit [ms, Phasen separat]",
    )
    for axis in axes:
        axis.set_xticks(x, [f"Seed {r['seed']}" for r in rows])
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.legend(fontsize=8)
        axis.spines[["top", "right"]].set_visible(False)
    figure.suptitle("Lokale Astkragen · Python und native Bibliothekskerne")
    return figure

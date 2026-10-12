"""Same-input comparisons of complete plant wood unions."""

import matplotlib.pyplot as plt
import numpy as np


def plant_surface_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    x = np.arange(len(report["cases"]))
    for index, (method, title, colour) in enumerate(
        (
            ("batch", "Gemeinsam", "#337758"),
            ("sequential", "Nacheinander", "#b99046"),
        )
    ):
        rows = [case["methods"][method] for case in report["cases"]]
        for axis, key in zip(axes, ("median_ms", "triangles"), strict=True):
            axis.bar(
                x + (index - 0.5) * 0.32,
                [row[key] for row in rows],
                width=0.3,
                color=colour,
                label=title,
            )
    axes[0].set(
        title="Vereinigung + Normalen + Ausgabe",
        ylabel="Lokale Zeit [ms, Median aus drei Läufen]",
    )
    axes[1].set(
        title="64 Holzachsen · vollständiges Oberflächenprodukt", ylabel="Dreiecke"
    )
    for axis in axes:
        axis.set_xticks(x, [f"Seed {case['seed']}" for case in report["cases"]])
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.spines[["top", "right"]].set_visible(False)
    axes[0].legend()
    figure.suptitle("Pflanzenoberflächen · Python mit nativem Manifold-Kern")
    return figure

"""Same-input mesh costs and the explicit orientation counterexample."""

import matplotlib.pyplot as plt
import numpy as np


def blade_library_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.8), layout="constrained")
    x, width = np.arange(len(report["cases"])), 0.35
    for offset, key, label in (
        (-width / 2, "before", "Bisher"),
        (width / 2, "after", "Bibliothek + Prüfung"),
    ):
        values = [np.median(c["mesh_seconds"][key]) * 1000 for c in report["cases"]]
        bars = axes[0].bar(x + offset, values, width, label=label)
        axes[0].bar_label(bars, fmt="%.2f", padding=4)
    axes[0].set(
        xticks=x,
        xticklabels=["Grundform", "Gras", "Quercus", "Fagus", "Betula"],
        ylabel="Gleiches 64×4-Mesh · ms",
        title="Fünf wechselnde CPU-Läufe",
    )
    axes[0].legend(fontsize=8)
    axes[0].margins(y=0.2)
    bars = axes[1].bar(
        [0, 1],
        [report["twist"]["coarse_triangles"], report["twist"]["accepted_triangles"]],
        color=["#b98625", "#236c52"],
    )
    axes[1].bar_label(bars, fmt="%d", padding=4)
    axes[1].set(
        xticks=[0, 1],
        xticklabels=["16×4 · abgelehnt", "512×8 · gültig"],
        ylabel="Dreiecke",
        title=f"Twist-Gegenfall: {report['twist']['coarse_bad_corner_orientations']} falsche Eckorientierungen",
    )
    axes[1].margins(y=0.2)
    figure.suptitle(
        "Blatt-/Graskern · bisherige Geometrie bitgleich · Quadraturdaten unveränderlich\nOrientierung geprüft; Material, Artenfreigabe und LOD-Deckung weiterhin offen"
    )
    return figure

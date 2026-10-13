"""Topology-aware terminal coverage and separately measured material construction."""

import matplotlib.pyplot as plt
import numpy as np


def terminal_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(16, 7.5), layout="constrained")
    bottom = np.zeros(3)
    for key, label in (
        ("free_tips", "Freie Enden"),
        ("attached_tips", "Übergänge zu Feinadern"),
        ("protected_tips", "Geschützte Verzweigungen"),
    ):
        values = np.array([c[key] for c in report["cases"]])
        bars = axes[0].bar(np.arange(3), values, bottom=bottom, label=label)
        axes[0].bar_label(bars, label_type="center")
        bottom += values
    axes[0].set_ylabel("Hauptaderenden nach vollständigem Netz")
    axes[0].legend()
    axes[0].margins(y=0.25)
    bars = axes[1].bar(np.arange(3), [c["material_seconds"] for c in report["cases"]])
    axes[1].bar_label(bars, fmt="%.2f", padding=4)
    axes[1].set_ylabel("Lokale CPU s · vollständige 512×512-Karten, ein Lauf")
    axes[1].margins(y=0.2)
    for axis in axes:
        axis.set_xticks(np.arange(3), ["Glatt", "Gelappt", "Schmal"])
    figure.suptitle(
        "Aderenden · freie Spitzen und verbundene Übergänge\nGraph unverändert; abgeleitete Materialradien separat gecacht"
    )
    return figure

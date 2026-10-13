"""Same spatial-input preparation, warm frame lookup and complete mesh CPU costs."""

import matplotlib.pyplot as plt
import numpy as np


def spine_library_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(13, 4.5), layout="constrained")
    labels = [case["name"] for case in report["cases"]]
    for axis, phase, title in zip(
        axes,
        ("cold_field", "warm_field", "cold_mesh"),
        (
            "Frischer Plan + 513 Rahmen",
            "513 warme Rahmen",
            "Frischer Plan + 48×12-Mesh",
        ),
        strict=True,
    ):
        for method, offset, colour in (
            ("before", -0.18, "#b98625"),
            ("after", 0.18, "#236c52"),
        ):
            samples = (
                np.array([c["samples_s"][phase][method] for c in report["cases"]])
                * 1000
            )
            median = np.median(samples, axis=1)
            spans = np.array(
                [median - samples.min(axis=1), samples.max(axis=1) - median]
            )
            bars = axis.bar(
                np.arange(len(labels)) + offset,
                median,
                0.36,
                color=colour,
                label="Bisher" if method == "before" else "Bibliothek + Prüfungen",
                yerr=spans,
                capsize=3,
            )
            axis.bar_label(bars, fmt="%.2f", padding=4, fontsize=8)
        axis.set_xticks(np.arange(len(labels)), labels, rotation=15)
        axis.set_title(title)
        axis.set_ylabel("Gleiche vollständige Eingaben · CPU ms")
        axis.margins(y=0.26)
    axes[0].legend(frameon=False, fontsize=8)
    figure.suptitle(
        "Räumlicher Astkern · vorbereitete Daten unveränderlich, Feld/Mesh exakt erhalten\n"
        "Fünf wechselnde Läufe · Aufbauschritte/Lookup getrennt, kein nativer LOD-/Gerätenachweis"
    )
    return figure

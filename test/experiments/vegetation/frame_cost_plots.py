"""Same complete plant outputs after fixed last-axis 3D component evaluation."""

import matplotlib.pyplot as plt
import numpy as np


def frame_cost_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(13, 4.8), layout="constrained")
    labels = [f"{c['name']}\nSeed {c['seed']}" for c in report["cases"]]
    for axis, phase, title in zip(
        axes,
        ("cold_full", "cold_partial", "warm_partial"),
        ("Vollständiger Plan · frisch", "Ein Zweig · frisch", "Ein Zweig · Treffer"),
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
            medians = np.median(samples, axis=1)
            bars = axis.bar(
                np.arange(len(labels)) + offset,
                medians,
                0.36,
                color=colour,
                label="NumPy cross" if method == "before" else "Feste 3D-Komponenten",
                yerr=np.array(
                    [medians - samples.min(axis=1), samples.max(axis=1) - medians]
                ),
                capsize=3,
            )
            axis.bar_label(
                bars,
                fmt="%.3f" if phase == "warm_partial" else "%.1f",
                padding=4,
                fontsize=8,
            )
        axis.set_xticks(np.arange(len(labels)), labels, rotation=15)
        axis.set_ylabel("Gleiche kanonische Ausgabe · CPU ms")
        axis.set_title(title)
        axis.margins(y=0.25)
    axes[0].legend(frameon=False, fontsize=8)
    figure.suptitle(
        "Rahmenarbeit · 64/4511 Achsen und 571/37800 Blätter unverändert\n"
        "Fünf wechselnde Läufe · kein Inhalt reduziert, kein nativer/GPU-/Gerätebeleg"
    )
    return figure

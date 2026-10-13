"""Equal complete site outputs; dense, sparse and refused-demand CPU workloads."""

import matplotlib.pyplot as plt
import numpy as np


def placement_library_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.4), layout="constrained")
    for axis, cases in zip(
        axes, (report["cases"][:2], report["cases"][2:]), strict=True
    ):
        for method, offset, color in (
            ("before", -0.18, "#b98625"),
            ("after", 0.18, "#236c52"),
        ):
            samples = np.array([c["samples_s"][method] for c in cases]) * 1000
            medians = np.median(samples, axis=1)
            spans = np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            )
            bars = axis.bar(
                np.arange(len(cases)) + offset,
                medians,
                0.36,
                color=color,
                label="Bisher" if method == "before" else "Bibliothek · unveränderlich",
                yerr=spans,
                capsize=4,
            )
            axis.bar_label(bars, fmt="%.2f", padding=5)
        axis.set_xticks(
            np.arange(len(cases)),
            [
                f"{c['name']}\n{c['parents']:,} Eltern / {c['sites']:,} Standorte"
                for c in cases
            ],
        )
        axis.set_ylabel("Gleiche vollständige Anfrage · CPU ms")
        axis.margins(y=0.28)
        axis.legend(frameon=False, fontsize=8)
    axes[0].set_title("256 belegte Eltern · konstante / profilierte Abstände")
    axes[1].set_title("65.536 Eltern · nur einer belegt / Budget verweigert")
    figure.suptitle(
        "Blattfolgen-/Quirlkern · Arrays und Graphprodukte exakt erhalten\n"
        "Fünf wechselnde Läufe · lokale Python-Kosten, kein nativer LOD-/Gerätenachweis"
    )
    return figure

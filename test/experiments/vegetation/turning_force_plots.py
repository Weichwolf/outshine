"""Same complete turning-axis preparation with identical quality requirements."""

import matplotlib.pyplot as plt
import numpy as np


def turning_force_figure(report):
    figure, axis = plt.subplots(figsize=(10, 4.8), layout="constrained")
    labels = [c["name"] for c in report["cases"]]
    for method, offset, colour in (
        ("RK45", -0.18, "#b98625"),
        ("DOP853", 0.18, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in report["cases"]]) * 1000
        medians = np.median(samples, axis=1)
        bars = axis.bar(
            np.arange(len(labels)) + offset,
            medians,
            0.36,
            color=colour,
            label=method,
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axis.bar_label(bars, fmt="%.2f", padding=4)
    axis.set_xticks(np.arange(len(labels)), labels)
    axis.set_ylabel("Vollständige Achsvorbereitung · CPU ms")
    axis.margins(y=0.3)
    axis.legend(frameon=False)
    figure.suptitle(
        "Curl/Gnarl/Planar/Return · fünf wechselnde Läufe · 33 Stationen\nGleiche Qualitätsanforderung · keine Original-/Frame-/Gerätenachweise"
    )
    return figure

"""Costs of identical complete guide-deformed frond products."""

import matplotlib.pyplot as plt
import numpy as np


def frond_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.2), layout="constrained")
    names = ("Gerade", "Querwölbung", "Biegung + Roll", "Breitenprofil")
    for axis, field, factor, title in zip(
        axes,
        ("elapsed_s", "peak_rss_bytes"),
        (1000, 1 / 2**20),
        (
            "Vollständige Verformung · ms (log)",
            "Prozessspitze einschließlich Eingabe · MiB",
        ),
    ):
        for method, shift, label, colour in (
            ("scalar", -0.18, "Punktweise", "#b98625"),
            ("batch", 0.18, "Gebündelt", "#236c52"),
        ):
            samples = np.array(
                [
                    [sample[field] * factor for sample in case["samples"][method]]
                    for case in report["cases"]
                ]
            )
            median = np.median(samples, axis=1)
            error = np.stack(
                (median - samples.min(axis=1), samples.max(axis=1) - median)
            )
            axis.bar(
                np.arange(len(names)) + shift,
                median,
                0.34,
                color=colour,
                label=label,
                yerr=error,
                capsize=3,
            )
        axis.set_xticks(np.arange(len(names)), names, rotation=15)
        axis.set_title(title)
        axis.legend(fontsize=8)
    axes[0].set_yscale("log")
    figure.suptitle("33 stabile Teilblätter · 6.204 Dreiecke · identische Produkte")
    return figure

"""Measured time and full-process memory for identical complete bark fields."""

import matplotlib.pyplot as plt
import numpy as np


def bark_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(10, 4.2), layout="constrained")
    methods = ("dense", "indexed")
    colours = ("#b98625", "#236c52")
    for axis, field, unit, title in zip(
        axes,
        ("elapsed_s", "peak_rss_bytes"),
        (1000, 1 / 2**20),
        ("Koordinatenerzeugung · ms", "Prozessspitze einschließlich Eingabe · MiB"),
    ):
        samples = np.array(
            [
                [sample[field] * unit for sample in report["samples"][method]]
                for method in methods
            ]
        )
        medians = np.median(samples, axis=1)
        error = np.stack((medians - samples.min(axis=1), samples.max(axis=1) - medians))
        axis.bar((0, 1), medians, color=colours, yerr=error, capsize=4)
        axis.set_xticks((0, 1), ("Alle Achsen × Punkte", "Räumlich begrenzt"))
        axis.set_title(title)
        for x, value in enumerate(medians):
            axis.text(x, value, f"{value:,.1f}", ha="center", va="bottom")
        axis.set_ylim(0, samples.max() * 1.18)
    figure.suptitle(
        f"Identische Rindenkoordinaten · {report['axes']} Achsen / {report['vertices']:,} Vertices"
    )
    return figure

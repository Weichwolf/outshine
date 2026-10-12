"""Continuous-curve surface probes and costs for complete spatial branch products."""

import matplotlib.pyplot as plt
import numpy as np


def curved_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    names = list(report["cases"])
    centres = np.arange(len(names))
    for index, (label, colour) in enumerate(
        zip(
            ("Dicht 96", "Block 64", "Block 96", "Block 128"),
            ("#687b89", "#73a19b", "#337758", "#b99046"),
            strict=True,
        )
    ):
        rows = [report["cases"][name]["cases"][index] for name in names]
        positions = centres + (index - 1.5) * 0.20
        for axis, key, scale in zip(
            axes,
            ("median_ms", "maximum_observed_surface_error_m"),
            (1, 1000),
            strict=True,
        ):
            bars = axis.bar(
                positions,
                [row[key] * scale for row in rows],
                0.18,
                color=colour,
                label=label,
            )
            for bar, row in zip(bars, rows, strict=True):
                if not row["same_quality_pass"]:
                    bar.set_hatch("///")
                    bar.set_edgecolor("#555")
    maximum = max(
        row["median_ms"] for case in report["cases"].values() for row in case["cases"]
    )
    axes[0].set_ylim(0, maximum * 1.25)
    axes[0].set(
        ylabel="Erzeugung [lokale CPU ms, Median]",
        title="Unabhängige Astkurven · drei Wiederholungen",
    )
    axes[1].set(
        ylabel="Maximale beobachtete Oberflächenabweichung [mm]",
        title="Kontinuierliche Polynomreferenz · beidseitige Proben",
    )
    axes[1].axhline(5, color="#555", linestyle="--")
    axes[0].legend(fontsize=9)
    for axis in axes:
        axis.set_xticks(centres, ("Bögen", "Seitenäste", "Räumlich"))
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.spines[["top", "right"]].set_visible(False)
    figure.suptitle("Gekrümmte Astanschlüsse · gemeinsame vollständige Oberflächen")
    return figure

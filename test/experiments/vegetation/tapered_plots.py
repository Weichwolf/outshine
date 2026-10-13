"""Equal-input quality and generation costs for tapered fork procedures."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch


def tapered_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    names = list(report["cases"])
    labels = (
        "Dicht 128",
        "Block 128",
        "Block 192",
        "Projektion 192",
        "Unterteilung 128",
        "Adaptiv 4 mm",
        "Adaptiv 2 mm",
    )
    colours = (
        "#687b89",
        "#72a1b0",
        "#b0a472",
        "#9c82ad",
        "#337758",
        "#2879bb",
        "#cf7941",
    )
    centres = np.arange(len(names))
    for index, (label, colour) in enumerate(zip(labels, colours, strict=True)):
        rows = [report["cases"][name]["cases"][index] for name in names]
        positions = centres + (index - 3) * 0.12
        for axis, key, scale in zip(
            axes,
            ("median_ms", "maximum_radial_probe_error_m"),
            (1, 1000),
            strict=True,
        ):
            bars = axis.bar(
                positions,
                [row[key] * scale for row in rows],
                0.11,
                color=colour,
                label=label,
            )
            for bar, row in zip(bars, rows, strict=True):
                if not row["same_quality_pass"]:
                    bar.set_hatch("///")
                    bar.set_edgecolor("#555")
    axes[0].set(
        ylabel="Erzeugung [lokale CPU ms, Median]",
        title="Gleiche Formdaten · drei Wiederholungen",
    )
    axes[1].set(
        ylabel="Maximaler beobachteter radialer Fehler [mm]",
        title="355 unabhängige Probestrahlen je Form",
    )
    axes[1].axhline(5, color="#555", linestyle="--")
    maximum_time = max(
        row["median_ms"] for case in report["cases"].values() for row in case["cases"]
    )
    axes[0].set_ylim(0, maximum_time * 1.55)
    handles, labels = axes[0].get_legend_handles_labels()
    handles.append(Patch(facecolor="white", edgecolor="#555", hatch="///"))
    axes[0].legend(handles, labels + ["Probeanforderung verfehlt"], fontsize=8)
    for axis in axes:
        axis.set_xticks(centres, ("Symmetrisch", "Ungleich", "Räumlich"))
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.spines[["top", "right"]].set_visible(False)
    figure.suptitle("Verjüngte Astgabeln · gleiches Fehlerziel: 5 mm")
    return figure

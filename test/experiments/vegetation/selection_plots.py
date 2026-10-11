"""Coverage versus draw/cache work on identical directional foliage inputs."""

import matplotlib.pyplot as plt
import numpy as np

METHODS = (
    "random15",
    "random30",
    "visible15",
    "visible30",
    "spread15",
    "spread30",
    "cloud15",
    "cloud60",
    "cloud120",
    "views32x8",
    "views32x16",
    "views64x16",
    "views64x32",
)
LABELS = (
    "Zufall · 15",
    "Zufall · 30",
    "Sichtfläche · 15",
    "Sichtfläche · 30",
    "Verteilt · 15",
    "Verteilt · 30",
    "Ebenen · 15 mm",
    "Ebenen · 60 mm",
    "Ebenen · 120 mm",
    "32 Ansichten · 8×8",
    "32 Ansichten · 16×16",
    "64 Ansichten · 16×16",
    "64 Ansichten · 32×32",
)


def coverage_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 7), layout="constrained")
    groups = [
        [case for case in report["study"] if case["method"] == method]
        for method in METHODS
    ]
    values = [
        np.array(
            [view["coverage_l1_over_union"] for case in group for view in case["views"]]
        )
        * 100
        for group in groups
    ]
    colours = ["#879d42"] * 6 + ["#236c52"] * 3 + ["#b98625"] * 4
    y = np.arange(len(METHODS))
    axes[0].barh(y, [value.mean() for value in values], color=colours, label="Mittel")
    axes[0].scatter(
        [value.max() for value in values],
        y,
        marker="|",
        color="#352e22",
        s=150,
        label="Maximum",
    )
    axes[0].set(
        xlabel="Deckungsabweichung / Vereinigung [%]",
        title="4 Gruppen × 24 unabhängige Richtungen",
        yticks=y,
        yticklabels=LABELS,
    )
    axes[0].legend(frameon=False)
    counts = np.array([[case["draw_charts"] for case in group] for group in groups])
    medians = np.median(counts, axis=1)
    axes[1].barh(
        y,
        medians,
        color=colours,
        xerr=(medians - counts.min(axis=1), counts.max(axis=1) - medians),
        capsize=3,
    )
    axes[1].set(
        xlabel="Karten je Draw [Anzahl]",
        title="Median und Spanne · gleiche Quellblätter",
        yticks=y,
        yticklabels=[],
    )
    for axis in axes:
        axis.invert_yaxis()
    figure.suptitle(
        "Flächenausgleich allein erhält keine räumliche Deckung · 8×8 Auswertung"
    )
    return figure


def storage_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 7), layout="constrained")
    groups = [
        [case for case in report["study"] if case["method"] == method]
        for method in METHODS
    ]
    y = np.arange(len(METHODS))
    for axis, field, divisor, title, unit in (
        (
            axes[0],
            "rgba8_base_bytes",
            1024,
            "Unbeleuchtete Farbe und Normalen · RGBA8",
            "Basiskarten [KiB je Gruppe]",
        ),
        (
            axes[1],
            "generation_ms",
            1,
            "Ein lokaler Aufbau je Gruppe · kein GPU-Budget",
            "Erzeugung [ms je Gruppe]",
        ),
    ):
        values = (
            np.array(
                [
                    [
                        case[field]
                        + (
                            case["stats"].get("visibility_ms", 0)
                            if field == "generation_ms"
                            else 0
                        )
                        for case in group
                    ]
                    for group in groups
                ]
            )
            / divisor
        )
        medians = np.median(values, axis=1)
        axis.barh(
            y,
            medians,
            xerr=(medians - values.min(axis=1), values.max(axis=1) - medians),
            color="#236c52",
            capsize=3,
        )
        axis.set(
            title=title,
            xlabel=unit,
            yticks=y,
            yticklabels=LABELS if axis == axes[0] else [],
        )
        axis.invert_yaxis()
    figure.suptitle(
        "Alle Rundum-Ansichten vorgehalten · Atlasränder, Mips und GPU-Formate offen"
    )
    return figure

"""Figures for equal-input foliage-cloud measurements and original render pixels."""

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image


def group_figure(report):
    methods = ("axes1", "axes4", "greedy8", "greedy16")
    labels = (
        "Achsen\n1 Tiefe",
        "Achsen\n4 Tiefen",
        "Organebenen\n8 × 8",
        "Organebenen\n16 × 16",
    )
    cases = [
        [case for case in report["study"] if case["method"] == method]
        for method in methods
    ]
    errors = [
        np.array(
            [view["coverage_l1_over_union"] for case in group for view in case["views"]]
        )
        * 100
        for group in cases
    ]
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.5), layout="constrained")
    colours = ("#a8bba4", "#879d42", "#236c52", "#b98625")
    x = np.arange(len(methods))
    axes[0].bar(x, [values.mean() for values in errors], color=colours, label="Mittel")
    axes[0].scatter(
        x,
        [values.max() for values in errors],
        marker="_",
        s=250,
        color="#352e22",
        label="Maximum",
    )
    axes[0].set(
        ylabel="Deckungsabweichung / Vereinigung [%]",
        title="4 Gruppen × 24 Richtungen · 8 px",
    )
    axes[0].legend(frameon=False)
    counts = np.array([[case["charts"] for case in group] for group in cases])
    medians = np.median(counts, axis=1)
    axes[1].bar(
        x,
        medians,
        color=colours,
        yerr=(medians - counts.min(axis=1), counts.max(axis=1) - medians),
        capsize=4,
    )
    axes[1].set(
        ylabel="Karten je 60-Blatt-Gruppe [Anzahl]",
        title="Kartenbedarf · Median und Spanne",
    )
    for axis in axes:
        axis.set_xticks(x, labels)
    figure.suptitle("Räumliche Referenz gegen gruppierte Ebenen", fontsize=14)
    return figure


def raster_figure(report):
    cost = report["raster"]["cost"]
    samples = np.array(
        [cost[key]["samples_ms"] for key in ("per_triangle", "bounded_batches")]
    )
    medians = np.median(samples, axis=1)
    figure, axis = plt.subplots(figsize=(8, 4.2), layout="constrained")
    bars = axis.bar(
        ("Dreieckweise", "Begrenzte Batches"),
        medians,
        color=("#b98625", "#236c52"),
        yerr=(medians - samples.min(axis=1), samples.max(axis=1) - medians),
        capsize=4,
    )
    for bar, value in zip(bars, medians):
        axis.annotate(
            f"{value:.2f} ms",
            (bar.get_x() + bar.get_width() / 2, value),
            xytext=(0, 12),
            textcoords="offset points",
            ha="center",
        )
    axis.set(
        yscale="log",
        ylabel="CPU-Rasterzeit [ms · logarithmisch]",
        title="3.960 identische Dreiecke → 64 × 64 Raster",
    )
    axis.set_ylim(bottom=samples.min() * 0.6, top=samples.max() * 2)
    figure.suptitle("Fünf lokale Läufe · Median und Minimum/Maximum", fontsize=14)
    return figure


def render_figure(experiments):
    images = []
    for name in ("cloud-reference", "cloud-tree"):
        with Image.open(experiments / "render" / f"{name}.png") as image:
            images.append(np.asarray(image.convert("RGB"))[260:520, 550:890])
    difference = np.abs(images[0].astype(int) - images[1]).max(axis=-1)
    figure, axes = plt.subplots(1, 3, figsize=(11, 4.2), layout="constrained")
    for axis, pixels, title in zip(
        axes[:2], images, ("Räumliche Referenz", "Gruppierte Karten")
    ):
        axis.imshow(pixels, interpolation="nearest")
        axis.set_title(title)
    shown = axes[2].imshow(
        difference, vmin=0, vmax=32, cmap="magma", interpolation="nearest"
    )
    axes[2].set_title("Größter RGB-Kanalunterschied")
    figure.colorbar(
        shown, ax=axes[2], shrink=0.7, label="Absoluter Unterschied [0–255]"
    )
    for axis in axes:
        axis.axis("off")
    figure.suptitle("Originalpixel · gleicher Ausschnitt aus 1280 × 720", fontsize=14)
    return figure

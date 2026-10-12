"""Current fork procedure costs and independent sampled shape error."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

LABELS = {
    "radial": "Gleichmäßig radial",
    "voxel": "Marching Cubes",
    "blocked": "Marching Cubes · Blockauswahl",
    "adaptive": "Adaptiv radial",
}


def fork_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.8), layout="constrained")
    rows = report["cases"]["symmetric"]["comparison"]["cases"]
    for method, label in LABELS.items():
        cases = [row for row in rows if row["method"] == method]
        line = axes[0].loglog(
            [row["median_ms"] for row in cases],
            [row["maximum_radial_probe_error_m"] * 1000 for row in cases],
            "o-",
            label=label,
        )
        failed = [row for row in cases if not row["same_quality_pass"]]
        axes[0].scatter(
            [row["median_ms"] for row in failed],
            [row["maximum_radial_probe_error_m"] * 1000 for row in failed],
            marker="x",
            s=70,
            color=line[0].get_color(),
            zorder=4,
        )
    axes[0].axhline(
        5, color="#777", linestyle="--", label="Gemeinsame Stichprobenanforderung"
    )
    axes[0].set(
        xlabel="Vollständige Erzeugung [lokale CPU ms, Median]",
        ylabel="Maximaler beobachteter radialer Fehler [mm]",
        title="Identische symmetrische Gabel · 355 Probestrahlen",
    )
    handles, labels = axes[0].get_legend_handles_labels()
    handles.append(Line2D([], [], color="#555", marker="x", linestyle="none"))
    axes[0].legend(
        handles, labels + ["Fehleranforderung oder Detailgrenze verfehlt"], fontsize=8
    )
    names = list(report["cases"])
    selected = [report["cases"][name]["comparison"]["selected"] for name in names]
    positions = np.arange(len(names))
    axes[1].bar(positions, [row["product_bytes"] / (1024 * 1024) for row in selected])
    axes[1].set(
        xticks=positions,
        xticklabels=["Symmetrisch", "Ungleich", "Räumlich"],
        ylabel="Gespeicherte Mesharrays [MiB]",
        title="Gewählte gemeinsame Oberflächen",
    )
    for x, row in zip(positions, selected, strict=True):
        axes[1].text(
            x,
            row["product_bytes"] / (1024 * 1024),
            f"{row['triangles']:,} Dreiecke\n{row['median_ms']:.0f} ms",
            ha="center",
            va="bottom",
            fontsize=9,
        )
    axes[1].margins(y=0.22)
    for axis in axes:
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.spines[["top", "right"]].set_visible(False)
    figure.suptitle(
        "Organische Astgabeln · gleiche Feldform, kontrollierte Verfahrensvergleiche"
    )
    return figure

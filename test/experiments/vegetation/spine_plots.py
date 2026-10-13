"""Current spatial-frame accuracy and same-quality CPU comparisons."""

import matplotlib.pyplot as plt
import numpy as np

LABELS = {
    "reflection_loop": "Reflexionsschleife",
    "reflection": "Doppelreflexion · Quaternionen",
    "rotation": "Tangentendrehung",
}


def spine_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.8), layout="constrained")
    rows = report["comparison"]["equal_sample_cases"]
    for name, label in LABELS.items():
        cases = [row for row in rows if row["method"] == name]
        axes[0].loglog(
            [row["segments"] for row in cases],
            [row["maximum_frame_vector_error"] for row in cases],
            "o-",
            label=label,
        )
    axes[0].axhline(
        1e-5, color="#888", linestyle="--", label="Gleiche Fehleranforderung"
    )
    axes[0].set(
        xlabel="Axiale Segmente der analytischen Helix",
        ylabel="Maximaler Richtungsfehler [Vektorabstand]",
        title="Rahmenfehler an identischen Stationen",
    )
    axes[0].legend(fontsize=8)
    passing = report["comparison"]["fastest_passing_case"]
    names = list(LABELS)
    positions = np.arange(len(names))
    axes[1].bar(positions, [passing[name]["cost"]["median_ms"] for name in names])
    axes[1].set(
        xticks=positions,
        xticklabels=[
            f"{LABELS[name]}\n{passing[name]['segments']} Segmente" for name in names
        ],
        ylabel="Lokale Python-CPU [ms, Median]",
        title="Günstigster geprüfter Fall mit Fehler ≤ 1e−5",
    )
    axes[1].tick_params(axis="x", labelsize=8)
    for axis in axes:
        axis.grid(axis="y", alpha=0.25)
        axis.set_axisbelow(True)
        axis.spines[["top", "right"]].set_visible(False)
    figure.suptitle("Räumliche Astachsen · unabhängige analytische Rahmenreferenz")
    return figure

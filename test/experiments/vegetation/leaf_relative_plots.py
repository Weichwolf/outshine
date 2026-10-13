"""Relative-neighbor cost evidence and a counterexample to fixed nearest-k influence."""

import matplotlib.pyplot as plt
import numpy as np


def relative_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(16, 7.5), layout="constrained")
    for method, shift, label, colour in (
        ("dense", -0.18, "Vollständige Paarprüfung", "#b98625"),
        ("sectors", 0.18, "Exakte Sektor-Vorauswahl", "#236c52"),
    ):
        samples = np.array([c["seconds"][method] for c in report["cases"]]) * 1000
        median = np.median(samples, axis=1)
        bars = axes[0].bar(
            np.arange(3) + shift,
            median,
            0.36,
            yerr=[median - samples.min(axis=1), samples.max(axis=1) - median],
            capsize=4,
            label=label,
            color=colour,
        )
        axes[0].bar_label(bars, fmt="%.1f", padding=5)
    axes[0].set_xticks(np.arange(3), [f"{c['nodes']} Knoten" for c in report["cases"]])
    axes[0].set_ylabel("128 identische Quellen · lokale CPU ms")
    axes[0].set_title("Gleiche Nachbarn · Median und Spannweite aus 5 Läufen")
    axes[0].legend(frameon=False, fontsize=9)
    axes[0].margins(y=0.22)
    case = report["checks"]["fixed_eight_nearest_counterexample"]
    nodes, source = np.array(case["nodes"]), np.array(case["source"])
    axes[1].scatter(*nodes.T, color="#82936e", s=35, zorder=3)
    for index in case["neighbors"]:
        segment = np.array([source, nodes[index]])
        axes[1].plot(*segment.T, color="#236c52", linewidth=2)
    axes[1].scatter(*source, color="#242e28", marker="*", s=150, zorder=4)
    axes[1].annotate("Quelle", source, xytext=(0.1, -0.6))
    axes[1].annotate(
        "9 nahe Knoten",
        nodes[0],
        xytext=(-1.8, 1.3),
        arrowprops={"arrowstyle": "->"},
    )
    axes[1].annotate(
        "Ferner relativer Nachbar\nfehlt unter den nächsten 8",
        nodes[-1],
        xytext=(4, 1.3),
        arrowprops={"arrowstyle": "->"},
    )
    axes[1].set(xlim=(-2, 11), ylim=(-2, 2), xlabel="Normierte Testkoordinaten")
    axes[1].set_aspect("equal")
    axes[1].set_title("Warum eine feste Nachbarzahl nicht genügt")
    figure.suptitle(
        "Geschlossene Blattadern · geprüfter Nachbarschaftsbaustein\n"
        "Getrennte Nachbarschaftsmessung · vollständiges Wachstum separat geprüft",
        fontsize=15,
    )
    return figure

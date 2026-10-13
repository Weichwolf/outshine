"""Measured prefilter accuracy and cost, separate from runtime shading."""

import matplotlib.pyplot as plt
import numpy as np


def filter_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(16, 6), layout="constrained")
    cases = report["cases"]
    for name, label, offset, colour in (
        ("point", "Texelmittelpunkt", -0.18, "#b98625"),
        ("integrated", "6×6-Quadratur", 0.18, "#236c52"),
    ):
        values = [c["errors"][name]["rms"] for c in cases]
        bars = axes[0].bar(
            np.arange(3) + offset, values, 0.36, label=label, color=colour
        )
        axes[0].bar_label(bars, fmt="%.5f", padding=5)
    axes[0].set_yscale("log")
    axes[0].set_ylabel("RMS-Feldfehler gegen 16×16-Quadratur · je 1024 aktive Texel")
    for name, label, offset, colour in (
        ("all_texels", "Alle Texel integrieren", -0.18, "#b98625"),
        ("support_only", "Konservative Vorauswahl", 0.18, "#236c52"),
    ):
        values = np.array([c["seconds"][name] for c in cases]) * 1000
        median = np.median(values, axis=1)
        bars = axes[1].bar(
            np.arange(3) + offset,
            median,
            0.36,
            label=label,
            color=colour,
            yerr=[median - values.min(axis=1), values.max(axis=1) - median],
            capsize=4,
        )
        axes[1].bar_label(bars, fmt="%.0f", padding=5)
    axes[1].set_ylabel("Identische 512×512-Integrale · lokale CPU ms")
    for axis in axes:
        axis.set_xticks(np.arange(3), ["Glatt", "Gelappt", "Schmal"])
        axis.legend(frameon=False)
        axis.margins(y=0.25)
    figure.suptitle(
        "Feinadern vor Rasterung filtern\n"
        "UV-Flächenmittel; Referenz ist numerisch, kein Beweis vollständiger Aliasfreiheit"
    )
    return figure

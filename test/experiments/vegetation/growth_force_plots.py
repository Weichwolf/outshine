"""Same-quality growth preparation and independent callback costs."""

import matplotlib.pyplot as plt
import numpy as np


def growth_force_figure(report):
    figure, axes = plt.subplots(1, 2, figsize=(12, 4.8), layout="constrained")
    cases = [c for c in report["cases"] if c["scale"] == 1]
    for method, offset, colour in (
        ("DOP853", -0.2, "#b98625"),
        ("RK45", 0.2, "#236c52"),
    ):
        samples = np.array([c["samples_s"][method] for c in cases]) * 1000
        medians = np.median(samples, axis=1)
        bars = axes[0].bar(
            np.arange(3) + offset,
            medians,
            0.36,
            color=colour,
            label=method,
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axes[0].bar_label(bars, fmt="%.2f", padding=3)
    analytic = np.median(cases[0]["samples_s"]["analytic"]) * 1000
    axes[0].scatter(
        0,
        analytic,
        color="#3458a2",
        marker="D",
        label=f"Konstante Richtung direkt: {analytic:.2f} ms",
        zorder=5,
    )
    for method, offset, colour in (
        ("vector", -0.2, "#b98625"),
        ("scalar", 0.2, "#236c52"),
    ):
        samples = (
            np.array([c["samples_s"][method] for c in report["callback_cases"]]) * 1000
        )
        medians = np.median(samples, axis=1)
        bars = axes[1].bar(
            np.arange(3) + offset,
            medians,
            0.36,
            color=colour,
            label="NumPy-Callbacks"
            if method == "vector"
            else "Vorbereitete skalare Felder",
            yerr=np.array(
                [medians - samples.min(axis=1), samples.max(axis=1) - medians]
            ),
            capsize=3,
        )
        axes[1].bar_label(bars, fmt="%.2f", padding=3)
    for axis, title in zip(
        axes,
        ("Solver · gleiche Eingaben und Toleranz", "Callbacks · derselbe RK45-Solver"),
        strict=True,
    ):
        axis.set_xticks(np.arange(3), ("Richtung", "Profile", "Anziehung"))
        axis.set_ylabel("Vollständige Achsvorbereitung · CPU ms")
        axis.set_title(title)
        axis.margins(y=0.3)
        axis.legend(frameon=False, fontsize=8)
    figure.suptitle(
        "Eigene Wachstumskräfte · fünf wechselnde Läufe · 33 Kurvenstationen\nKeine mechanische Steifheits-, native Frame- oder Gerätemessung"
    )
    return figure

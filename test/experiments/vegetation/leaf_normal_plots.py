"""Normal distribution retention, mip costs, and a multimodal counterexample."""

import matplotlib.pyplot as plt
import numpy as np


def normal_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(19, 7), layout="constrained")
    for case in report["cases"]:
        axes[0].plot(
            range(10),
            [c["mean_slope_variance"] for c in case["levels"]],
            "o-",
            label=f"Form {case['index'] + 1}",
        )
    axes[0].set(
        xlabel="Material-Mipstufe · 512² bis 1²",
        ylabel="Mittlere gespeicherte Neigungsvarianz",
    )
    axes[0].legend()
    width = 0.35
    x = np.arange(3)
    for offset, method, label in (
        (-width / 2, "direct", "Direkt aus Basis"),
        (width / 2, "pyramid", "Hierarchisch"),
    ):
        bars = axes[1].bar(
            x + offset,
            [1000 * np.median(c["seconds"][method]) for c in report["cases"]],
            width,
            label=label,
        )
        axes[1].bar_label(bars, fmt="%.1f", padding=4)
    axes[1].set(
        xticks=x,
        xticklabels=("Form 1", "Form 2", "Form 3"),
        ylabel="Lokale CPU ms · gleiche 10 Mipstufen · 3 Läufe",
    )
    axes[1].legend()
    axes[1].margins(y=0.25)
    x = np.linspace(-0.5, 0.5, 501)
    sigma = 0.06
    delta = 0.12
    gaussian = lambda x, s: np.exp(-0.5 * (x / s) ** 2) / (s * np.sqrt(2 * np.pi))
    axes[2].plot(
        x,
        (gaussian(x - delta, sigma) + gaussian(x + delta, sigma)) / 2,
        label="Zwei Neigungsgruppen · Referenz",
    )
    axes[2].plot(
        x,
        gaussian(x, np.sqrt(sigma**2 + delta**2)),
        label="Mittel + Varianz · Einzelverteilung",
    )
    axes[2].plot(x, gaussian(x, sigma), label="Nur gemittelte Normale", linestyle="--")
    axes[2].set(
        xlabel="Projizierte Neigung · künstliches Gegenbeispiel",
        ylabel="Normierte eindimensionale Dichte",
    )
    axes[2].legend(fontsize=8)
    figure.suptitle(
        "Blattnormalen · physische Ableitung, vollständige Momenthierarchie\nIsotrope Rauheitsübertragung auf GGX ist eine Näherung; mehrere Glanzrichtungen bleiben offen"
    )
    return figure

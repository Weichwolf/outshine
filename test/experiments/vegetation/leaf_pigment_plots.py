"""Optical reference spectra, interpolation error, and matched-query costs."""

import matplotlib.pyplot as plt
import numpy as np


def pigment_figure(report):
    figure, axes = plt.subplots(1, 3, figsize=(19, 7), layout="constrained")
    for sample, color in zip(report["spectra"], ("#287748", "#b69719", "#923c30")):
        label = f"Chl {sample['state'][0]:g}, Ant {sample['state'][1]:g} µg/cm²"
        axes[0].plot(sample["nm"], sample["reflection"], color=color, label=label)
        axes[0].plot(sample["nm"], sample["transmission"], color=color, linestyle="--")
    axes[0].set(
        xlabel="Wellenlänge · nm",
        ylabel="Reflexion (durchgezogen) / Transmission (gestrichelt)",
        ylim=(0, 0.65),
    )
    axes[0].legend(fontsize=8)
    sizes = [c["size"] for c in report["candidates"]]
    for key, label in (
        ("max_error", "Gamut nach Interpolation"),
        ("gamut_before_interpolation_error", "Gamut davor · Zufallsproben"),
    ):
        axes[1].plot(sizes, [c[key] for c in report["candidates"]], "o-", label=label)
    axes[1].axhline(
        report["error_limit"], color="black", linestyle=":", label="Prüfgrenze 1/1024"
    )
    axes[1].set(
        xlabel="Tabellengröße je Pigmentachse",
        ylabel="Maximaler linearer RGB-Fehler der Prüfproben",
        yscale="log",
    )
    axes[1].legend(fontsize=8)
    values = [1000 * np.median(report["seconds"][m]) for m in ("direct", "table")]
    bars = axes[2].bar(["Direktes PROSPECT-D", "Gewählte Tabelle"], values)
    axes[2].bar_label(bars, fmt="%.3f", padding=4)
    axes[2].set(
        ylabel=f"Lokale CPU ms · {report['cost_probes']} gleiche Anfragen · Median aus 3 Läufen",
        yscale="log",
    )
    axes[2].margins(y=0.2)
    figure.suptitle(
        f"Blattpigmente · externe Optik, eigene Approximation\n{report['selected_size']}×{report['selected_size']} Tabelle; N=1,3, Car=10 µg/cm², Wasser=0,012 cm, Trockenmasse=0,010 g/cm²"
    )
    return figure

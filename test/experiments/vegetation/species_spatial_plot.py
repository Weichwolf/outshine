"""Actual source-site comparison of independent and grouped species marks."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from outshine_ref.vegetation import load_species_catalog


def plot(sites_path, baseline, product, titles=None):
    sites = np.load(sites_path, allow_pickle=False)
    before = np.load(baseline / "choices.npz", allow_pickle=False)
    after = np.load(product / "choices.npz", allow_pickle=False)
    np.testing.assert_array_equal(sites["ids"], before["ids"])
    np.testing.assert_array_equal(before["ids"], after["ids"])
    report = json.loads((product / "comparison.json").read_text())
    if len(report["mixtures"]) != 1:
        raise ValueError(
            "plot currently expects the single actual Koerbersee pilot pool"
        )
    catalog = load_species_catalog()
    colours = plt.get_cmap("tab10").colors
    fig, axes = plt.subplots(1, 2, figsize=(16, 9), dpi=160)
    xy = sites["positions"][:, :2]
    # Same fixed source window, chosen around the actual forest site's median.
    forest = before["species_index"] >= 0
    centre = np.median(xy[forest], axis=0)
    window = np.all(abs(xy - centre) <= 900, axis=1)
    titles = titles or (
        "Independent site marks",
        "World-anchored correlated marks · 60m / 0.85",
    )
    for ax, data, title in zip(
        axes,
        (before, after),
        titles,
        strict=True,
    ):
        unsupported = window & (data["species_index"] < 0)
        ax.scatter(*xy[unsupported].T, s=2, color="#ccc", label="nonforest: unchanged")
        for i, taxon in enumerate(report["mixtures"][0]["mixture"]["taxa"]):
            selected = window & (data["species_index"] == i)
            ax.scatter(
                *xy[selected].T,
                s=3,
                color=colours[i],
                label=catalog.records[taxon]["scientific_name"],
            )
        ax.set(
            aspect="equal",
            xlim=(centre[0] - 900, centre[0] + 900),
            ylim=(centre[1] - 900, centre[1] + 900),
            xlabel="east (m)",
            ylabel="north (m)",
            title=title,
        )
    axes[0].legend(loc="upper left", fontsize=8, markerscale=2)
    fig.suptitle(
        "Koerbersee actual source sites · identical positions / height weights · uncalibrated marking experiment",
        fontsize=13,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(product / "species-spatial.png")
    plt.close(fig)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sites", type=Path, required=True)
    p.add_argument("--baseline", type=Path, required=True)
    p.add_argument("--product", type=Path, required=True)
    p.add_argument("--titles", nargs=2)
    a = p.parse_args()
    plot(a.sites, a.baseline, a.product, a.titles)

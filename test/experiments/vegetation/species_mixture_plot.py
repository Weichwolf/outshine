"""Inspect actual cached site assignments; this is not a Place rendering."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from outshine_ref.vegetation import load_species_catalog


def plot(directory, sites_path):
    report = json.loads((directory / "comparison.json").read_text())
    data = np.load(directory / "choices.npz", allow_pickle=False)
    sites = np.load(sites_path, allow_pickle=False)
    np.testing.assert_array_equal(data["ids"], sites["ids"])
    catalog = load_species_catalog()
    taxa = sorted(
        {taxon for entry in report["mixtures"] for taxon in entry["mixture"]["taxa"]}
    )
    assigned = np.full(len(data["ids"]), -1, dtype=int)
    for group, entry in enumerate(report["mixtures"]):
        for local, taxon in enumerate(entry["mixture"]["taxa"]):
            assigned[
                (data["mixture_index"] == group) & (data["species_index"] == local)
            ] = taxa.index(taxon)
    colours = plt.get_cmap("tab10").colors
    fig, (map_axis, fractions) = plt.subplots(1, 2, figsize=(16, 9), dpi=160)
    xy = sites["positions"][:, :2] / 1000
    unsupported = assigned < 0
    map_axis.scatter(
        *xy[unsupported].T,
        s=0.25,
        color="#c7c7c7",
        label="nonforest / unsupported pilot",
    )
    bins = np.linspace(data["height_m"].min(), data["height_m"].max(), 23)
    total, _ = np.histogram(data["height_m"][~unsupported], bins=bins)
    bottom = np.zeros(len(total))
    for index, taxon in enumerate(taxa):
        selected = assigned == index
        name = catalog.records[taxon]["scientific_name"]
        map_axis.scatter(*xy[selected].T, s=0.3, color=colours[index], label=name)
        counts, _ = np.histogram(data["height_m"][selected], bins=bins)
        ratio = np.divide(counts, total, out=np.zeros(len(total)), where=total > 0)
        fractions.bar(
            bins[:-1],
            ratio,
            width=np.diff(bins),
            align="edge",
            bottom=bottom,
            color=colours[index],
            label=name,
        )
        bottom += ratio
    map_axis.set(
        aspect="equal",
        xlabel="east (km)",
        ylabel="north (km)",
        title="Existing source-site positions; colour = proposed species",
    )
    map_axis.legend(loc="upper left", fontsize=7, markerscale=6)
    fractions.set(
        xlabel="scene/provider height (m)",
        ylabel="fraction of selected forest sites",
        ylim=(0, 1),
        title="Empirical composition by height; authored uncalibrated weights",
    )
    fig.suptitle(
        "Koerbersee species-choice experiment · 53,200 unchanged sites · no plant geometry or image acceptance",
        fontsize=13,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    target = directory / "species-mixture.png"
    fig.savefig(target)
    plt.close(fig)
    print(target)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--sites", type=Path, required=True)
    a = parser.parse_args()
    plot(a.product, a.sites)

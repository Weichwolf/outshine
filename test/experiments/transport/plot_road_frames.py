"""Openable saved-data lighting diagnostic, not a native/PBR scene (#1531)."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = np.load(args.input, allow_pickle=False)
    fig, axes = plt.subplots(4, 3, figsize=(15, 10), layout="constrained")
    light = np.array([0.4, -0.2, 0.9])
    light /= np.linalg.norm(light)
    for group, ordinal in enumerate((8329, 32370)):
        prefix = f"witness_{ordinal}_"
        stations, lateral = (
            data[prefix + "sample_stations_m"],
            data[prefix + "sample_lateral_m"],
        )
        extent = [stations[0], stations[-1], lateral[0], lateral[-1]]
        for column, (key, title) in enumerate(
            (
                (
                    "interpolated_triangle_normals",
                    "Area-weighted triangle vertex normals",
                ),
                (
                    "interpolated_recipe_normals",
                    "Analytic vertex normals, interpolated",
                ),
                ("reference_normals", "Analytic normals evaluated at each sample"),
            )
        ):
            normal = data[prefix + key]
            value = np.maximum(0, normal @ light)
            ax = axes[2 * group, column]
            ax.imshow(
                value.T,
                origin="lower",
                extent=extent,
                aspect="auto",
                vmin=0,
                vmax=1,
                cmap="gray",
            )
            ax.set_title(f"{ordinal}: {title}", fontsize=10)
            ax.set_ylabel("Lateral u [m]")
            ax.set_xlabel("Reference station s [m]")
        for column, (key, title) in enumerate(
            (
                ("triangle_error_degrees", "Triangle interpolation error [degrees]"),
                ("recipe_error_degrees", "Recipe interpolation error [degrees]"),
            )
        ):
            error = data[prefix + key]
            ax = axes[2 * group + 1, column]
            artist = ax.imshow(
                error.T,
                origin="lower",
                extent=extent,
                aspect="auto",
                vmin=0,
                vmax=90,
                cmap="magma",
            )
            ax.set_title(f"{title}; max {error.max():.3f}", fontsize=10)
            ax.set_xlabel("Reference station s [m]")
            ax.set_ylabel("Lateral u [m]")
            fig.colorbar(artist, ax=ax, shrink=0.7)
        ax = axes[2 * group + 1, 2]
        points = data[prefix + "surface_positions_enh_m"]
        ax.plot(stations, points[:, 0, 2], label="right outer edge height")
        ax.plot(stations, points[:, -1, 2], label="left outer edge height")
        ax.set_xlabel("Reference station s [m]")
        ax.set_ylabel("Height [m]")
        ax.legend(fontsize=8)
        ax.set_title("Original analytic edge profiles", fontsize=10)
    fig.suptitle(
        "Same saved recipes and meshes; Lambert diagnostic in metric parameter charts\nChart axes have independent scales. The 5 mm case defeats vertex-only shading; no PBR/device admission."
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=120)
    plt.close(fig)


if __name__ == "__main__":
    main()

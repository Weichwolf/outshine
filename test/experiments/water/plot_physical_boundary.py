"""Local numerical field diagnostics; no native PBR or published gallery (#941)."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.tri as mtri
import numpy as np
from physical_boundary import from_world, rotation, to_world


def plot(output):
    report = json.loads((output / "report.json").read_text())
    original = next(c for c in report["cases"] if c["name"] == "original_ill")
    methods = original["methods"]
    with np.load(output / "original_ill.npz") as data:
        vertices, faces, values = {}, {}, {}
        for method in methods:
            vertices[method], faces[method], values[method] = [], [], []
            for side, anchor in enumerate(original["regional_anchors_lon_lat"]):
                local = data[f"{method}_{side}_vertices"]
                triangle = data[f"{method}_{side}_triangles"]
                body = from_world(
                    original["body_anchor_lon_lat"], to_world(anchor, local)
                )
                p = local[triangle]
                normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                normal = normal @ rotation(anchor, original["body_anchor_lon_lat"])
                vertices[method].append(body)
                faces[method].append(triangle)
                values[method].append(-normal[:, 0] / normal[:, 2])
        seam = data["canonical_vertices"][data["canonical_seam"]]
        worst = np.argmax(methods["separate_fits"]["crease"]["per_segment_degrees"])
        centre = seam[worst].mean(0)
        fig, axes = plt.subplots(1, 3, figsize=(16, 6), layout="constrained")
        for side in range(2):
            v = vertices["shared_body_cut"][side]
            tri = mtri.Triangulation(v[:, 0], v[:, 1], faces["shared_body_cut"][side])
            full = axes[0].tripcolor(tri, v[:, 2], shading="gouraud", cmap="viridis")
        fig.colorbar(full, ax=axes[0], label="canonical ENU Up / m")
        axes[0].set_title("Original Ill body, shared canonical cut")
        axes[0].axvline(original["cut"]["plane_x_m"], color="red", linewidth=0.8)
        visible = []
        for method in methods:
            for v, f, scalar in zip(
                vertices[method], faces[method], values[method], strict=True
            ):
                bounds = v[f, :2]
                mask = np.all(bounds.max(1) >= centre[:2] - 30, axis=1) & np.all(
                    bounds.min(1) <= centre[:2] + 30, axis=1
                )
                visible.extend(scalar[mask])
        low, high = min(visible), max(visible)
        for ax, method in zip(axes[1:], methods, strict=True):
            for v, f, scalar in zip(
                vertices[method], faces[method], values[method], strict=True
            ):
                tri = mtri.Triangulation(v[:, 0], v[:, 1], f)
                field = ax.tripcolor(
                    tri,
                    facecolors=scalar,
                    shading="flat",
                    cmap="coolwarm",
                    vmin=low,
                    vmax=high,
                )
                ax.triplot(tri, color="black", linewidth=0.3, alpha=0.5)
            for segment in seam:
                ax.plot(segment[:, 0], segment[:, 1], color="lime", linewidth=1)
            ax.set_xlim(centre[0] - 30, centre[0] + 30)
            ax.set_ylim(centre[1] - 30, centre[1] + 30)
            ax.set_title(method.replace("_", " ") + " — same physical crop")
        fig.colorbar(
            field, ax=axes[1:], label="facet ENU Up per canonical East / m per m"
        )
        for ax in axes:
            ax.set_aspect("equal")
            ax.set_xlabel("body East / m")
            ax.set_ylabel("body North / m")
        fig.savefig(output / "original-fields.png", dpi=110)
        plt.close(fig)
    fig, axes = plt.subplots(1, 2, figsize=(12, 5), layout="constrained")
    for ax, case in zip(axes, (report["cases"][0], original), strict=True):
        for method, result in case["methods"].items():
            angle = result["crease"]["per_segment_degrees"]
            ax.semilogy(
                np.arange(len(angle)), angle, "o-", label=method.replace("_", " ")
            )
        ax.set_title(case["name"] + ": new cut inside original facets")
        ax.set_xlabel("canonical subedge index")
        ax.set_ylabel("dihedral angle / degrees")
        ax.grid(True, alpha=0.3)
        ax.legend()
    fig.savefig(output / "boundary-angles.png", dpi=110)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    plot(parser.parse_args().output)

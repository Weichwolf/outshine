"""Exact compact/physical window sections from the retained actual provider source."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from evidence import revision
from opening_details import slots
from stepped_openings import apply
from stepped_profile_probe import sources
from stepped_source import exterior, plan_and_near


def local_mesh(mesh, opening):
    centre = np.r_[opening["centre"], opening["bottom_z_m"]]
    tangent = np.asarray(opening["tangent"])
    basis = np.array([[*tangent, 0], [tangent[1], -tangent[0], 0], [0, 0, 1]])
    vertices = (mesh["vertices"] - centre) @ basis.T
    triangles = vertices[mesh["faces"]]
    low, high = triangles.min(axis=1), triangles.max(axis=1)
    width, height = opening["width_m"], opening["height_m"]
    keep = (
        (low[:, 0] <= width / 2 + 0.3)
        & (high[:, 0] >= -width / 2 - 0.3)
        & (low[:, 1] <= 0.3)
        & (high[:, 1] >= -0.5)
        & (low[:, 2] <= height + 0.3)
        & (high[:, 2] >= -0.2)
    )
    return {
        "vertices": vertices,
        "faces": mesh["faces"][keep],
        "materials": mesh["materials"][keep],
    }


def draw(ax, mesh, opening, horizontal, name):
    width, height = opening["width_m"], opening["height_m"]
    normal = [0, 0, 1] if horizontal else [1, 0, 0]
    offset = height / 2 if horizontal else 0
    dims = (0, 1) if horizontal else (1, 2)
    colours = {
        1: "#777c7e",
        3: "#468cba",
        10: "#282f35",
        11: "#b88a5a",
        32768: "#468cba",
    }
    for line, material in slice_triangles(mesh, normal, offset):
        ax.plot(
            line[:, dims[0]],
            line[:, dims[1]],
            color=colours.get(material, "#8eae98"),
            linewidth=1.6,
        )
    ax.set(
        aspect="equal",
        title=name + (" · Horizontal cut" if horizontal else " · Vertical cut"),
        xlabel="U [m]" if horizontal else "Outward [m]",
        ylabel="Outward [m]" if horizontal else "Z above sill [m]",
        xlim=(-width / 2 - 0.3, width / 2 + 0.3) if horizontal else (-0.5, 0.3),
        ylim=(-0.5, 0.3) if horizontal else (-0.2, height + 0.3),
    )
    ax.grid(alpha=0.18)


def run(output):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Freeze the source before section comparisons")
    output.mkdir(parents=True, exist_ok=True)
    plan, payload = plan_and_near(sources()[0], "corner")
    compact = apply(plan, exterior(plan, 1)[0], 32768)[0]
    openings = slots(plan)
    proof = []
    for label, sill in (("Sill", True), ("TerraceNoSill", False)):
        opening = next(
            o for o in openings if o["kind"] == "window" and o["sill"] == sill
        )
        fig, axes = plt.subplots(2, 2, figsize=(20, 12))
        for row, (name, mesh) in enumerate(
            (("Compact", compact), ("Physical", payload[0]))
        ):
            local = local_mesh(mesh, opening)
            for column in range(2):
                draw(axes[row, column], local, opening, column == 0, name)
        fig.suptitle(opening["id"] + " · sill=" + str(sill))
        fig.tight_layout()
        path = output / ("window-" + label + "-sections.png")
        fig.savefig(path, dpi=120)
        plt.close(fig)
        proof.append({"opening_id": opening["id"], "sill": sill, "path": str(path)})
    record = provenance | {
        "scope": "Exact original near/compact triangles; local AABB filters only plot candidates",
        "cases": proof,
    }
    (output / "window-sections.json").write_text(json.dumps(record, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)

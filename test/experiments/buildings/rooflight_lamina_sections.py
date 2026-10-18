"""Actual plan/section/2.5D diagnostic of a source-owned full and sheet panel."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from matplotlib.collections import PolyCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.rooflight_panels import closed, place
from recipe_hall_operators import inputs_for
from recipe_rooflight_operators import prepare
from rooflight_lamina import template
from verify_industrial_halls import sources


def diagram(output):
    row, neighbors = sources()[2]
    fields = (
        prepare()
        .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
        .outputs["rooflights"]
        .value
    )
    item = fields[len(fields) // 2]
    a, low, b, high = item["bounds"]
    full = closed([item])[0]
    reduced = place(item, template(b - a, high - low, item["slope"]))
    centre = (
        np.array([(a + b) / 2, (low + high) / 2]) @ item["axes"] + item["origin_xy"]
    )
    meshes = []
    for mesh in (full, reduced):
        p = mesh["vertices"].copy()
        p[:, :2] = (p[:, :2] - centre) @ np.asarray(item["axes"]).T
        meshes.append(dict(mesh, vertices=p))
    fig = plt.figure(figsize=(24, 13.5))
    ax = fig.add_subplot(131)
    triangles = meshes[1]["vertices"][meshes[1]["faces"]]
    colours = ["#267d9b" if mat == 3 else "#c08442" for mat in reduced["materials"]]
    ax.add_collection(
        PolyCollection(triangles[:, :, :2], facecolors=colours, edgecolors="none")
    )
    ax.autoscale()
    ax.set_aspect("equal")
    ax.set(
        title="Plan · actual retained frame/glass caps",
        xlabel="Section run [m]",
        ylabel="Transverse [m]",
    )
    ax = fig.add_subplot(132)
    for mesh, colour, label in zip(
        meshes, ("#777777", "#267d9b"), ("full44", "sheets20"), strict=True
    ):
        first = True
        for line, _ in slice_triangles(mesh, np.array([0, 1, 0]), 0):
            ax.plot(
                line[:, 0],
                line[:, 2],
                color=colour,
                lw=2 if label == "full44" else 1,
                label=label if first else None,
            )
            first = False
    ax.set(
        title="Actual section · vertical sides omitted",
        xlabel="Section run [m]",
        ylabel="Height [m]",
    )
    ax.legend()
    ax = fig.add_subplot(133, projection="3d")
    ax.add_collection3d(
        Poly3DCollection(
            triangles, facecolors=colours, edgecolors="#333333", linewidths=0.25
        )
    )
    points = meshes[1]["vertices"]
    bounds = points.min(axis=0), points.max(axis=0)
    ax.set(
        xlim=(bounds[0][0], bounds[1][0]),
        ylim=(bounds[0][1], bounds[1][1]),
        zlim=(bounds[0][2] - 0.05, bounds[1][2] + 0.05),
        title="2.5D · open render sheets, not a collider",
    )
    ax.set_box_aspect(bounds[1] - bounds[0])
    ax.view_init(elev=30, azim=-65)
    fig.suptitle(
        item["id"]
        + "\nActual triangles; frame 6 cm/glass 3 mm two-sided surface-distance bounds, not a radiance bound"
    )
    fig.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=100)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    diagram(parser.parse_args().output)

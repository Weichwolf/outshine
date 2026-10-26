"""Actual local relief plan, triangle section and 2.5D geometry at three source facades."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from building_mesh import slice_triangles
from evidence import revision
from industrial_relief_mesh import direct
from industrial_relief_products import ArticulatedDetails
from matplotlib.collections import LineCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile


def diagram(part, output):
    p = part.plan
    mesh, _count = direct(p)
    figure = plt.figure(figsize=(19.2, 10.8))
    ax = figure.add_subplot(211)
    for polygon in shapely.get_parts(p.region):
        ax.fill(*np.asarray(polygon.exterior.coords).T, color="#8c9aa4")
        for ring in polygon.interiors:
            ax.fill(*np.asarray(ring.coords).T, color="white")
    for polygon in shapely.get_parts(p.keepouts):
        ax.plot(*np.asarray(polygon.exterior.coords).T, color="#bf6139", lw=0.7)
    ax.set(
        aspect="equal",
        xlabel="Facade U [m]",
        ylabel="Height [m]",
        title="2D · generated relief and protected existing opening/casing/access regions",
    )
    ax = figure.add_subplot(223)
    u = p.region.bounds[0] + 0.10
    lines = [
        points[:, [2, 1]] for points, _material in slice_triangles(mesh, [1, 0, 0], u)
    ]
    ax.add_collection(LineCollection(lines, colors="#537b91", linewidths=1.2))
    ax.set(
        xlim=(-0.05, p.depth_m + 0.05),
        ylim=(-0.05, p.region.bounds[3] + 0.05),
        xlabel="Outward [m]",
        ylabel="Height [m]",
        aspect="equal",
        title=f"Actual physical section · U={u:.2f} m",
    )
    spatial = figure.add_subplot(224, projection="3d")
    points = mesh["vertices"][mesh["faces"]][:, :, [0, 2, 1]]
    spatial.add_collection3d(
        Poly3DCollection(
            points, facecolors="#8c9aa4", edgecolors="#526471", linewidths=0.1
        )
    )
    spatial.set(
        xlim=(0, p.region.bounds[2]),
        ylim=(0, p.depth_m),
        zlim=(0, p.region.bounds[3]),
        xlabel="U [m]",
        ylabel="Outward [m]",
        zlabel="Height [m]",
        title="2.5D · exact closed source-attached relief",
    )
    spatial.set_box_aspect((p.region.bounds[2], p.depth_m, p.region.bounds[3]))
    spatial.set_yticks([0, p.depth_m])
    spatial.view_init(elev=20, azim=-70)
    figure.suptitle(
        p.kind
        + " · "
        + p.id
        + "\nProcedural interpretation, not supplied OSM construction"
    )
    figure.tight_layout()
    figure.savefig(output / (p.kind + "-sections.png"), dpi=100)
    plt.close(figure)


def diagrams(output):
    if revision()["experiment_tree_dirty"]:
        raise ValueError("Freeze industrial relief before diagnostics")
    rows, _receipt, _context, view = source_tile()
    owner = ArticulatedDetails(prepare_halls(rows), view)
    output.mkdir(parents=True, exist_ok=True)
    seen = set()
    for parts in owner.reliefs.values():
        for part in parts:
            if part.plan.kind not in seen:
                diagram(part, output)
                seen.add(part.plan.kind)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    diagrams(args.output)

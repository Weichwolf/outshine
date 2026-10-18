"""Render a labelled diagnostic, never an engine screenshot or a reference oracle."""

import argparse
from pathlib import Path

import numpy as np
import shapely
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

from river_surface import source_polygons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--surface", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--vertical-exaggeration", type=float, default=60)
    args = parser.parse_args()
    if not np.isfinite(args.vertical_exaggeration) or args.vertical_exaggeration <= 0:
        parser.error("positive finite vertical exaggeration required")
    output = args.output.resolve()
    if output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private results belong outside the checkout")
    with np.load(args.input, allow_pickle=False) as data, np.load(args.surface, allow_pickle=False) as fit:
        old = []
        for polygon, (_, _, level) in zip(source_polygons(data), data["surfaces"]):
            pieces = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
            xy = shapely.get_coordinates(pieces).reshape(-1, 4, 2)[:, :3]
            old.append(np.concatenate([xy, np.full((*xy.shape[:2], 1), level)], axis=2))
        vertices, faces = fit["vertices"], fit["triangles"]
        prior = vertices.copy()
        prior[:, 2] = fit["prior"]
        panels = [("Existing flat fragments", np.concatenate(old)),
                  ("Continuous prior; uphill counterexample", prior[faces]),
                  ("Directed shared surface; hypothesis", vertices[faces])]
    all_points = np.concatenate([points.reshape(-1, 3) for _, points in panels])
    low, high = all_points.min(axis=0), all_points.max(axis=0)
    span = np.maximum(high - low, 1)
    norm = Normalize(low[2], high[2])
    fig, axes = plt.subplots(1, 3, figsize=(18, 7), layout="constrained",
                             subplot_kw={"projection": "3d"})
    for ax, (name, triangles) in zip(axes, panels):
        colours = plt.cm.viridis(norm(triangles[:, :, 2].mean(axis=1)))
        ax.add_collection3d(Poly3DCollection(triangles, facecolors=colours, edgecolors="none"))
        ax.set(xlim=(low[0], high[0]), ylim=(low[1], high[1]),
               zlim=(low[2] - 1, high[2] + 1), title=name,
               xlabel="East [m]", ylabel="North [m]", zlabel="ASL [m]")
        ax.set_box_aspect((span[0], span[1], span[2] * args.vertical_exaggeration))
        ax.set_proj_type("ortho")
        ax.view_init(elev=27, azim=-60)
    fig.suptitle(f"Python water-model diagnostic — shared axes/colour scale, Z×{args.vertical_exaggeration:g}")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=150)
    plt.close(fig)
    print(output)


if __name__ == "__main__":
    main()

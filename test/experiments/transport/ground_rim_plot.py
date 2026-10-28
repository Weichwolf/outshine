"""Actual same-domain terrain before/after plus unfitted DEM perimeter curves."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import MaxNLocator
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(context, results, at, observed, arrays, output):
    figure = plt.figure(figsize=(18, 11), layout="constrained")
    grid = figure.add_gridspec(2, 3, height_ratios=(1.5, 1))
    road = context.road
    datum = float(road.vertices[:, 2].min())
    combined = np.vstack([v[0]["mesh"].vertices for v in results.values()])
    low, high = combined.min(axis=0), combined.max(axis=0)
    labels = {
        "coarse": "Before: four DEM chords",
        "uniform_reduced": f"After:{len(results['uniform_reduced'][0]['outer_vertex_ids'])} fitted rim points",
        "adaptive_reduced": f"Alternative:{len(results['adaptive_reduced'][0]['outer_vertex_ids'])} adaptive rim points",
    }
    for column, name in enumerate(labels):
        mesh = results[name][0]["mesh"]
        axis = figure.add_subplot(grid[0, column], projection="3d")
        for vertices, faces, color in (
            (mesh.vertices, mesh.triangles[len(road.triangles) :], ".72"),
            (road.vertices, road.triangles, ".25"),
        ):
            triangles = vertices[faces].copy()
            triangles[:, :, 2] -= datum
            axis.add_collection3d(
                Poly3DCollection(
                    triangles, facecolor=color, edgecolor=".45", linewidth=0.15
                )
            )
        ids = results[name][0]["outer_vertex_ids"]
        boundary = mesh.vertices[ids].copy()
        boundary[:, 2] -= datum
        axis.scatter(*boundary.T, color="steelblue", s=7)
        for terminal in context.report["terminal_values"]:
            cap = road.vertices[
                list(terminal["source_terminal"]["cap_vertices"])
            ].copy()
            cap[:, 2] -= datum
            axis.plot(*cap.T, color="crimson", linewidth=2)
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2] - datum, high[2] - datum),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus{datum:.3f}m",
            title=labels[name],
        )
        axis.set_box_aspect(high - low)
        axis.zaxis.set_major_locator(MaxNLocator(4))
        axis.view_init(elev=24, azim=-65)
    profile = figure.add_subplot(grid[1, :2])
    errors = figure.add_subplot(grid[1, 2])
    profile.plot(
        at,
        observed - datum,
        color=".35",
        linewidth=2,
        label="bound DEM,512 unfitted probes",
    )
    for name, color in (
        ("coarse", "tab:orange"),
        ("uniform_reduced", "tab:blue"),
        ("adaptive_reduced", "tab:green"),
    ):
        predicted = arrays[name + "_validation_z"]
        profile.plot(at, predicted - datum, color=color, label=labels[name])
        errors.plot(at, predicted - observed, color=color, label=name)
    profile.set(
        xlabel="closed perimeter arc m",
        ylabel=f"height minus{datum:.3f}m",
        title="Same physical perimeter and four exact DEM corners; intermediate observations explicitly fitted",
    )
    profile.legend(fontsize=9)
    errors.axhline(0, color=".4", linewidth=0.7)
    errors.set(
        xlabel="closed perimeter arc m",
        ylabel="height error against bound DEM m",
        title="Unfitted sampled error; no global bound",
    )
    errors.legend(fontsize=8)
    figure.suptitle(
        "Actual three-arm road: same123XYZ/154faces/three red end profiles/90whole contacts and15m domain\nShared metric3D axes, no vertical exaggeration; local C0 reference, no native DEM-grid/neighbour/World/PBR/device acceptance"
    )
    figure.savefig(output / "terrain-rim-profiles.png", dpi=150)
    plt.close(figure)

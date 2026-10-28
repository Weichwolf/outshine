"""Actual wider/narrower contact meshes with shared metric axes, plus footprint."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(context, baseline, tighter, output):
    road = context.road
    datum = float(road.vertices[:, 2].min())
    low, high = (
        baseline["mesh"].vertices.min(axis=0),
        baseline["mesh"].vertices.max(axis=0),
    )
    figure = plt.figure(figsize=(18, 7), layout="constrained")
    for index, value, title in (
        (1, baseline, "15m family choice"),
        (2, tighter, "10.5m family choice"),
    ):
        axis = figure.add_subplot(1, 3, index, projection="3d")
        mesh = value["mesh"]
        for vertices, faces, color in (
            (mesh.vertices, mesh.triangles[len(road.triangles) :], "0.72"),
            (road.vertices, road.triangles, "0.25"),
        ):
            triangles = vertices[faces].copy()
            triangles[:, :, 2] -= datum
            axis.add_collection3d(
                Poly3DCollection(
                    triangles, facecolor=color, edgecolor="0.4", linewidth=0.15
                )
            )
        for terminal in context.report["terminal_values"]:
            cap = road.vertices[
                list(terminal["source_terminal"]["cap_vertices"])
            ].copy()
            cap[:, 2] -= datum
            axis.plot(*cap.T, color="crimson", linewidth=2)
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2] - datum - 0.5, high[2] - datum + 0.5),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus {datum:.3f}m",
            title=f"{title}: {value['domain'].area:.2f} m²",
        )
        axis.set_box_aspect((high[0] - low[0], high[1] - low[1], high[2] - low[2] + 1))
        axis.view_init(elev=24, azim=-65)
    axis = figure.add_subplot(1, 3, 3)
    for value, color, title in (
        (baseline, "0.55", "15m"),
        (tighter, "tab:blue", "10.5m"),
    ):
        xy = value["domain"].exterior.xy
        axis.fill(*xy, color=color, alpha=0.2)
        axis.plot(*xy, color=color, label=title)
    axis.triplot(
        road.vertices[:, 0],
        road.vertices[:, 1],
        road.triangles,
        color="0.2",
        linewidth=0.35,
    )
    axis.set(
        aspect="equal",
        xlabel="east m",
        ylabel="north m",
        title="Same road, 28.87% smaller local domain",
    )
    axis.legend()
    figure.suptitle(
        "Actual qualified three-arm road: same123XYZ/154faces/three red end profiles/90 whole contacts\nShared metric axes, no vertical exaggeration; local C0 four-DEM-chord diagnostic, no native World/PBR/device acceptance"
    )
    figure.savefig(output / "whole-contact-profiles.png", dpi=150)
    plt.close(figure)

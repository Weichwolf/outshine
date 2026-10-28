"""Metric actual surface/body pairs and thickness sections; local diagnostics."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import MaxNLocator
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(products, output):
    figure = plt.figure(figsize=(17, 10), layout="constrained")
    for row, (product, report) in enumerate(products):
        surface, body = product["surface"], product["body"]
        origin = surface.vertices[:2].mean(axis=0)
        low, high = (
            body.vertices.min(axis=0) - origin,
            body.vertices.max(axis=0) - origin,
        )
        for column, mesh, title in (
            (0, surface, "Before: supplied walking surface"),
            (1, body, "After: closed deck, original surface retained"),
        ):
            axis = figure.add_subplot(2, 3, row * 3 + column + 1, projection="3d")
            triangles = mesh.vertices[mesh.triangles] - origin
            axis.add_collection3d(
                Poly3DCollection(
                    triangles, facecolor=".55", edgecolor=".25", linewidth=0.35
                )
            )
            cap = surface.vertices[:2] - origin
            axis.plot(*cap.T, color="crimson", linewidth=3)
            axis.set(
                xlim=(low[0] - 0.3, high[0] + 0.3),
                ylim=(low[1] - 0.3, high[1] + 0.3),
                zlim=(low[2] - 0.15, high[2] + 0.15),
                xlabel="east from cap m",
                ylabel="north from cap m",
                zlabel="height from cap m",
                title=f"{report['group'].rsplit(':', 1)[-1]} / {title}",
            )
            axis.set_box_aspect((*(high[:2] - low[:2] + 0.6), high[2] - low[2] + 0.3))
            axis.zaxis.set_major_locator(MaxNLocator(3))
            axis.view_init(elev=18, azim=-65)
        axis = figure.add_subplot(2, 3, row * 3 + 3)
        top = surface.vertices.reshape(-1, 2, 3).mean(axis=1)
        bottom = body.vertices[len(surface.vertices) :].reshape(-1, 2, 3).mean(axis=1)
        station = np.r_[
            0.0, np.cumsum(np.linalg.norm(np.diff(top[:, :2], axis=0), axis=1))
        ]
        axis.fill_between(
            station, top[:, 2] - origin[2], bottom[:, 2] - origin[2], color=".55"
        )
        (top_line,) = axis.plot(
            station, top[:, 2] - origin[2], color=".2", label="unchanged top"
        )
        (bottom_line,) = axis.plot(
            station,
            bottom[:, 2] - origin[2],
            color=".2",
            linestyle="--",
            label="actual lower cap",
        )
        axis.set(
            xlabel="centreline horizontal station m",
            ylabel="height from cap m",
            title="Vertical recipe depth: 0.25m",
            aspect="equal",
        )
        axis.yaxis.set_major_locator(MaxNLocator(3))
    figure.legend(
        [top_line, bottom_line],
        ["unchanged top", "actual lower cap"],
        loc="lower center",
        ncol=2,
    )
    figure.suptitle(
        "Actual source-bound local bridge ends489/940: whole ground caps marked red\nMetric source-ENU geometry; no supports/statics, terrain/crossing, whole-bridge, World/PBR/device acceptance"
    )
    figure.savefig(output / "bridge-end-bodies.png", dpi=150)
    plt.close(figure)

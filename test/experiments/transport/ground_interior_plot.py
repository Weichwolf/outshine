"""Local before/after geometry and unfitted terrain residuals; no PBR claim."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import Normalize
from matplotlib.ticker import MaxNLocator
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(context, base, results, inputs, output):
    figure = plt.figure(figsize=(16, 10), layout="constrained")
    road = context.road
    count = len(road.triangles)
    selected = (
        ("base", base),
        ("batch2", results["batch2"]),
        ("coupled", results["coupled"]),
    )
    vertices = np.vstack([value["mesh"].vertices for _, value in selected])
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    datum = float(road.vertices[:, 2].min())
    colors = Normalize(-4.1, 4.1)
    for column, (name, result) in enumerate(selected):
        mesh = result["mesh"]
        axis = figure.add_subplot(2, 3, column + 1, projection="3d")
        for faces, color in (
            (mesh.triangles[count:], "#baae89"),
            (mesh.triangles[:count], ".22"),
        ):
            triangles = mesh.vertices[faces].copy()
            triangles[:, :, 2] -= datum
            axis.add_collection3d(
                Poly3DCollection(
                    triangles, facecolor=color, edgecolor=".45", linewidth=0.25
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
            zlim=(low[2] - datum, high[2] - datum),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus {datum:.3f}m",
            title=name,
        )
        axis.set_box_aspect(high - low)
        axis.zaxis.set_major_locator(MaxNLocator(4))
        axis.view_init(elev=24, azim=-65)
        error = (
            mesh.vertices[inputs["faces"], 2].mean(axis=1) - inputs["centroid_dem_z"]
        )
        panel = figure.add_subplot(2, 3, column + 4)
        panel.tripcolor(
            mesh.vertices[:, 0],
            mesh.vertices[:, 1],
            inputs["faces"],
            facecolors=error,
            cmap="coolwarm",
            norm=colors,
        )
        panel.triplot(
            mesh.vertices[:, 0],
            mesh.vertices[:, 1],
            mesh.triangles,
            color=".3",
            linewidth=0.2,
        )
        panel.set(
            aspect="equal",
            xlabel="east m",
            ylabel="north m",
            title=f"410 unfitted centroid errors: {name}",
        )
    figure.colorbar(
        plt.cm.ScalarMappable(norm=colors, cmap="coolwarm"),
        ax=figure.axes[3:],
        label="terrain minus bound DEM m",
        shrink=0.75,
    )
    figure.suptitle(
        "Same 315 vertices / 564 faces, exact road/rim/coarse XYZ and whole soil grade 2/3\nShared metric axes, no vertical exaggeration; local C0 diagnostic, no native/PBR/world/device acceptance"
    )
    figure.savefig(output / "terrain-interior.png", dpi=150)
    plt.close(figure)

"""Actual unchanged road versus original/adapted local terrain triangles."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(context, result, output, *, title=None):
    road = context.road
    coarse, final = result["coarse_mesh"], result["mesh"]
    prior = coarse.vertices.copy()
    prior[:, 2] = result["prior_z"]
    datum = float(road.vertices[:, 2].min())
    figure = plt.figure(figsize=(16, 7), layout="constrained")
    for panel, vertices, faces, panel_title in (
        (
            1,
            prior,
            coarse.triangles,
            "Before: unchanged road + independent raw DEM height mesh",
        ),
        (
            2,
            final.vertices,
            final.triangles[len(road.triangles) :],
            "After: same road + conforming slope-bounded terrain",
        ),
    ):
        axis = figure.add_subplot(1, 2, panel, projection="3d")
        terrain = vertices[faces].copy()
        terrain[:, :, 2] -= datum
        axis.add_collection3d(
            Poly3DCollection(terrain, facecolor="0.70", edgecolor="0.52", linewidth=0.2)
        )
        triangles = road.vertices[road.triangles].copy()
        triangles[:, :, 2] -= datum
        axis.add_collection3d(
            Poly3DCollection(
                triangles, facecolor="0.25", edgecolor="0.18", linewidth=0.15
            )
        )
        for value in context.report["terminal_values"]:
            # Actual chosen source endpoint cap coordinates, not requested poses.
            indices = value["source_terminal"]["cap_vertices"]
            cap = road.vertices[list(indices)].copy()
            cap[:, 2] -= datum
            axis.plot(*cap.T, color="crimson", linewidth=2)
        low, high = (
            coarse.vertices[:, :2].min(axis=0),
            coarse.vertices[:, :2].max(axis=0),
        )
        zlow = min(prior[:, 2].min(), final.vertices[:, 2].min()) - datum - 0.5
        zhigh = max(prior[:, 2].max(), final.vertices[:, 2].max()) - datum + 0.5
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(zlow, zhigh),
            xlabel="east m",
            ylabel="north m",
            zlabel=f"height minus{datum:.3f}m",
            title=panel_title,
        )
        axis.set_box_aspect((*(high - low), zhigh - zlow))
        axis.view_init(elev=24, azim=-65)
    figure.suptitle(
        title
        or "Actual source-qualified three-arm road / all123XYZ and three terminal profiles unchanged\nMetric aspect, no vertical exaggeration; local C0 diagnostic, no native World/PBR/device acceptance"
    )
    figure.savefig(output / "terrain-contact.png", dpi=150)
    plt.close(figure)

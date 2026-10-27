"""Actual stair triangles and the emitted floor sections around their openings."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from matplotlib.collections import PolyCollection
from matplotlib.patches import Rectangle
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(binding, before, after, output):
    points = np.asarray(binding["request"].axis.coords)
    if binding["ends"][0]["z_m"] > binding["ends"][1]["z_m"]:
        points = points[::-1]
    origin = np.r_[points[0], min(e["z_m"] for e in binding["ends"])]
    tangent = (points[1] - points[0]) / np.linalg.norm(points[1] - points[0])
    normal = np.array([-tangent[1], tangent[0]])
    rotation = np.array([[*tangent, 0], [*normal, 0], [0, 0, 1]])
    mesh = after["mesh"]
    local = (mesh["vertices"] - origin) @ rotation.T
    triangles = local[mesh["faces"]]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    side = triangles[cross[:, 1] > 0][:, :, (0, 2)]
    length = float(np.linalg.norm(points[1] - points[0]))
    height = max(e["z_m"] for e in binding["ends"]) - origin[2]
    line = shapely.LineString([points[0] - 3 * tangent, points[1] + 3 * tangent])
    figure, axes = plt.subplots(2, 1, figsize=(13, 9), sharex=True, sharey=True)
    for axis, floors, title in zip(
        axes,
        (before["floors"], after["floors"]),
        (
            "Constructed stair candidate + uncut plate: obstructed",
            "Same emitted stair + explicit plate opening: qualified local connection",
        ),
        strict=True,
    ):
        for floor in floors:
            if floor.identifier not in {e["floor_id"] for e in binding["ends"]}:
                continue
            for piece in shapely.get_parts(floor.polygon.intersection(line)):
                coordinates = shapely.get_coordinates(piece)
                if len(coordinates) < 2:
                    continue
                extent = (coordinates - points[0]) @ tangent
                axis.add_patch(
                    Rectangle(
                        (extent.min(), floor.lower_z_m - origin[2]),
                        extent.max() - extent.min(),
                        floor.upper_z_m - floor.lower_z_m,
                        color="0.65",
                    )
                )
        axis.add_collection(
            PolyCollection(side, facecolor="#b8c8cd", edgecolor="0.25", linewidth=0.35)
        )
        for surface in after["surfaces"][::3]:
            centre = (surface["start_m"] + surface["end_m"]) / 2
            z = surface["z_m"] - origin[2]
            axis.plot(
                [centre, centre],
                [z, z + 2.1],
                color="#46825f",
                linewidth=0.7,
                linestyle="--",
            )
        axis.set(
            xlim=(-3, length + 3),
            ylim=(-0.5, height + 2.8),
            xlabel="longitudinal m",
            ylabel="height above lower floor m",
            title=title,
        )
        axis.set_aspect("equal")
        axis.grid(alpha=0.2)
    figure.suptitle(
        "Actual side triangles and actual plate intersections; dashed2.1m headroom\nChosen metric end profiles, positive discrete risers and intermediate landings. Diagnostic, no native World/rooms/statics certificate."
    )
    figure.tight_layout(rect=(0, 0, 1, 0.94))
    figure.savefig(output / "stair-section.png", dpi=150)
    plt.close(figure)
    figure = plt.figure(figsize=(13, 7))
    axis = figure.add_subplot(projection="3d")
    axis.add_collection3d(
        Poly3DCollection(
            triangles, facecolor="#b8c8cd", edgecolor="0.35", linewidth=0.25
        )
    )
    chosen = {f.identifier: f for f in after["floors"]}
    for opening in after["openings"]:
        polygon = shapely.from_wkb(bytes.fromhex(opening["wkb_hex"]))
        for part in shapely.get_parts(polygon):
            for ring in (part.exterior, *part.interiors):
                xy = np.asarray(ring.coords)
                for z in (
                    chosen[opening["floor_id"]].lower_z_m,
                    chosen[opening["floor_id"]].upper_z_m,
                ):
                    vertices = (np.c_[xy, np.full(len(xy), z)] - origin) @ rotation.T
                    axis.plot(*vertices.T, color="0.4", linewidth=1)
    axis.set(
        xlim=(-1, length + 1),
        ylim=(-2, 2),
        zlim=(-0.5, height + 2),
        xlabel="longitudinal m",
        ylabel="lateral m",
        zlabel="height above lower floor m",
        title="Actual closed stair waist, treads, landings and plate-opening boundary",
    )
    axis.set_box_aspect((length + 2, 4, height + 2.5))
    axis.view_init(elev=24, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "stair-body.png", dpi=150)
    plt.close(figure)

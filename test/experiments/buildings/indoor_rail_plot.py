"""Local diagnostic pictures of emitted rail meshes, never source-path cartoons."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from indoor_rail_instances import materialize
from indoor_rail_recipe import stair_pose
from matplotlib.collections import PolyCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def owner_meshes(result, owner):
    meshes = [r["mesh"] for r in result["handrails"] if r["source_id"] == owner]
    guards = [g for g in result["guards"] if owner in g["source_ids"]]
    meshes.extend(g["mesh"] for g in guards)
    guard_owners = {g["identifier"] for g in guards}
    for row in result["supports"]:
        if row["source_id"] != owner and row.get("guard_id") not in guard_owners:
            continue
        mesh = row.get("mesh")
        if mesh is None:
            mesh = materialize(result["templates"][row["kind"]], row["transform"])
        meshes.append(mesh)
    return meshes


def clip_triangles(triangles, bounds):
    result = []
    for triangle in triangles:
        points = list(triangle)
        for dimension, low, high in (
            (0, bounds[0], bounds[1]),
            (1, bounds[2], bounds[3]),
        ):
            for plane, sign in ((low, 1), (high, -1)):
                clipped = []
                for first, last in zip(points, points[1:] + points[:1], strict=True):
                    a = sign * (first[dimension] - plane) >= 0
                    b = sign * (last[dimension] - plane) >= 0
                    if a != b:
                        t = (plane - first[dimension]) / (
                            last[dimension] - first[dimension]
                        )
                        clipped.append(first + t * (last - first))
                    if b:
                        clipped.append(last)
                points = clipped
        if len(points) >= 3:
            result.append(np.asarray(points))
    return result


def draw(batch, result, output):
    figure, axes = plt.subplots(3, 2, figsize=(17, 11), squeeze=False)
    for axis, stair in zip(axes.flat, batch["stairs"], strict=False):
        origin, basis = stair_pose(stair)
        rotation = basis[:, (0, 2, 1)]
        meshes = [stair["mesh"], *owner_meshes(result, stair["source_id"])]
        for index, mesh in enumerate(meshes):
            triangles = ((mesh["vertices"] - origin) @ rotation)[mesh["faces"]]
            axis.add_collection(
                PolyCollection(
                    triangles[:, :, (0, 2)],
                    facecolor="0.72" if index == 0 else "0.22",
                    edgecolor="none",
                )
            )
        length = stair["surfaces"][-1]["end_m"]
        rise = stair["end_profiles"][-1]["z_m"] - origin[2]
        axis.set(
            xlim=(-0.5, length + 0.5),
            ylim=(-0.4, rise + 1.5),
            xlabel="longitudinal m",
            ylabel="height above lower floor m",
            title=stair["source_id"].split("transportation:")[-1],
        )
        axis.set_aspect("equal")
        axis.grid(alpha=0.15)
    figure.suptitle(
        "Actual six stair / hollow handrail / support / opening-guard meshes\nLocal diagnostic projection; no native lighting, World or structural qualification"
    )
    figure.tight_layout(rect=(0, 0, 1, 0.94))
    figure.savefig(output / "rail-sections.png", dpi=150)
    plt.close(figure)
    stair = next(s for s in batch["stairs"] if s["source_id"].endswith(":2"))
    origin, basis = stair_pose(stair)
    rotation = basis[:, (0, 2, 1)]
    figure = plt.figure(figsize=(15, 8))
    axis = figure.add_subplot(projection="3d")
    length = stair["surfaces"][-1]["end_m"]
    plate = batch["modified_floor_meshes"][stair["end_profiles"][-1]["floor_id"]]
    triangles = ((plate["vertices"] - origin) @ rotation)[plate["faces"]]
    polygons = clip_triangles(triangles, (-0.5, length + 0.5, -1.5, 1.5))
    axis.add_collection3d(
        Poly3DCollection(polygons, facecolor="0.65", edgecolor="none", alpha=0.25)
    )
    meshes = [stair["mesh"], *owner_meshes(result, stair["source_id"])]
    for index, mesh in enumerate(meshes):
        triangles = ((mesh["vertices"] - origin) @ rotation)[mesh["faces"]]
        axis.add_collection3d(
            Poly3DCollection(
                triangles, facecolor="0.72" if index == 0 else "0.25", edgecolor="none"
            )
        )
    length = stair["surfaces"][-1]["end_m"]
    rise = stair["end_profiles"][-1]["z_m"] - origin[2]
    axis.set(
        xlim=(-0.5, length + 0.5),
        ylim=(-2, 2),
        zlim=(-0.4, rise + 1.5),
        xlabel="longitudinal m",
        ylabel="lateral m",
        zlabel="height m",
        title="Actual hollow rails, posts, anchors and open upper approach\nActual host plate clipped/translucent for this local diagnostic",
    )
    axis.set_box_aspect((length + 1, 4, rise + 1.9))
    axis.view_init(elev=24, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "rail-body.png", dpi=150)
    plt.close(figure)

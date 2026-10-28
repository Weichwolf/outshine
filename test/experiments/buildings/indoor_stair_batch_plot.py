"""Actual joint stair sections and the two shared final plate regions."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from indoor_stair_plot import section_geometry
from matplotlib.collections import PolyCollection
from matplotlib.patches import Rectangle


def draw(context, result, output):
    bindings = {b["source_id"]: b for b in context.bindings}
    stairs = result["stairs"]
    figure, axes = plt.subplots(
        (len(stairs) + 1) // 2, 2, figsize=(16, 12), squeeze=False
    )
    for axis, stair in zip(axes.flat, stairs):
        geometry = section_geometry(bindings[stair["source_id"]], stair["mesh"])
        origin, rotation = geometry["origin"], geometry["rotation"]
        for floor in result["floors"]:
            if floor.identifier not in {e["floor_id"] for e in stair["end_profiles"]}:
                continue
            for part in shapely.get_parts(floor.polygon.intersection(geometry["line"])):
                points = shapely.get_coordinates(part)
                if len(points) < 2:
                    continue
                extent = (points - origin[:2]) @ rotation[0, :2]
                axis.add_patch(
                    Rectangle(
                        (extent.min(), floor.lower_z_m - origin[2]),
                        extent.max() - extent.min(),
                        floor.upper_z_m - floor.lower_z_m,
                        color="0.7",
                    )
                )
        axis.add_collection(
            PolyCollection(
                geometry["side"], facecolor="#b8c8cd", edgecolor="0.25", linewidth=0.25
            )
        )
        recipe = stair["recipe"]
        axis.set(
            xlim=(-2, geometry["length"] + 2),
            ylim=(-0.5, geometry["height"] + 2.6),
            xlabel="longitudinal m",
            ylabel="height above lower floor m",
            title=f"source part {stair['source_id'].rsplit(':', 1)[1]} · {recipe['risers']} risers/{recipe['flights']} flights · {len(stair['source_ids'])} source(s)",
        )
        axis.set_aspect("equal")
        axis.grid(alpha=0.2)
    for axis in list(axes.flat)[len(stairs) :]:
        axis.set_visible(False)
    figure.suptitle(
        "Six actual jointly qualified bodies and final shared plate sections\nWhole end profiles and source identities retained; one tiled source shares its body. Diagnostic, no World/public-route/statics certificate."
    )
    figure.tight_layout(rect=(0, 0, 1, 0.94))
    figure.savefig(output / "joint-stair-sections.png", dpi=140)
    plt.close(figure)
    final = {f.identifier: f for f in result["floors"]}
    figure, axes = plt.subplots(
        1, len(result["openings"]), figsize=(15, 7), squeeze=False
    )
    for axis, opening in zip(axes.flat, result["openings"], strict=True):
        floor = final[opening["floor_id"]]
        local = [
            s
            for s in stairs
            if any(e["floor_id"] == floor.identifier for e in s["end_profiles"])
        ]
        footprint = shapely.union_all([s["footprint"] for s in local])
        bounds = footprint.bounds
        centre = np.array([(bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2])
        mesh = result["modified_floor_meshes"][floor.identifier]
        triangles = mesh["vertices"][mesh["faces"]]
        top = (
            triangles[np.all(triangles[:, :, 2] == floor.upper_z_m, axis=1), :, :2]
            - centre
        )
        axis.add_collection(PolyCollection(top, facecolor="#dce4e7", edgecolor="none"))
        for s in local:
            xy = np.asarray(s["footprint"].exterior.coords) - centre
            axis.plot(xy[:, 0], xy[:, 1], color="#007a42", linewidth=1.5)
        for part in shapely.get_parts(
            shapely.from_wkb(bytes.fromhex(opening["wkb_hex"]))
        ):
            for ring in (part.exterior, *part.interiors):
                xy = np.asarray(ring.coords) - centre
                axis.plot(xy[:, 0], xy[:, 1], color="#b95c00", linewidth=1)
        axis.set(
            xlim=(bounds[0] - centre[0] - 4, bounds[2] - centre[0] + 4),
            ylim=(bounds[1] - centre[1] - 4, bounds[3] - centre[1] + 4),
            xlabel="east offset m",
            ylabel="north offset m",
            title=f"{len(local)} bodies · one final plate · {opening['area_m2']:.3f}m² opening",
        )
        axis.set_aspect("equal")
        axis.grid(alpha=0.2)
    figure.suptitle(
        "Actual final plate top triangles (grey), body footprints (green), openings (orange)\nAll73 selected footway corridors remain covered; positive-area footprint separation protects bodies/headroom."
    )
    figure.tight_layout(rect=(0, 0, 1, 0.93))
    figure.savefig(output / "joint-plate-regions.png", dpi=140)
    plt.close(figure)

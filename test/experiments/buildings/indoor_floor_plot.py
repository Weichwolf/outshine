"""Shared-scale diagnostic of actual emitted plates and original source axes."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from matplotlib.collections import PolyCollection
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(inputs, results, path):
    methods = ("uniform", "adapted")
    groups = results["adapted"]["programmes"]
    figure = plt.figure(figsize=(16, 12))
    original = {r.identifier: r for r in inputs.requests}
    for row, group in enumerate(groups):
        members = set(group["source_ids"])
        bodies = [p for p in inputs.parts if p.identifier in members]
        bounds = shapely.union_all([p.polygon for p in bodies]).bounds
        centre = np.array(
            [
                (bounds[0] + bounds[2]) / 2,
                (bounds[1] + bounds[3]) / 2,
                group["base_z_m"],
            ]
        )
        span = (
            max(
                bounds[2] - bounds[0],
                bounds[3] - bounds[1],
                max(p.upper_m for p in bodies),
            )
            * 1.1
        )
        height = max(p.upper_m for p in bodies) + 1
        for column, method in enumerate(methods):
            axis = figure.add_subplot(
                len(groups), 2, row * 2 + column + 1, projection="3d"
            )
            result = results[method]
            floors = [f for f in result["floors"] if set(f.source_ids) == members]
            selected = {f.identifier for f in floors}
            for part in result["adjusted_volumes"]:
                if part.identifier not in members:
                    continue
                for ring in (part.polygon.exterior, *part.polygon.interiors):
                    xy = np.asarray(ring.coords) - centre[:2]
                    for z in (part.lower_m, part.upper_m):
                        axis.plot(
                            xy[:, 0],
                            xy[:, 1],
                            np.full(len(xy), z),
                            color="0.5",
                            linewidth=0.55,
                        )
            for floor in floors:
                if floor.identifier not in result["meshes"]:
                    continue
                mesh = result["meshes"][floor.identifier]
                triangles = mesh["vertices"][mesh["faces"]] - centre
                axis.add_collection3d(
                    Poly3DCollection(
                        triangles, facecolor="#c5d6e1", edgecolor="none", alpha=0.45
                    )
                )
            ready, pending = 0, 0
            footprint = shapely.union_all([p.polygon for p in bodies])
            for choice in result["choices"]:
                request = original[choice["source_id"]]
                if not footprint.intersects(request.axis):
                    continue
                if choice["status"] == "ready" and choice["floor_id"] in selected:
                    points = choice["xyz"] - centre
                    axis.plot(
                        points[:, 0],
                        points[:, 1],
                        points[:, 2],
                        color="#007a42",
                        linewidth=2.0,
                    )
                    ready += 1
                elif choice["status"] == "pending":
                    pending += 1
            axis.set(
                xlim=(-span / 2, span / 2),
                ylim=(-span / 2, span / 2),
                zlim=(-1, height),
                xlabel="east m",
                ylabel="north m",
                zlabel="height above common base m",
                title=f"{method} · component {row + 1} · {ready} bound / {pending} pending",
            )
            axis.set_box_aspect((span, span, height + 1))
            axis.set_zticks((0, height - 1))
            axis.view_init(elev=32, azim=-58)
    figure.suptitle(
        "Actual closed floor plates + bound indoor axes; source envelopes in grey\nShared metric scale per pair; pending axes have no fabricated height. No room/door/public-route certificate."
    )
    figure.tight_layout(rect=(0, 0, 1, 0.95))
    figure.savefig(path, dpi=140)
    plt.close(figure)
    draw_adjustments(inputs, results, path.with_name("outline-contacts.png"))


def draw_adjustments(inputs, results, path):
    before, after = results["constrained"], results["adapted"]
    old = {r["source_id"]: r for r in before["choices"]}
    requests = {r.identifier: r for r in inputs.requests}
    changed = [
        r
        for r in after["choices"]
        if r["status"] == "ready" and old[r["source_id"]]["status"] != "ready"
    ]
    if not changed:
        return
    figure, axes = plt.subplots(
        len(changed), 2, figsize=(10, 3 * len(changed)), squeeze=False
    )
    for row, choice in enumerate(changed):
        request = requests[choice["source_id"]]
        selected = next(
            f for f in after["floors"] if f.identifier == choice["floor_id"]
        )
        previous = next(
            f
            for f in before["floors"]
            if f.source_ids == selected.source_ids and f.level == selected.level
        )
        centre = np.asarray(
            request.corridor.difference(previous.polygon).representative_point().coords
        )[0]
        for column, (result, floor, label) in enumerate(
            (
                (before, previous, "original envelope"),
                (after, selected, "adapted envelope"),
            )
        ):
            axis = axes[row, column]
            mesh = result["meshes"][floor.identifier]
            triangles = mesh["vertices"][mesh["faces"]]
            top = (
                triangles[np.all(triangles[:, :, 2] == floor.upper_z_m, axis=1), :, :2]
                - centre
            )
            axis.add_collection(
                PolyCollection(top, facecolor="#c5d6e1", edgecolor="none")
            )
            for ring in (request.corridor.exterior,):
                xy = np.asarray(ring.coords) - centre
                axis.plot(xy[:, 0], xy[:, 1], color="#b95c00", linewidth=1.5)
            axis.set(
                xlim=(-1.5, 1.5),
                ylim=(-1.5, 1.5),
                xlabel="east offset m",
                ylabel="north offset m",
                title=f"{label} · whole corridor covered: {floor.polygon.covers(request.corridor)}",
            )
            axis.set_aspect("equal")
            axis.grid(alpha=0.25)
    figure.suptitle(
        "Actual top triangles at the four previously uncovered corridor edges\nOrange: unchanged1.5m corridor; blue: emitted floor plate. Same3×3m window per pair."
    )
    figure.tight_layout(rect=(0, 0, 1, 0.94))
    figure.savefig(path, dpi=140)
    plt.close(figure)

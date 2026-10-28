"""Matched whole, outside-near and inside views from actual generated recesses."""

import json

import numpy as np
import shapely
from demand import View
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_facade_prepare import positions
from terraced_facade_probe import materials
from tile_terraced_crowns import crown_definitions


def views(before, after, *, inside=False):
    recess = after.body.plan.prepared.recesses[0]
    core = after.plan.body.core
    tangent = np.asarray(recess.outer.tangent)
    outward = tangent[[1, 0]] * [1, -1]
    facing = outward + 0.65 * tangent
    direction = np.r_[
        facing[0] * np.asarray(core.across_xy) + facing[1] * np.asarray(core.along_xy),
        0.7,
    ]
    points = np.r_[before.draw(True)["vertices"], after.draw(True)["vertices"]]
    whole = overview(points, direction)
    centre = np.r_[recess.outer.centre, recess.outer.bottom_m + 1.35]
    eye = centre + np.r_[7.0 * outward + 2.5 * tangent, 0.5]
    target = centre - np.r_[0.65 * outward, 0]
    close = View(
        *positions(np.array([eye, target]), core), width=1920, height=1080, fov_deg=48
    )
    result = {"whole": whole, "near": close}
    if inside:
        doorway = shapely.from_wkb(recess.door_route_wkb)
        start, target = doorway.coords[0], doorway.coords[-1]
        view = positions(
            np.array(
                [
                    [*start, recess.outer.bottom_m + 1.6],
                    [*target, recess.outer.bottom_m + 1.35],
                ]
            ),
            core,
        )
        result["inside"] = View(*view, width=1920, height=1080, fov_deg=65)
    return result


def export(groups, output, proof, *, inside=False):
    palette = materials() | {"60003": crown_definitions()["60003"]}
    queue = []
    for index, group in enumerate(groups):
        current_views = views(group["baseline"], group["recess"], inside=inside)
        for mode, owner in group.items():
            name = f"{index}-{mode}-whole"
            record = {
                "camera": camera(current_views["whole"]),
                "source_id": owner.plan.body.identifier,
                "source": {"material_definitions": palette},
                "facade_materials": {},
                "ground_span_m": 10000,
                "scope": "Actual full source body and optional contained recessed facade bays. Same camera/material/light before and after; open rear doors and physical guard. Flat diagnostic ground, no DEM/native/World/controller/device acceptance.",
            }
            write(output, name, owner.draw(True), record, proof)
            path = output / (name + "-geometry.json")
            base = json.loads(path.read_text())
            queue.append({"geometry": str(path), "output": str(output / "images")})
            for label, view in current_views.items():
                if label == "whole":
                    continue
                name = f"{index}-{mode}-{label}"
                path = output / (name + "-geometry.json")
                path.write_text(
                    json.dumps(base | {"city": name, "camera": camera(view)}, indent=2)
                    + "\n"
                )
                queue.append({"geometry": str(path), "output": str(output / "images")})
    (output / "render-queue.json").write_text(json.dumps(queue, indent=2) + "\n")
    return queue

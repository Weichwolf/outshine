"""Actual whole-source and elevated terrace views with shared before/after cameras."""

import numpy as np
from demand import View
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_facade_prepare import positions
from terraced_facade_probe import materials
from tile_terraced_crowns import crown_definitions


def export(owners, output, proof):
    palette = materials() | {"60003": crown_definitions()["60003"]}
    current = owners["shared"]
    core = current.plan.body.core
    points = np.r_[
        owners["grid"].draw(True)["vertices"], current.draw(True)["vertices"]
    ]
    direction = np.r_[
        np.asarray(core.across_xy) - 1.25 * np.asarray(core.along_xy), 0.85
    ]
    views = {"whole": overview(points, direction)}
    floor = current.plan.body.storeys[6].bottom_m
    views["terraces"] = View(
        *positions(np.array([(-18, -35, floor + 12), (5, 12, floor - 2)]), core),
        width=1920,
        height=1080,
        fov_deg=48,
    )
    queue = []
    for mode in ("grid", "shared"):
        owner = owners[mode]
        for label, view in views.items():
            name = mode + "-" + label
            write(
                output,
                name,
                owner.draw(True),
                {
                    "camera": camera(view),
                    "source_id": owner.plan.body.identifier,
                    "source": {"material_definitions": palette},
                    "facade_materials": {},
                    "ground_span_m": 10000,
                    "scope": "Same real complete source/height/floors/material/camera/light, optional contained terrace routes and actual new openings. No general roaming/controller/native/World/device acceptance.",
                },
                proof,
            )
            queue.append(
                {
                    "geometry": str(output / (name + "-geometry.json")),
                    "output": str(output / "images"),
                }
            )
    return queue

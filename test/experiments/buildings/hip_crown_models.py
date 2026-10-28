"""Matched real complete building and high roof views, with unchanged materials."""

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
    queue = []
    for index, group in enumerate(owners):
        original, current = group["flat"], group["auto"]
        core = current.plan.body.core
        points = np.r_[original.draw(True)["vertices"], current.draw(True)["vertices"]]
        direction = np.r_[
            np.asarray(core.across_xy) - 1.25 * np.asarray(core.along_xy), 0.85
        ]
        view = overview(points, direction)
        for mode, owner in (("flat", original), ("hip", current)):
            name = f"{index}-{mode}"
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
                    "scope": "Complete actual source body and optional source-height crown; same camera/material/light, flat plane without DEM. No controller/native/World/device acceptance.",
                },
                proof,
            )
            queue.append(
                {
                    "geometry": str(output / (name + "-geometry.json")),
                    "output": str(output / "images"),
                }
            )
        if index == 0:
            deck = current.plan.crown.deck_m
            view = View(
                *positions(np.array([(15, -25, deck + 35), (-8, 14, deck)]), core),
                width=1920,
                height=1080,
                fov_deg=48,
            )
            for mode, owner in (("flat", original), ("hip", current)):
                name = f"{index}-{mode}-roof"
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
                        "scope": "Matched complete geometry at a higher two-courtyard roof view; camera/lighting/materials unchanged.",
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

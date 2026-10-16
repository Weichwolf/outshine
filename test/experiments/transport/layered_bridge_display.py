"""Matched whole-scene regional 720p views of both old and newly planned networks."""

import json

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from transport_surface_experiment import camera_settings


def scenes(args, before, bodies):
    camera = camera_settings(
        json.loads((args.baseline / "after-scene.json").read_text())
    )
    camera.update(
        eye=[4090, -1800, 1110],
        target=[4090, -800, 160],
        span_m=2100,
        profile=[1280, 720],
        detail_view=True,
    )
    after = append(before, bodies, np.array(camera["colours"])) if bodies else before
    path = args.output / "after-body.npz"
    np.savez_compressed(path, **after)
    for name, source in (("before", args.baseline / "after-body.npz"), ("after", path)):
        row = dict(
            camera,
            id="Wien-layered-bridges-" + name,
            mesh=str(source),
            mesh_sha256=digest(source),
            output=str(args.output / (name + ".png")),
        )
        (args.output / (name + "-scene.json")).write_text(
            json.dumps(row, indent=2) + "\n"
        )
    return {
        "vertices": len(after["vertices"]),
        "faces": len(after["triangles"]),
        "after_body_sha256": digest(path),
        "complete_scene_array_bytes": sum(a.nbytes for a in after.values()),
        "original_scene_array_bytes": sum(a.nbytes for a in before.values()),
    }

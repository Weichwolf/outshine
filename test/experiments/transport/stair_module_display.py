"""Matched whole-region and moderate 720p views of actual saved module products."""

import json

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh


def display(output, base, supports, state, data, terrain, parts, roles):
    original = state["road_display"]
    scene = json.loads((supports / "after-overview-scene.json").read_text())
    palette = np.array(scene["colours"])
    actual = Mesh(terrain.vertices.astype("f4"), terrain.triangles)
    shown = append(original, [(actual, 1)], palette)
    p = actual.vertices[actual.triangles]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    slots = np.where(abs(normals[:, 2]) < np.linalg.norm(normals, axis=1) * 1e-6, 6, 1)
    # These are face indices in the slab's top-first export.
    soil = np.flatnonzero(~parts["ground"])
    slots[soil[roles]] = 6
    first = len(original["triangles"])
    shown["materials"][first:] = slots
    shown["face_base_colour_linear_rgba"][first:] = palette[slots]
    module = Mesh(data["body_vertices"].astype("f4"), data["body_faces"])
    bodies = [(module, 3)] + [
        (Mesh(p.vertices.astype("f4"), p.triangles), 6) for p in state["piers"]
    ]
    shown = append(shown, bodies, palette)
    np.savez_compressed(output / "after-display.npz", **shown)
    (output / "before-display.npz").symlink_to(state["before_display_path"].resolve())
    for view in ("overview", "network"):
        camera = dict(scene)
        if view == "network":
            camera.update(
                eye=[600, -490, 616],
                target=[0, 310, 166],
                span_m=1450,
                detail_view=True,
            )
        for phase in ("before", "after"):
            mesh = output / (phase + "-display.npz")
            row = dict(
                camera,
                id="Wien-ground-module-" + phase + "-" + view,
                profile=[1280, 720],
                mesh=str(mesh),
                mesh_sha256=digest(mesh),
                output=str(output / (phase + "-" + view + ".png")),
            )
            (output / (phase + "-" + view + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
    return {
        "before_display_sha256": digest(output / "before-display.npz"),
        "after_display_sha256": digest(output / "after-display.npz"),
        "owned_array_bytes": sum(v.nbytes for v in shown.values()),
        "old_road_display_arrays_byte_exact": all(
            np.array_equal(shown[k][: len(v)], v) for k, v in original.items()
        ),
    }

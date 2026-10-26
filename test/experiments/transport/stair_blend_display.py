"""Actual before/after bodies; the constructed terrain faces remain explicit."""

import json

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from stair_terrain_display import overview_span


def scenes(output, products, bodies):
    roads = dict(np.load(products / "after/display-body.npz"))
    wide = json.loads((products / "after/overview-scene.json").read_text())
    palette = np.array(wide["colours"])
    span = overview_span(wide, [body for _name, body, _roles in bodies])
    report = {"overview_span_m": span}
    for name, body, constructed in bodies:
        actual = Mesh(body.vertices.astype(np.float32).astype(float), body.triangles)
        arrays = append(roads, [(actual, 1)], palette)
        p = actual.vertices[actual.triangles]
        normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        slots = np.where(
            abs(normals[:, 2]) < np.linalg.norm(normals, axis=1) * 1e-6, 6, 1
        )
        if constructed is not None:
            slots[np.flatnonzero(constructed)] = 6
        first = len(roads["triangles"])
        arrays["materials"][first:] = slots
        arrays["face_base_colour_linear_rgba"][first:] = palette[slots]
        target = output / (name + "-display.npz")
        np.savez_compressed(target, **arrays)
        for view in ("overview", "diagnostic"):
            row = json.loads((products / "after" / (view + "-scene.json")).read_text())
            if view == "overview":
                row["span_m"] = span
            row.update(
                id="Wien-terrain-blend-" + name + "-" + view,
                mesh=str(target),
                mesh_sha256=digest(target),
                output=str(output / (name + "-" + view + ".png")),
            )
            (output / (name + "-" + view + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
        report[name + "_display_sha256"] = digest(target)
    return report

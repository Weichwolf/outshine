"""Matched actual-body 720p counterexample scenes; no substitute city geometry."""

import json

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append


def overview_span(camera, bodies):
    target = np.asarray(camera["target"])
    forward = target - camera["eye"]
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0.0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    ratio = camera["profile"][0] / camera["profile"][1]
    return float(
        max(
            camera["span_m"],
            *[
                2.2
                * max(
                    abs((body.vertices - target) @ right).max(),
                    ratio * abs((body.vertices - target) @ up).max(),
                )
                for body in bodies
            ],
        )
    )


def scenes(output, products, retaining, excavated, palette_path):
    with np.load(products / "after/display-body.npz") as data:
        roads = dict(data)
    palette = np.array(json.loads(palette_path.read_text())["colours"])
    wide = json.loads((products / "after/overview-scene.json").read_text())
    span = overview_span(wide, (retaining, excavated))
    result = {}
    for name, body in (("before", retaining), ("after", excavated)):
        actual = type(body)(
            body.vertices.astype(np.float32).astype(float), body.triangles
        )
        display = append(roads, [(actual, 1)], palette)
        p = actual.vertices[actual.triangles]
        normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        vertical = abs(normals[:, 2]) < np.linalg.norm(normals, axis=1) * 1e-6
        slots = np.where(vertical, 6, 1)
        start = len(roads["triangles"])
        display["materials"][start:] = slots
        display["face_base_colour_linear_rgba"][start:] = palette[slots]
        path = output / (name + "-display.npz")
        np.savez_compressed(path, **display)
        for view in ("overview", "diagnostic"):
            row = json.loads((products / "after" / (view + "-scene.json")).read_text())
            if view == "overview":
                row["span_m"] = span
            row.update(
                id="Wien-terrain-" + name + "-" + view,
                mesh=str(path),
                mesh_sha256=digest(path),
                output=str(output / (name + "-" + view + ".png")),
            )
            (output / (name + "-" + view + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
        result[name + "_display_sha256"] = digest(path)
    result["overview_span_m"] = span
    return result

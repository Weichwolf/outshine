"""Matched regional 720p scenes append actual verified bodies to the complete old scene."""

import json

import numpy as np
from camera_height_inputs import digest
from transport_surface_experiment import camera_settings


def append(before, bodies, palette):
    vertices, faces, normals, slots, colours = [], [], [], [], []
    offset = len(before["vertices"])
    for body, slot in bodies:
        p = body.vertices[body.triangles]
        n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        n /= np.linalg.norm(n, axis=1)[:, None]
        vertices.append(body.vertices)
        faces.append(body.triangles + offset)
        normals.append(np.repeat(n, 3, axis=0))
        slots.append(np.full(len(p), slot, int))
        colours.append(np.tile(palette[slot], (len(p), 1)))
        offset += len(body.vertices)
    result = dict(before)
    for name, rows in zip(
        (
            "vertices",
            "triangles",
            "normals",
            "materials",
            "face_base_colour_linear_rgba",
        ),
        (vertices, faces, normals, slots, colours),
        strict=True,
    ):
        result[name] = np.concatenate([before[name], *rows]).astype(
            before[name].dtype, copy=False
        )
    return result


def scenes(args, before, bodies, problem):
    camera = camera_settings(
        json.loads((args.baseline / "after-scene.json").read_text())
    )
    centre = np.asarray(problem["domain"].centroid.coords[0]) - [0, 200]
    target = np.r_[
        centre,
        problem["reference"][2]
        + (centre - problem["reference"][:2]) @ problem["gradient"],
    ]
    camera.update(
        eye=(target + [0, -1000, 950]).tolist(),
        target=target.tolist(),
        span_m=950,
        profile=[1280, 720],
        detail_view=True,
    )
    after = append(before, bodies, np.array(camera["colours"]))
    path = args.output / "after-body.npz"
    np.savez_compressed(path, **after)
    for name, source in (("before", args.baseline / "after-body.npz"), ("after", path)):
        row = dict(
            camera,
            id="Wien-cycle-bridge-" + name,
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
        "scope": "Actual full original array prefixes retained, with candidate ribbon and piers appended. Former selected ground geometry is retained as foundation; the three source profile bindings choose the new travel height. No native/API/consumer delivery.",
    }

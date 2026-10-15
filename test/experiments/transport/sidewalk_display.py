"""Append independently inspectable sidewalk geometry to the unchanged curb scene."""

import json

import numpy as np
from camera_height_inputs import digest
from transport_surface_experiment import PALETTE, camera_settings


def scenes(output, baseline, before, bodies):
    camera = camera_settings(json.loads((baseline / "after-scene.json").read_text()))
    concrete = next(
        r["albedo"]
        for r in json.loads(PALETTE.read_text())["classes"]
        if r["name"] == "concrete"
    )
    colour = np.asarray([*concrete, 1], np.float32)
    if len(camera["colours"]) != 11 or not np.array_equal(
        np.asarray(camera["colours"][10], np.float32), colour
    ):
        raise ValueError("existing curb concrete slot required")
    p = bodies["vertices"][bodies["triangles"]]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normals = (normals / np.linalg.norm(normals, axis=1)[:, None]).astype(np.float32)
    after = {
        "vertices": np.vstack([before["vertices"], bodies["vertices"]]),
        "triangles": np.vstack(
            [before["triangles"], bodies["triangles"] + len(before["vertices"])]
        ),
        "normals": np.vstack([before["normals"], np.repeat(normals, 3, axis=0)]),
        "materials": np.r_[
            before["materials"],
            np.full(len(bodies["triangles"]), 10, dtype=before["materials"].dtype),
        ],
        "face_base_colour_linear_rgba": np.vstack(
            [before["face_base_colour_linear_rgba"], np.tile(colour, (len(p), 1))]
        ),
    }
    np.savez_compressed(output / "after-body.npz", **after)
    records = []
    for name, path, arrays in [
        ("before", baseline / "after-body.npz", before),
        ("after", output / "after-body.npz", after),
    ]:
        row = dict(
            camera,
            id="Wien-sidewalks-" + name,
            mesh=str(path),
            mesh_sha256=digest(path),
            source_array_payload_bytes=sum(a.nbytes for a in arrays.values()),
            output=str(output / (name + ".png")),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        records.append(row)
    high = dict(
        records[1],
        profile=[1920, 1080],
        id="Wien-sidewalks-after-1080",
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(high, indent=2) + "\n")
    return records, {
        "material_json_sha256": digest(PALETTE),
        "procedural_material": "concrete",
        "linear_rgba": colour.tolist(),
        "new_indexed_xyz_copy_bytes": p.nbytes,
        "new_corner_normal_copy_bytes": normals.nbytes * 3,
    }

"""Matched regional actual before/after scenes with preserved old traffic arrays."""

import json

import numpy as np
from camera_height_inputs import digest
from transport_surface_experiment import PALETTE, camera_settings


def scenes(args, data, packed, rows):
    base = data["base"]
    camera = camera_settings(
        json.loads((args.sidewalks / "after-scene.json").read_text())
    )
    colour = np.asarray(
        [
            *next(
                r["albedo"]
                for r in json.loads(PALETTE.read_text())["classes"]
                if r["name"] == "concrete"
            ),
            1,
        ],
        np.float32,
    )
    if not np.array_equal(np.asarray(camera["colours"][10], np.float32), colour):
        raise ValueError("existing concrete inspection material required")
    centre = np.mean([r["origin"] for r in rows], axis=0)
    shift = np.r_[centre - np.asarray(camera["target"])[:2], 0.0]
    camera.update(
        eye=(np.asarray(camera["eye"]) + shift).tolist(),
        target=(np.asarray(camera["target"]) + shift).tolist(),
        span_m=1100,
    )
    p = packed["vertices"][packed["triangles"]]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normals = (normals / np.linalg.norm(normals, axis=1)[:, None]).astype(np.float32)
    after = {
        "vertices": np.vstack([base["vertices"], packed["vertices"]]),
        "triangles": np.vstack(
            [base["triangles"], packed["triangles"] + len(base["vertices"])]
        ),
        "normals": np.vstack([base["normals"], np.repeat(normals, 3, axis=0)]),
        "materials": np.r_[
            base["materials"], np.full(len(p), 10, dtype=base["materials"].dtype)
        ],
        "face_base_colour_linear_rgba": np.vstack(
            [base["face_base_colour_linear_rgba"], np.tile(colour, (len(p), 1))]
        ),
    }
    path = args.output / "after-body.npz"
    np.savez_compressed(path, **after)
    records = []
    for name, mesh, arrays in [
        ("before", args.sidewalks / "after-body.npz", data["before"]),
        ("after", path, after),
    ]:
        (args.output / name).mkdir()
        row = dict(
            camera,
            id="Wien-foot-ramps",
            mesh=str(mesh),
            mesh_sha256=digest(mesh),
            source_array_payload_bytes=sum(a.nbytes for a in arrays.values()),
            output=str(args.output / name / "shot.png"),
        )
        (args.output / (name + "-scene.json")).write_text(
            json.dumps(row, indent=2) + "\n"
        )
        records.append(row)
    (args.output / "after-1080").mkdir()
    high = dict(
        records[1],
        profile=[1920, 1080],
        output=str(args.output / "after-1080/shot.png"),
    )
    (args.output / "after-1080-scene.json").write_text(
        json.dumps(high, indent=2) + "\n"
    )
    return records, {
        "material_json_sha256": digest(PALETTE),
        "linear_rgba": colour.tolist(),
        "new_indexed_xyz_copy_bytes": p.nbytes,
        "new_corner_normal_copy_bytes": normals.nbytes * 3,
    }

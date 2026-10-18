"""Keep complete source bodies; encode explicit origins and two camera-frame pairs."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_regional_precision import position_payload
from stair_terrain_experiment import frozen


def source_bodies(row):
    bodies = row.get("bodies", [row])
    if not 1 <= len(bodies) <= 64 or len({b["id"] for b in bodies}) != len(bodies):
        raise ValueError("complete bounded distinct source bodies required")
    result = []
    for body in bodies:
        path = Path(body["mesh"])
        if digest(path) != body["mesh_sha256"]:
            raise ValueError("original source body changed")
        with np.load(path, allow_pickle=False) as saved:
            data = dict(saved)
        if not {"vertices", "triangles", "normals", "materials"} <= data.keys():
            raise ValueError("complete actual body/normal/material arrays required")
        result.append((body, data))
    return result


def encode_body(output, body, data, index):
    points, faces = data["vertices"], data["triangles"]
    origin = np.round((points.min(axis=0) + points.max(axis=0)) / 128) * 64
    positions = position_payload(points, origin)
    actual = positions.astype(float) + origin
    reference = np.cross(
        points[faces[:, 1]] - points[faces[:, 0]],
        points[faces[:, 2]] - points[faces[:, 0]],
    )

    def quality(candidate):
        cross = np.cross(
            candidate[faces[:, 1]] - candidate[faces[:, 0]],
            candidate[faces[:, 2]] - candidate[faces[:, 0]],
        )
        return {
            "maximum_position_error_m": float(
                np.linalg.norm(candidate - points, axis=1).max()
            ),
            "nonpositive_orientation_vs_source_faces": int(
                np.count_nonzero(np.einsum("ij,ij->i", reference, cross) <= 0)
            ),
        }

    path = output / f"body-{index}.npz"
    np.savez_compressed(path, **(data | {"vertices": positions}))
    return dict(
        body,
        mesh=str(path),
        mesh_sha256=digest(path),
        origin_m=origin.tolist(),
        encoded_float32=True,
    ), {
        "id": body["id"],
        "original_mesh": body["mesh"],
        "original_mesh_sha256": body["mesh_sha256"],
        "vertices": len(points),
        "faces": len(faces),
        "encoded_position_bytes": positions.nbytes,
        "origin_bytes": origin.nbytes,
        "absolute_quality": quality(points.astype("f4").astype(float)),
        "regional_quality": quality(actual),
    }


def camera_pairs(row, bodies, output):
    names = []
    # Binary-exact camera controls make global translation testable without
    # confounding binary64 rounding of decimal input camera coordinates.
    eye = np.round(np.asarray(row["eye"]) * 8) / 8
    target = np.round(np.asarray(row["target"]) * 8) / 8
    for name, translation, motion in (
        ("regional", 0, [0, 0, 0]),
        ("translated", 2**30, [0, 0, 0]),
        ("next", 0, [8, -4, 1]),
        ("next-translated", 2**30, [8, -4, 1]),
    ):
        here = eye + translation + motion
        scene = dict(
            row,
            id=name,
            eye=here.tolist(),
            target=(target + translation + motion).tolist(),
            render_origin_m=here.tolist(),
            bodies=[
                dict(b, origin_m=(np.asarray(b["origin_m"]) + translation).tolist())
                for b in bodies
            ],
            output=str(output / (name + ".png")),
        )
        scene.pop("camera_basis", None)
        write_json(output / (name + "-scene.json"), scene)
        names.append(name)
    return names


def run(args):
    commit = frozen()
    fresh(args.output)
    row = json.loads(args.scene.read_text())
    bodies = source_bodies(row)
    args.output.mkdir()
    began = time.perf_counter()
    encoded, proofs = [], []
    for index, (body, data) in enumerate(bodies):
        item, proof = encode_body(args.output, body, data, index)
        encoded.append(item)
        proofs.append(proof)
    legacy = dict(row, output=str(args.output / "legacy.png"))
    write_json(args.output / "legacy-scene.json", legacy)
    scenes = camera_pairs(row, encoded, args.output)
    result = {
        "commit": commit,
        "original_scene": str(args.scene.resolve()),
        "original_scene_sha256": digest(args.scene),
        "profile": row["profile"],
        "bodies": proofs,
        "scenes": ["legacy", *scenes],
        "encoding_and_diagnostic_seconds": time.perf_counter() - began,
        "scope": "Actual complete saved body arrays and separate render encoding; source normals/materials/topology remain. Relative render positions are not physical source admission. Orientation/position observations do not replace closed volume/contact/profile/controller checks or native GPU/device admission.",
    }
    write_json(args.output / "comparison.json", result)
    print({"bodies": len(proofs), "faces": sum(p["faces"] for p in proofs)}, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

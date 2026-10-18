"""Finished-pose cache hit, independent rigid geometry and demanded organ replay."""

import argparse
import hashlib
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_posture import pose_group
from crown_shoots import CrownShoots
from outshine_ref.vegetation import Shoot
from spatial_crown_cost import restore
from spatial_crown_replay import check_mesh
from tree_entry import load


def read_pose(inputs, expected_source):
    header = json.loads((inputs / "posture.json").read_text())
    if (
        header["format"] != "spatial-crown-posture-1"
        or header["source_identity"] != expected_source
    ):
        raise ValueError("matching source crown and static posture format required")
    payload = inputs / "posture.npz"
    if hashlib.sha256(payload.read_bytes()).hexdigest() != header["payload_sha256"]:
        raise ValueError("saved posture payload changed")
    with np.load(payload, allow_pickle=False) as data:
        values = {key: data[key] for key in data.files}
    identity_header = {
        k: v for k, v in header.items() if k not in ("identity", "payload_sha256")
    }
    digest = hashlib.sha256(json.dumps(identity_header, sort_keys=True).encode())
    for key in ("source", "parents", "mobility"):
        digest.update(values[key].view(np.uint8))
    if digest.hexdigest() != header["identity"]:
        raise ValueError("posture recipe or source identity changed")
    return header, values


def geometry_checks(before, after):
    original, posed = before["wood.npz"], after["wood.npz"]
    np.testing.assert_array_equal(original["faces"], posed["faces"])

    def areas(mesh):
        p = mesh["vertices"][mesh["faces"]]
        return (
            np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1) / 2
        )

    a, b = areas(original), areas(posed)
    np.testing.assert_allclose(a, b, rtol=2e-10, atol=1e-16)
    report = check_mesh(posed)
    leaf_area = 0.0
    for variant in range(8):
        name = f"blade-{variant}.npz"
        for key, value in before[name].items():
            np.testing.assert_array_equal(value, after[name][key])
        count = int(np.count_nonzero(after["tables.npz"]["leaf_variants"] == variant))
        leaf_area += count * areas(after[name]).sum()
    for key in ("leaf_ids", "leaf_variants"):
        np.testing.assert_array_equal(
            before["tables.npz"][key], after["tables.npz"][key]
        )
    frames = after["tables.npz"]["leaf_frames"]
    np.testing.assert_allclose(np.linalg.det(frames), 1, atol=3e-14)
    report.update(
        wood_max_area_error_m2=float(abs(a - b).max()),
        physical_leaf_area_m2=float(leaf_area),
    )
    return report


def run(source, inputs, output):
    start = time.perf_counter()
    crown = restore(source)
    with (
        patch("crown_posture.posture", side_effect=AssertionError("kinematics on hit")),
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch.object(CrownShoots, "group", side_effect=AssertionError("organs on hit")),
        patch.object(Shoot, "detail", side_effect=AssertionError("organs on hit")),
    ):
        header, pose = read_pose(inputs, crown.identity)
        _, before = load(inputs / "recursive")
        _, after = load(inputs / "spatial")
    loaded = time.perf_counter()
    geometry = geometry_checks(before, after)
    np.testing.assert_array_equal(pose["source"], crown.positions)
    np.testing.assert_array_equal(pose["parents"], crown.parents)
    lengths = np.linalg.norm(
        crown.positions[1:] - crown.positions[crown.parents[1:]], axis=1
    )
    new_lengths = np.linalg.norm(
        pose["positions"][1:] - pose["positions"][crown.parents[1:]], axis=1
    )
    np.testing.assert_allclose(lengths, new_lengths, atol=2e-14, rtol=0)
    rotations = pose["rotations"]
    np.testing.assert_allclose(
        rotations @ rotations.transpose(0, 2, 1),
        np.broadcast_to(np.eye(3), rotations.shape),
        atol=2e-14,
    )
    assert not crown._groups
    for node in crown.hosts[::-1]:
        rows = pose_group(crown, pose["positions"], rotations, int(node), 100000)
        index = rows["ids"].astype(int)
        for key in ("ids", "variants", "origins", "frames"):
            expected = after["tables.npz"]["leaf_" + key][index]
            np.testing.assert_allclose(rows[key], expected, atol=2e-14, rtol=0)
        np.testing.assert_allclose(
            rows["origins"] - crown.petiole_m * rows["frames"][:, :, 1],
            rows["nodes"],
            atol=2e-14,
            rtol=0,
        )
    result = {
        "identity": header["identity"],
        "source_identity": crown.identity,
        "cache_read_s": loaded - start,
        "checks_s": time.perf_counter() - loaded,
        "groups": len(crown._groups),
        "leaves": len(crown.variants),
        "geometry": geometry,
        "scope": "Finished static pose hit forbids growth/kinematics/organ work; separate reversed demanded-organ replay and rigid finite mesh checks. No wind/collision/welded skin/world/device admission.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.inputs, args.output)

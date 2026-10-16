"""Fresh complete products, source-bound foliage replay and whole-geometry controls."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_shoots import CrownShoots
from outshine_ref.vegetation import Blade, PlantPlan, Shoot
from spatial_crown_cost import restore
from tree_entry import load


def check_mesh(mesh):
    vertices, faces, normals = mesh["vertices"], mesh["faces"], mesh["normals"]
    assert np.isfinite(vertices).all() and np.isfinite(normals).all()
    assert faces.dtype.kind in "iu" and faces.min() >= 0 and faces.max() < len(vertices)
    p = vertices[faces]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    assert np.all(np.linalg.norm(cross, axis=1) > 0)
    assert normals.shape == (faces.size, 3)
    assert np.all(np.sum(cross[:, None] * normals.reshape(-1, 3, 3), axis=2) > 0)
    np.testing.assert_allclose(np.linalg.norm(normals, axis=1), 1, atol=2e-7)
    return {
        "faces": len(faces),
        "array_bytes": sum(a.nbytes for a in mesh.values()),
        "minimum_double_area_m2": float(np.linalg.norm(cross, axis=1).min()),
    }


def run(inputs, output):
    start = time.perf_counter()
    with (
        patch.object(
            PlantPlan, "tables", side_effect=AssertionError("placements on hit")
        ),
        patch.object(
            CrownShoots, "tables", side_effect=AssertionError("shoots on hit")
        ),
        patch.object(Shoot, "detail", side_effect=AssertionError("organs on hit")),
        patch.object(Blade, "mesh", side_effect=AssertionError("meshes on hit")),
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
    ):
        _, before = load(inputs / "recursive")
        _, after = load(inputs / "spatial")
    loaded = time.perf_counter()
    for name in ("leaf_ids", "leaf_variants"):
        np.testing.assert_array_equal(
            before["tables.npz"][name], after["tables.npz"][name]
        )
    area = np.zeros(8)
    for variant in range(8):
        name = f"blade-{variant}.npz"
        for key, values in before[name].items():
            np.testing.assert_array_equal(values, after[name][key])
        mesh = after[name]
        p = mesh["vertices"][mesh["faces"]]
        area[variant] = (
            np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum()
            / 2
        )
    crown = restore(inputs)
    with patch.object(Shoot, "detail", side_effect=AssertionError("organs in summary")):
        summary = crown.summary()
    assert not crown._groups
    for node in crown.hosts[::-1]:
        crown.group(int(node), 100000)
    rows = crown.tables(100000)
    for suffix in ("ids", "variants", "origins", "frames"):
        np.testing.assert_array_equal(
            rows[suffix], after["tables.npz"]["leaf_" + suffix]
        )
    np.testing.assert_allclose(
        rows["origins"] - crown.petiole_m * rows["frames"][:, :, 1],
        rows["nodes"],
        atol=1e-13,
    )
    np.testing.assert_allclose(np.linalg.det(rows["frames"]), 1, atol=1e-13)
    geometry = {
        name: check_mesh(mesh) for name, mesh in after.items() if name != "tables.npz"
    }
    result = {
        "identity": crown.identity,
        "summary": summary,
        "ready_read_s": loaded - start,
        "remaining_checks_s": time.perf_counter() - loaded,
        "mesh_leaf_area_m2_both": float(area[rows["variants"]].sum()),
        "geometry": geometry,
        "checks": "passed",
        "scope": "Exact shared variants/leaf meshes and reversed partial replay, independent whole cache read and physical geometry checks. Separate capped segment bodies, not welded tree manifold; no species/LOD/native claim.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)

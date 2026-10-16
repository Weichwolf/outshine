"""Whole cached plant, local new material, complete topology and unchanged organ rows."""

import argparse
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_union_replay import topology
from plant_surface_checks import winding_inside
from spatial_crown_replay import check_mesh
from tree_entry import load


def material_probes(base, after, collars, bounds, positions):
    nodes = positions[np.linspace(1, len(positions) - 1, 96, dtype=int)]
    faces = after["faces"]
    triangles = after["vertices"][faces]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    eligible = np.flatnonzero(
        (after["source_kind"] == 2) & (np.linalg.norm(cross, axis=1) > 1e-7)
    )
    assert len(eligible)
    selected = eligible[np.linspace(0, len(eligible) - 1, 64, dtype=int)]
    normal = cross[selected] / np.linalg.norm(cross[selected], axis=1)[:, None]
    centre = triangles[selected].mean(axis=1)
    new_surface = np.r_[centre + 1e-4 * normal, centre - 1e-4 * normal]
    rng = np.random.default_rng(443)
    random = rng.uniform(
        after["vertices"].min(axis=0) - 0.1,
        after["vertices"].max(axis=0) + 0.1,
        (512, 3),
    )
    outside = np.ones(len(random), bool)
    for low, high in bounds:
        outside &= np.any((random < low) | (random > high), axis=1)
    far = random[outside][:96]
    assert len(far) == 96
    points = np.vstack((nodes, new_surface, far))
    original = winding_inside(base, points)
    expected = original.copy()
    for collar in collars:
        expected |= winding_inside(collar, points)
    actual = winding_inside(after, points)
    np.testing.assert_array_equal(actual, expected)
    assert original[:96].all() and actual[:96].all()
    np.testing.assert_array_equal(actual[-96:], original[-96:])
    return {
        "node_probes": 96,
        "new_surface_side_probes": 128,
        "outside_all_collar_bounds_probes": 96,
        "new_material_probes": int(np.sum(actual & ~original)),
    }


def run(inputs, products, output):
    with (
        patch(
            "crown_collar_experiment.demanded_collars",
            side_effect=AssertionError("fields on hit"),
        ),
        patch(
            "collar_union_product.add_collars",
            side_effect=AssertionError("union on hit"),
        ),
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch(
            "crown_shoots.CrownShoots.tables",
            side_effect=AssertionError("placement on hit"),
        ),
    ):
        before, original = load(inputs / "spatial")
        metadata, loaded = load(products / "spatial")
    assert metadata["plan_identity"] == before["plan_identity"]
    unchanged = []
    for root, target in (
        (inputs / "spatial", products / "spatial"),
        (inputs, products),
    ):
        for source in root.iterdir():
            if not source.is_file() or source.name in (
                "wood.npz",
                "entry.json",
                "comparison.json",
            ):
                continue
            other = target / source.name
            if not other.is_file():
                continue
            assert (
                hashlib.sha256(source.read_bytes()).digest()
                == hashlib.sha256(other.read_bytes()).digest()
            )
            unchanged.append(source.name)
    base, after = original["wood.npz"], loaded["wood.npz"]
    checked = check_mesh(after) | topology(after)
    old_topology = topology(base)
    added = checked["signed_volume_m3"] - old_topology["signed_volume_m3"]
    assert added > 0
    plans = json.loads((products / "collar-plans.json").read_text())
    collars, bounds = [], []
    for plan in plans:
        node = plan["fork_node"]
        with np.load(products / f"collar-{node}.npz") as z:
            collar = {k: z[k] for k in z.files}
        check_mesh(collar)
        topology(collar)
        collars.append(collar)
        low, high = np.asarray(plan["bounds_m"])
        bounds.append((low, high))
        chosen = (after["source_kind"] == 2) & (after["source_node"] == node)
        points = after["vertices"][after["faces"][chosen]]
        assert np.all(points >= low - 1e-10) and np.all(points <= high + 1e-10)
    assert set(after["source_node"][after["source_kind"] == 2]) <= {
        p["fork_node"] for p in plans
    }
    with np.load(products / "spatial-plan.npz") as z:
        points = z["positions"]
    probes = material_probes(base, after, collars, bounds, points)
    result = {
        "checks": "passed",
        "source_identity": metadata["plan_identity"],
        "geometry_identity": metadata["geometry_identity"],
        "geometry": checked,
        "added_volume_m3": added,
        "unchanged_files": sorted(set(unchanged)),
        "stored_leaf_rows": len(loaded["tables.npz"]["leaf_ids"]),
        "material_probes": probes,
        "scope": "Closed positive oriented connected whole plant and local field meshes; exact cached organs/plan, bounded new-source facets and sampled source-union material equivalence. No global self-intersection/C1/error, Float32/native/device/LOD/wind/species certificate.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.products, args.output)

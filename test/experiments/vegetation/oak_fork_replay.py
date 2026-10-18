"""Cold cache decode, exact untouched geometry and independent local occupancy/topology."""

import argparse
import hashlib
import json
import resource
import time
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_collar_accuracy import field_from_plan, ray_accuracy
from crown_union_replay import source_inside, topology
from fork_surface import voxel_mesh
from plant_surface_checks import winding_inside
from spatial_crown_replay import check_mesh
from tree_entry import load


def source_axes(wood, owners):
    meshes = []
    for owner in owners:
        faces = wood["faces"][wood["limb_ids"] == owner]
        ids, inverse = np.unique(faces, return_inverse=True)
        meshes.append(
            {"vertices": wood["vertices"][ids], "faces": inverse.reshape(-1, 3)}
        )
    return meshes


def unchanged_wood(before, after, owners):
    old = ~np.isin(before["limb_ids"], owners)
    new = after["patch_nodes"] == np.iinfo(np.uint32).max
    np.testing.assert_array_equal(
        before["vertices"][before["faces"][old]], after["vertices"][after["faces"][new]]
    )
    for field, width in (("normals", 3), ("corner_uv", 2)):
        np.testing.assert_array_equal(
            before[field].reshape(-1, 3, width)[old],
            after[field].reshape(-1, 3, width)[new],
        )
    for field in ("surface", "limb_ids"):
        np.testing.assert_array_equal(before[field][old], after[field][new])
    return int(old.sum())


def occupancy(mesh, sources, collars, plans):
    triangles = mesh["vertices"][mesh["faces"]]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    eligible = np.flatnonzero(
        (mesh["source_kind"] == 2) & (np.linalg.norm(cross, axis=1) > 1e-9)
    )
    assert len(eligible) >= 64
    selected = eligible[np.linspace(0, len(eligible) - 1, 64, dtype=int)]
    normals = cross[selected] / np.linalg.norm(cross[selected], axis=1)[:, None]
    centres = triangles[selected].mean(axis=1)
    near = np.r_[centres + normals * 1e-5, centres - normals * 1e-5]
    rng = np.random.default_rng(687)
    samples = []
    for plan in plans:
        low, high = np.asarray(plan["bounds_m"])
        samples.append(rng.uniform(low, high, (128, 3)))
    points = np.vstack([near, *samples])
    original = source_inside(sources, points)
    expected = original.copy()
    for collar in collars:
        expected |= winding_inside(collar, points)
    actual = winding_inside(mesh, points)
    np.testing.assert_array_equal(actual, expected)
    assert np.any(actual & ~original)
    return {
        "independent_solid_angle_probes": len(points),
        "new_material_probes": int(np.sum(actual & ~original)),
    }


def run(inputs, products, output):
    began = time.perf_counter()
    with ExitStack() as guards:
        for symbol in (
            "colonization.grow",
            "crown_shoots.CrownShoots.tables",
            "oak_fork_product.build",
            "oak_fork_product.join_sources",
            "oak_fork_product.blocked_voxel_mesh",
            "oak_fork_product.add_collars",
        ):
            guards.enter_context(
                patch(symbol, side_effect=AssertionError("generation on cache hit"))
            )
        before, original = load(inputs / "spatial")
        metadata, loaded = load(products / "spatial")
    decoded = time.perf_counter()
    recipe = metadata["product_recipe"]
    assert recipe["source_product_identity"] == before["product_identity"]
    assert (
        recipe["source_wood_sha256"]
        == hashlib.sha256((inputs / "spatial/wood.npz").read_bytes()).hexdigest()
    )
    assert (
        metadata["product_identity"]
        == hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    )
    unchanged = []
    for file in (inputs / "spatial").iterdir():
        if file.name in ("entry.json", "wood.npz") or not file.is_file():
            continue
        assert file.read_bytes() == (products / "spatial" / file.name).read_bytes()
        unchanged.append(file.name)
    mesh = loaded["wood.npz"]
    checks = check_mesh(mesh)
    groups = json.loads((products / "fork-plans.json").read_text())
    all_owners = [o for g in groups for o in g["owners"]]
    assert len(all_owners) == len(set(all_owners))
    untouched = unchanged_wood(original["wood.npz"], mesh, all_owners)
    results = []
    for group in groups:
        node = min(group["nodes"])
        with np.load(products / f"patch-{node}.npz") as z:
            local = dict(z)
        chosen = mesh["patch_nodes"] == node
        np.testing.assert_array_equal(
            mesh["vertices"][mesh["faces"][chosen]], local["vertices"][local["faces"]]
        )
        np.testing.assert_array_equal(
            mesh["normals"].reshape(-1, 3, 3)[chosen],
            local["normals"].reshape(-1, 3, 3),
        )
        assert set(local["source_node"][local["source_kind"] == 2]) <= set(
            group["nodes"]
        )
        checks_local = check_mesh(local) | topology(local)
        assert checks_local["genus"] == 0
        collars, accuracy = [], []
        for plan in group["forks"]:
            field = field_from_plan(plan)
            collar, _ = voxel_mesh(field, recipe["cells"])
            collars.append(collar)
            accuracy.append(ray_accuracy(collar, plan))
        assert all(a["two_mm_probe_contract_met"] for a in accuracy)
        sources = source_axes(original["wood.npz"], group["owners"])
        checks_local.update(occupancy(local, sources, collars, group["forks"]))
        results.append(
            {"nodes": group["nodes"], "checks": checks_local, "accuracy": accuracy}
        )
    report = {
        "product_identity": metadata["product_identity"],
        "unchanged_files": unchanged,
        "exact_untouched_wood_faces": untouched,
        "whole_surface": checks,
        "groups": results,
        "paired_guarded_decode_s": decoded - began,
        "paired_decoded_array_bytes": sum(
            a.nbytes
            for products in (original, loaded)
            for data in products.values()
            for a in data.values()
        ),
        "separate_verifier_s": time.perf_counter() - decoded,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Finite solid-angle/ray controls and local closed genus-0 topology; separate whole-tree components, no global self-intersection/C1/Float32/species/LOD/device admission.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.products, args.output)

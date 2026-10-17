"""Independent scalar weighting and exact cache/geometry checks for contact shading."""

import argparse
import hashlib
import json
import resource
import time
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

import numpy as np
from join_normal_checks import scalar_angles
from spatial_crown_replay import check_mesh
from tree_entry import load


def check_patch(before, after):
    for name in before.keys() - {"normals"}:
        np.testing.assert_array_equal(before[name], after[name])
    faces = before["faces"]
    kind = before["source_kind"]
    contact = np.intersect1d(np.unique(faces[kind == 2]), np.unique(faces[kind != 2]))
    wanted = scalar_angles(before)
    expected = before["normals"].copy()
    vertex_indices = faces.ravel()
    triangles = before["vertices"][faces]
    geometric = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    accepted, rejected, before_spread, after_spread = [], [], 0.0, 0.0
    for vertex in contact:
        corners = np.flatnonzero(vertex_indices == vertex)
        old = before["normals"][corners]
        candidate = wanted[vertex]
        if np.any(old @ candidate < np.cos(np.pi / 4)) or np.any(
            geometric[corners // 3] @ candidate <= 0
        ):
            rejected.append(int(vertex))
            continue
        accepted.append(int(vertex))
        expected[corners] = candidate
        before_spread = max(before_spread, float(np.ptp(old, axis=0).max()))
        after_spread = max(
            after_spread, float(np.ptp(after["normals"][corners], axis=0).max())
        )
    np.testing.assert_allclose(after["normals"], expected, rtol=0, atol=2e-14)
    local = np.isin(vertex_indices, accepted)
    np.testing.assert_array_equal(after["normals"][~local], before["normals"][~local])
    changed = np.any(after["normals"] != before["normals"], axis=1)
    assert not np.any(changed & ~local)
    return {
        "contacts": len(contact),
        "accepted": len(accepted),
        "rejected": rejected,
        "changed_corners": int(changed.sum()),
        "before_max_component_spread": before_spread,
        "after_max_component_spread": after_spread,
        "oriented_surface": check_mesh(after),
    }


def run(inputs, products, output):
    began = time.perf_counter()
    with ExitStack() as guards:
        for symbol in (
            "fork_shading.build",
            "fork_shading.shade",
            "fork_shading.smooth_join_corners",
            "join_normals.smooth_join_corners",
            "colonization.grow",
            "crown_shoots.CrownShoots.tables",
        ):
            guards.enter_context(
                patch(
                    symbol,
                    side_effect=AssertionError("normal/growth work on cache hit"),
                )
            )
        before, original = load(inputs / "spatial")
        entry, loaded = load(products / "spatial")
    decoded = time.perf_counter()
    recipe = entry["product_recipe"]
    assert entry["geometry_product_identity"] == before["product_identity"]
    assert recipe["shading"]["source_product_identity"] == before["product_identity"]
    assert (
        recipe["shading"]["source_wood_sha256"]
        == hashlib.sha256((inputs / "spatial/wood.npz").read_bytes()).hexdigest()
    )
    assert recipe["shading"]["method"] == "angle"
    assert (
        entry["product_identity"]
        == hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    )
    unchanged_files = []
    for source in (inputs / "spatial").iterdir():
        if source.name in ("entry.json", "wood.npz") or not source.is_file():
            continue
        assert source.read_bytes() == (products / "spatial" / source.name).read_bytes()
        unchanged_files.append(source.name)
    old, new = original["wood.npz"], loaded["wood.npz"]
    for name in old.keys() - {"normals"}:
        np.testing.assert_array_equal(old[name], new[name])
    geometry = check_mesh(new)
    plans = json.loads((products / "fork-plans.json").read_text())
    checks = []
    changed = np.any(old["normals"] != new["normals"], axis=1)
    covered = np.zeros(len(changed), bool)
    for plan in plans:
        node = min(plan["nodes"])
        with np.load(inputs / f"patch-{node}.npz", allow_pickle=False) as z:
            a = dict(z)
        with np.load(products / f"patch-{node}.npz", allow_pickle=False) as z:
            b = dict(z)
        check = check_patch(a, b)
        selected = np.repeat(new["patch_nodes"] == node, 3)
        np.testing.assert_array_equal(new["normals"][selected], b["normals"])
        covered |= selected
        checks.append({"group": node, **check})
    assert not np.any(changed & ~covered)
    report = {
        "product_identity": entry["product_identity"],
        "geometry_product_identity": before["product_identity"],
        "unchanged_files": sorted(unchanged_files),
        "whole_surface": geometry,
        "groups": checks,
        "whole_changed_corners": int(changed.sum()),
        "paired_guarded_decode_s": decoded - began,
        "separate_scalar_check_s": time.perf_counter() - decoded,
        "paired_decoded_array_bytes": sum(
            a.nbytes for p in (original, loaded) for m in p.values() for a in m.values()
        ),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Exact saved geometry/material/leaf data, independent scalar weighting and finite positive corner normals; no geometric C1/species/LOD/native/device admission.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.products, args.output)

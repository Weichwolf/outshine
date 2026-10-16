"""Independent complete topology, cached organ equality and occupancy controls."""

import argparse
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_union_experiment import prepared
from plant_surface_checks import winding_inside
from scipy.sparse import coo_array
from scipy.sparse.csgraph import connected_components
from spatial_crown_cost import restore
from spatial_crown_replay import check_mesh
from tree_entry import load


def topology(mesh):
    faces = mesh["faces"]
    edges = np.stack((faces, np.roll(faces, -1, axis=1)), axis=-1).reshape(-1, 2)
    keys = np.sort(edges, axis=1)
    _, inverse, count = np.unique(keys, axis=0, return_inverse=True, return_counts=True)
    assert np.all(count == 2)
    assert np.all(np.bincount(inverse, weights=np.sign(edges[:, 1] - edges[:, 0])) == 0)
    order = np.argsort(inverse, kind="stable").reshape(-1, 2)
    opposite = np.empty(len(edges), np.int64)
    opposite[order[:, 0]], opposite[order[:, 1]] = order[:, 1], order[:, 0]
    successor = opposite // 3 * 3 + (opposite % 3 + 1) % 3
    visited = np.zeros(len(edges), bool)
    fans = np.zeros(len(mesh["vertices"]), np.int32)
    for first in range(len(edges)):
        if visited[first]:
            continue
        corner = first
        fans[edges[first, 0]] += 1
        while not visited[corner]:
            assert edges[corner, 0] == edges[first, 0]
            visited[corner] = True
            corner = successor[corner]
        assert corner == first
    assert np.all(fans == 1)
    graph = coo_array(
        (np.ones(len(edges)), (edges[:, 0], edges[:, 1])), shape=(len(fans), len(fans))
    ).tocsr()
    assert connected_components(graph, directed=False, return_labels=False) == 1
    euler = len(fans) - len(count) + len(faces)
    assert euler % 2 == 0 and euler <= 2
    return {
        "connected_components": 1,
        "single_vertex_fans": len(fans),
        "euler_characteristic": euler,
        "genus": (2 - euler) // 2,
    }


def source_inside(sources, points):
    inside = np.zeros(len(points), bool)
    for source in sources:
        v = source["vertices"]
        candidate = (
            ~inside
            & np.all(points > v.min(axis=0), axis=1)
            & np.all(points < v.max(axis=0), axis=1)
        )
        indices = np.flatnonzero(candidate)
        if len(indices):
            inside[indices] = winding_inside(source, points[indices])
    return inside


def run(inputs, plan, products, output):
    with (
        patch(
            "crown_union_experiment.join_sources",
            side_effect=AssertionError("union on hit"),
        ),
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch(
            "crown_shoots.CrownShoots.tables",
            side_effect=AssertionError("placement on hit"),
        ),
    ):
        before, original = load(products / "recursive")
        metadata, loaded = load(products / "spatial")
    assert before["plan_identity"] == metadata["plan_identity"]
    for file in (products / "recursive").iterdir():
        if file.name not in ("entry.json", "wood.npz") and file.is_file():
            other = products / "spatial" / file.name
            assert (
                hashlib.sha256(file.read_bytes()).digest()
                == hashlib.sha256(other.read_bytes()).digest()
            )
    mesh = loaded["wood.npz"]
    checked = check_mesh(mesh)
    checked.update(topology(mesh))
    sources, _ = prepared(inputs, plan)
    core = [s for s in sources if s["source_kind"] == 0]
    shoots = [s for s in sources if s["source_kind"] == 1]
    prefixes = np.concatenate([s["vertices"][:4] for s in shoots])
    assert source_inside(core, prefixes).all()
    crown = restore(plan)
    nodes = crown.positions.copy()
    nodes[0] += (nodes[1] - nodes[0]) * 1e-3
    tips = np.array([s["vertices"][12:20].mean(axis=0) for s in shoots])
    required = np.concatenate((nodes, tips))
    assert winding_inside(mesh, required).all()
    rng = np.random.default_rng(429)
    bounds = mesh["vertices"]
    points = rng.uniform(bounds.min(axis=0) - 0.1, bounds.max(axis=0) + 0.1, (128, 3))
    np.testing.assert_array_equal(
        winding_inside(mesh, points), source_inside(sources, points)
    )
    saved = json.loads((products / "wood-source-nodes.json").read_text())
    covered = {}
    for kind, expected in (
        (0, {s["source_node"] for s in core}),
        (1, {s["source_node"] for s in shoots}),
    ):
        actual = set(mesh["source_node"][mesh["source_kind"] == kind])
        assert actual <= expected and set(saved[str(kind)]) == expected
        covered[str(kind)] = sorted(expected - actual)
    assert restore(products).identity == crown.identity
    report = {
        "source_identity": metadata["plan_identity"],
        "geometry_identity": metadata["geometry_identity"],
        "checks": "passed",
        "geometry": checked,
        "all_node_and_shoot_interior_probes": len(required),
        "source_or_probes": len(points),
        "contained_root_prefix_vertices": len(prefixes),
        "stored_leaf_rows": len(original["tables.npz"]["leaf_ids"]),
        "covered_sources_without_external_faces": covered,
        "scope": "Independent closed oriented connected vertex-fan topology, finite positive facets and solid-angle samples; no global geometric self-intersection or continuous volume/error certificate, native float32/device admission.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "plan", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.plan, args.products, args.output)

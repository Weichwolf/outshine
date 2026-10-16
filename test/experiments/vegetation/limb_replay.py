"""Check the actual saved whole crown independently of growth and foliage builders."""

import argparse
import hashlib
import json
from itertools import pairwise
from pathlib import Path
from unittest.mock import patch

import numpy as np
from colonization import pipe_radii
from limb_experiment import canonical_wood
from limb_surface import path_recipe
from spatial_crown_cost import restore
from spatial_crown_replay import check_mesh
from tree_entry import load
from wood_mesh import branch_tubes


def run(source, products, output):
    crown = restore(source)
    with (
        patch(
            "limb_experiment.canonical_wood",
            side_effect=AssertionError("meshing on hit"),
        ),
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch(
            "crown_shoots.CrownShoots.tables",
            side_effect=AssertionError("foliage on hit"),
        ),
    ):
        old_entry, old = load(products / "recursive")
        entry, new = load(products / "spatial")
    assert old_entry["plan_identity"] == entry["plan_identity"] == crown.identity
    unchanged = []
    for file in (products / "recursive").iterdir():
        if file.name in ("wood.npz", "entry.json") or not file.is_file():
            continue
        other = products / "spatial" / file.name
        assert (
            hashlib.sha256(file.read_bytes()).digest()
            == hashlib.sha256(other.read_bytes()).digest()
        )
        unchanged.append(file.name)
    paths = json.loads((products / "limb-paths.json").read_text())
    edges = np.array([edge for path in paths for edge in pairwise(path)])
    np.testing.assert_array_equal(
        np.sort(edges[:, 1]), np.arange(1, len(crown.parents))
    )
    np.testing.assert_array_equal(edges[:, 0], crown.parents[edges[:, 1]])
    radii = pipe_radii(crown.parents, 0.14)
    geometry = []
    with np.load(products / "limbs-only.npz") as data:
        body = {key: data[key] for key in data.files}
    face_offset, vertex_offset = 0, 0
    for path in paths:
        stations = (len(path) - 1) * 2 + 1
        nv, nf = stations * 8 + 2, stations * 16
        v = body["vertices"][vertex_offset : vertex_offset + nv]
        f = body["faces"][face_offset : face_offset + nf] - vertex_offset
        n = body["normals"][face_offset * 3 : (face_offset + nf) * 3]
        geometry.append(check_mesh({"vertices": v, "faces": f, "normals": n}))
        incidence = np.sort(
            np.concatenate([f[:, pair] for pair in ([0, 1], [1, 2], [2, 0])]), axis=1
        )
        unique, count = np.unique(incidence, axis=0, return_counts=True)
        assert np.all(count == 2) and nv - len(unique) + nf == 2
        directed = np.concatenate([f[:, pair] for pair in ([0, 1], [1, 2], [2, 0])])
        _, summed = np.unique(np.sort(directed, axis=1), axis=0, return_inverse=True)
        assert np.all(
            np.bincount(summed, weights=np.sign(directed[:, 1] - directed[:, 0])) == 0
        )
        p = v[f]
        volume = np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
        assert volume > 0
        spine, _ = path_recipe(crown.positions, radii, path)
        np.testing.assert_allclose(
            spine.evaluate(np.linspace(0, 1, len(path))),
            crown.positions[path],
            atol=2e-14,
        )
        face_offset += nf
        vertex_offset += nv
    assert face_offset == len(body["faces"]) and vertex_offset == len(body["vertices"])
    rebuilt, rebuilt_paths = canonical_wood(crown)
    assert tuple(map(tuple, paths)) == rebuilt_paths
    for key, values in body.items():
        np.testing.assert_array_equal(values, rebuilt[key])
    with patch(
        "limb_experiment.limb_mesh",
        side_effect=AssertionError("mesh before whole demand"),
    ):
        assert canonical_wood(crown, max_faces=face_offset - 1) is None
    baseline = branch_tubes(crown.positions, crown.parents, radii, 8)
    ov, of = len(baseline[0]), len(baseline[1])
    np.testing.assert_array_equal(
        old["wood.npz"]["vertices"][ov:], new["wood.npz"]["vertices"][vertex_offset:]
    )
    np.testing.assert_array_equal(
        old["wood.npz"]["faces"][of:] - ov,
        new["wood.npz"]["faces"][face_offset:] - vertex_offset,
    )
    np.testing.assert_array_equal(
        old["wood.npz"]["normals"][of * 3 :],
        new["wood.npz"]["normals"][face_offset * 3 :],
    )
    result = {
        "source_identity": crown.identity,
        "geometry_identity": entry["geometry_identity"],
        "limbs": len(paths),
        "canonical_edges": len(edges),
        "unchanged_files": sorted(unchanged),
        "body_faces": face_offset,
        "body_vertices": vertex_offset,
        "minimum_double_area_m2": min(v["minimum_double_area_m2"] for v in geometry),
        "checks": "passed",
        "whole_mesh": check_mesh(new["wood.npz"]),
        "scope": "Closed oriented individual limbs and exact stored foliage/twig replay; no whole-tree union or global self-intersection certificate.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.products, args.output)

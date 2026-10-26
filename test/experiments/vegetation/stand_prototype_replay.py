"""Fresh standalone finished-product read and optional scalar full-source comparison."""

import argparse
import hashlib
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from spatial_crown_replay import check_mesh
from stand_leaf_product import ready, source_files
from tree_entry import load


def compare_source(inputs, metadata, products):
    source_meta, source = load(inputs)
    fingerprint = hashlib.sha256(
        json.dumps(source_files(inputs, source_meta), sort_keys=True).encode()
    ).hexdigest()
    assert fingerprint == metadata["source_product_sha256"]
    for key, values in source["wood.npz"].items():
        np.testing.assert_array_equal(values, products["wood.npz"][key])
    rows, runs = source[source_meta["tables"]], products[metadata["tables"]]
    mesh = products["leaf-mesh.npz"]
    index = {int(identity): i for i, identity in enumerate(rows["leaf_ids"])}
    assert len(index) == len(runs["ids"]) == len(rows["leaf_ids"])
    assert set(index) == set(map(int, runs["ids"]))
    position_error = normal_error = 0.0
    vertex_end = face_end = 0
    for i, identity in enumerate(runs["ids"]):
        row = index[int(identity)]
        variant = int(rows["leaf_variants"][row])
        blade = source[f"blade-{variant}.npz"]
        frame, origin = rows["leaf_frames"][row], rows["leaf_origins"][row]
        nv, nf = len(blade["vertices"]), len(blade["faces"])
        assert int(runs["variants"][i]) == variant
        assert int(runs["vertices"][i]) == nv and int(runs["faces"][i]) == nf
        assert int(runs["first_vertex"][i]) == vertex_end
        assert int(runs["first_face"][i]) == face_end
        v, f = slice(vertex_end, vertex_end + nv), slice(face_end, face_end + nf)
        c = slice(face_end * 3, (face_end + nf) * 3)
        expected = blade["vertices"] @ frame.T + origin
        normals = blade["normals"] @ frame.T
        position_error = max(
            position_error, float(abs(mesh["vertices"][v] - expected).max())
        )
        normal_error = max(normal_error, float(abs(mesh["normals"][c] - normals).max()))
        np.testing.assert_array_equal(mesh["faces"][f], blade["faces"] + vertex_end)
        np.testing.assert_array_equal(
            mesh["corner_uv"][c], blade["corner_uv"].astype(np.float32)
        )
        np.testing.assert_array_equal(mesh["surface"][f], blade["surface"])
        vertex_end += nv
        face_end += nf
    assert vertex_end == len(mesh["vertices"]) and face_end == len(mesh["faces"])
    assert position_error <= 1e-6 and normal_error <= 1e-6
    assert (
        metadata["instanced_triangles"] == len(products["wood.npz"]["faces"]) + face_end
    )
    return {
        "physical_leaves": len(index),
        "position_error_m": position_error,
        "normal_error": normal_error,
        "wood_arrays": "exact",
        "uv_faces_organ_runs": "exact",
    }


def run(inputs, source, output):
    start = time.perf_counter()
    with (
        patch(
            "stand_leaf_product.pack_leaves",
            side_effect=AssertionError("packing on hit"),
        ),
        patch(
            "stand_leaf_product.source_files",
            side_effect=AssertionError("source read on hit"),
        ),
        patch("tree_entry.build", side_effect=AssertionError("generation on hit")),
    ):
        metadata, products = ready(inputs)
    loaded = time.perf_counter()
    meshes = {name: check_mesh(products[name]) for name in metadata["products"]}
    comparison = compare_source(source, metadata, products) if source else None
    result = {
        "identity": metadata["identity"],
        "source_product_sha256": metadata["source_product_sha256"],
        "finished_read_s": loaded - start,
        "checks_s": time.perf_counter() - loaded,
        "finished_file_bytes": sum(
            p.stat().st_size for p in inputs.iterdir() if p.is_file()
        ),
        "decoded_array_bytes": sum(
            a.nbytes for p in products.values() for a in p.values()
        ),
        "decoded_arrays": sum(len(p) for p in products.values()),
        "mesh_checks": meshes,
        "source_comparison": comparison,
        "scope": "Complete static rendering product; source comparison optional and separate from hit. No native/cache API/device/LOD/wind admission.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.source, args.output)

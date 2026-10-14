"""Fresh-process cached plant loading with all procedural constructors forbidden."""

import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Blade, PlantPlan, Shoot

ROOT = Path("build/experiments/vegetation/plant-library/assets")


def decode_products(manifest):
    tables = {}
    with np.load(ROOT / manifest["table"]) as source:
        for key in source.files:
            tables[key] = source[key]
    meshes, poses = {}, {}
    for product in manifest["near"] + manifest["coarse"]:
        for key, container in (("mesh", meshes), ("placements", poses)):
            name = product[key]
            with np.load(ROOT / name) as source:
                container[name] = {key: source[key] for key in source.files}
    return tables, meshes, poses


def check_rows(manifest, tables, meshes, poses):
    axes, leaves = manifest["axes"], manifest["leaves"]
    np.testing.assert_array_equal(tables["axis_ids"], np.arange(axes, dtype=np.uint64))
    np.testing.assert_array_equal(
        tables["leaf_ids"], np.arange(leaves, dtype=np.uint64)
    )
    assert np.all(tables["axis_parent_ids"][1:] < tables["axis_ids"][1:])
    assert np.all(tables["leaf_axis_ids"] < axes)
    for detail in ("near", "coarse"):
        leaf_ids = []
        wood_ids = []
        for p in manifest[detail]:
            mesh = meshes[p["mesh"]]
            pose = poses[p["placements"]]
            ids = pose["organ_ids"]
            if p["surface"] == "wood":
                wood_ids.extend(ids.tolist())
                np.testing.assert_array_equal(
                    pose["origins"], tables["axis_origins"][ids]
                )
                np.testing.assert_array_equal(
                    pose["frames"], tables["axis_frames"][ids]
                )
            else:
                leaf_ids.extend(ids.tolist())
                np.testing.assert_array_equal(
                    pose["origins"], tables["leaf_origins"][ids]
                )
                np.testing.assert_array_equal(
                    pose["frames"], tables["leaf_frames"][ids]
                )
                np.testing.assert_array_equal(
                    pose["parent_axis_ids"], tables["leaf_axis_ids"][ids]
                )
                np.testing.assert_array_equal(
                    tables["leaf_variants"][ids], p["variant"]
                )
            for name in ("vertices", "normals"):
                assert mesh[name].dtype == np.float64 and np.isfinite(mesh[name]).all()
            assert mesh["faces"].dtype == np.int32
            triangle = mesh["vertices"][mesh["faces"]]
            cross = np.cross(
                triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
            )
            assert np.all(
                np.sum(cross[:, None] * mesh["normals"].reshape(-1, 3, 3), axis=2) > 0
            )
        np.testing.assert_array_equal(wood_ids, np.arange(axes))
        np.testing.assert_array_equal(np.sort(leaf_ids), np.arange(leaves))


def main():
    manifest = json.loads((ROOT / "asset.json").read_text())
    began = time.perf_counter()
    for name, digest in manifest["files_sha256"].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    hash_s = time.perf_counter() - began
    with (
        patch.object(
            PlantPlan, "from_recipe", side_effect=AssertionError("plan rebuild")
        ),
        patch.object(PlantPlan, "axis", side_effect=AssertionError("axis rebuild")),
        patch.object(PlantPlan, "leaves", side_effect=AssertionError("leaf rebuild")),
        patch.object(Shoot, "detail", side_effect=AssertionError("shoot rebuild")),
        patch.object(Blade, "mesh", side_effect=AssertionError("prototype rebuild")),
    ):
        began = time.perf_counter()
        tables, meshes, poses = decode_products(manifest)
        decode_s = time.perf_counter() - began
    check_rows(manifest, tables, meshes, poses)
    report = {
        "identity": manifest["identity"],
        "axes": manifest["axes"],
        "leaves": manifest["leaves"],
        "all_file_hashes": len(manifest["files_sha256"]),
        "hash_s": hash_s,
        "decode_s": decode_s,
        "decode_scope": "canonical arrays, both full mesh/placement levels; no image decode or GPU upload",
        "no_procedural_rebuild": True,
        "full_near_coarse_ids_and_owners_exact": True,
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
    }
    (ROOT.parent / "replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

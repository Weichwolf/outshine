"""Use existing whole-product checks and replay demanded source groups in reverse."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_shoots import CrownShoots
from oak_scaffold import tapered_radii
from outshine_ref.vegetation import Blade, Shoot
from spatial_crown_experiment import crown_wood
from spatial_crown_replay import check_mesh
from tree_entry import load


def check_wood_binding(inputs, name, metadata, crown, wood):
    entry = json.loads((inputs / name / "entry.json").read_text())
    expected = hashlib.sha256(
        json.dumps(entry["product_recipe"], sort_keys=True).encode()
    ).hexdigest()
    assert (
        expected
        == entry["product_identity"]
        == metadata["products"][name]["product_identity"]
    )
    assert entry["product_recipe"]["recipe"] == metadata["recipe"]
    assert entry["product_recipe"]["leaf_plan_identity"] == crown.identity
    radii = tapered_radii(
        crown.parents,
        metadata["recipe"]["root_radius_m"],
        metadata["recipe"]["terminal_radius_m"],
    )
    reconstructed = crown_wood(crown, radii=radii)
    for key, value in reconstructed.items():
        np.testing.assert_array_equal(value, wood[key])


def replay(inputs, output, *, wood_check=None):
    wood_check = check_wood_binding if wood_check is None else wood_check
    began = time.perf_counter()
    metadata = json.loads((inputs / "comparison.json").read_text())
    assert metadata["format"] == "oak-scaffold-study-1"
    products = {}
    with (
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch.object(Shoot, "detail", side_effect=AssertionError("placement on hit")),
        patch.object(Blade, "mesh", side_effect=AssertionError("meshing on hit")),
    ):
        for name in ("recursive", "spatial"):
            products[name] = load(inputs / name)[1]
    loaded = time.perf_counter()
    before, after = (products[n] for n in ("recursive", "spatial"))
    for key in ("leaf_ids", "leaf_variants"):
        np.testing.assert_array_equal(
            before["tables.npz"][key], after["tables.npz"][key]
        )
    for variant in range(8):
        key = f"blade-{variant}.npz"
        for field, a in before[key].items():
            np.testing.assert_array_equal(a, after[key][field])
    reports = {}
    for name, product in products.items():
        rows = product["tables.npz"]
        with np.load(inputs / name / "skeleton.npz") as saved:
            skeleton = {k: saved[k] for k in saved.files}
        for key in ("positions", "parents"):
            source = skeleton["seed_" + key]
            np.testing.assert_array_equal(skeleton[key][: len(source)], source)
        crown = CrownShoots(
            skeleton["positions"],
            skeleton["parents"],
            rows["leaf_variants"],
            metadata["recipe"]["seed"],
            length_m=0.35,
            bend_rad=0.4,
            foliage_depth_m=1.1,
        )
        assert crown.identity == metadata["products"][name]["identity"]
        with patch.object(
            Shoot, "detail", side_effect=AssertionError("detail before demand")
        ):
            summary = crown.summary()
            assert not crown._groups
            first = int(crown.hosts[0])
            capacity = int(crown.counts[0])
            assert crown.group(first, capacity - 1) is None
        for node in crown.hosts[::-1]:
            group = crown.group(int(node), 100000)
            ids = group["ids"].astype(np.intp)
            for key, value in group.items():
                np.testing.assert_array_equal(value, rows["leaf_" + key][ids])
        np.testing.assert_allclose(np.linalg.det(rows["leaf_frames"]), 1, atol=2e-14)
        np.testing.assert_allclose(
            rows["leaf_origins"] - 0.004 * rows["leaf_frames"][:, :, 1],
            rows["leaf_nodes"],
            atol=1e-13,
        )
        wood_check(inputs, name, metadata, crown, product["wood.npz"])
        geometry = {k: check_mesh(v) for k, v in product.items() if k != "tables.npz"}
        reports[name] = {
            "identity": crown.identity,
            "summary": summary,
            "geometry": geometry,
        }
    hashes = {
        str(p.relative_to(inputs)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in inputs.rglob("*")
        if p.is_file()
    }
    result = {
        "cache_decode_s": loaded - began,
        "replay_and_geometry_s": time.perf_counter() - loaded,
        "products": reports,
        "hashes": hashes,
        "checks": "passed",
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "finished hit forbids growth/placement/meshing; separately regenerated requested groups and checked complete meshes, not native/LOD/species acceptance",
    }
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private reports must be under/tmp")
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    replay(args.inputs, args.output)

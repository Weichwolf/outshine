"""Separate saved-row oracle and cold/repeated group work, never a runtime cache."""

import argparse
import hashlib
import json
import resource
import shutil
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from spatial_group_source import load_source


def cold_fixture(inputs, folder):
    """Verifier prepares an input directory containing no near geometry or poses."""
    (folder / "spatial").mkdir(parents=True, exist_ok=False)
    shutil.copy2(inputs / "comparison.json", folder / "comparison.json")
    with np.load(inputs / "spatial/skeleton.npz", allow_pickle=False) as z:
        np.savez_compressed(
            folder / "spatial/skeleton.npz",
            positions=z["positions"],
            parents=z["parents"],
        )
    with np.load(inputs / "spatial/tables.npz", allow_pickle=False) as z:
        np.savez_compressed(
            folder / "spatial/tables.npz", leaf_variants=z["leaf_variants"]
        )
    for name in ("fern-material.json", "fern-colour.png", "fern-height.png"):
        shutil.copy2(inputs / "spatial" / name, folder / "spatial" / name)
    return {
        str(p.relative_to(folder)): p.stat().st_size
        for p in folder.rglob("*")
        if p.is_file()
    }


def replay(inputs, controls, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    files = cold_fixture(inputs, output.with_suffix(""))
    began = time.perf_counter()
    source = load_source(output.with_suffix(""))
    loaded = time.perf_counter()
    with patch(
        "outshine_ref.vegetation.Shoot.detail",
        side_effect=AssertionError("refused pose work"),
    ):
        assert source.geometry(int(source.crown.hosts[0]), 0) is None
    assert not source.prototypes and not source.crown._groups
    with np.load(inputs / "spatial/tables.npz", allow_pickle=False) as z:
        rows = {k: z[k] for k in ("leaf_origins", "leaf_frames", "leaf_variants")}
    oracle_loaded = time.perf_counter()
    checked = 0
    maximum = 0.0
    for node in map(int, source.crown.hosts):
        source.crown._groups.clear()
        group = source.crown.group(node, 128)
        ids = group["ids"]
        np.testing.assert_array_equal(group["origins"], rows["leaf_origins"][ids])
        np.testing.assert_array_equal(group["frames"], rows["leaf_frames"][ids])
        np.testing.assert_array_equal(group["variants"], rows["leaf_variants"][ids])
        checked += len(ids)
    oracle_done = time.perf_counter()
    source.crown._groups.clear()
    results = []
    report = json.loads((controls / "report.json").read_text())
    with patch(
        "crown_shoots.CrownShoots.tables",
        side_effect=AssertionError("whole producer table"),
    ):
        for node in report["nodes"][::-1]:
            start = time.perf_counter()
            geometry = source.geometry(node)
            cold_s = time.perf_counter() - start
            ids = geometry["ids"]
            world = (
                geometry["vertices"] @ geometry["anchor_frame"].T
                + geometry["anchor_origin"]
            )
            offset = 0
            for organ, variant in enumerate(geometry["leaf_variants"]):
                mesh = source.prototypes[int(variant)]
                expected = (
                    mesh["vertices"] @ rows["leaf_frames"][ids[organ]].T
                    + rows["leaf_origins"][ids[organ]]
                )
                maximum = max(
                    maximum,
                    float(abs(world[offset : offset + len(expected)] - expected).max()),
                )
                offset += len(expected)
            assert maximum < 1e-12
            start = time.perf_counter()
            again = source.geometry(node)
            repeat_s = time.perf_counter() - start
            for k, a in geometry.items():
                if isinstance(a, np.ndarray):
                    np.testing.assert_array_equal(a, again[k])
            start = time.perf_counter()
            with (
                patch.object(
                    source,
                    "geometry",
                    side_effect=AssertionError("finished-product generation"),
                ),
                np.load(controls / f"host-{node}/source.npz", allow_pickle=False) as z,
            ):
                stored = dict(z)
            decode_s = time.perf_counter() - start
            for k, a in stored.items():
                np.testing.assert_array_equal(a, geometry[k])
            results.append(
                {
                    "host_node": node,
                    "triangles": len(geometry["faces"]),
                    "array_count": len(stored),
                    "array_payload_bytes": sum(a.nbytes for a in stored.values()),
                    "cold_group_s": cold_s,
                    "repeat_same_group_s": repeat_s,
                    "finished_npz_decode_s": decode_s,
                }
            )
    result = {
        "source_identity": source.crown.identity,
        "catalog_identity": source.identity,
        "source_decode_and_prepare_s": loaded - began,
        "cold_input_files_bytes": files,
        "separate_oracle_decode_s": oracle_loaded - loaded,
        "separate_all_source_pose_check_s": oracle_done - oracle_loaded,
        "exact_original_pose_rows": checked,
        "maximum_same_source_world_coordinate_error_m": maximum,
        "requested_groups": results,
        "blade_prototypes": len(source.prototypes),
        "retained_pose_groups": len(source.crown._groups),
        "original_table_file_sha256": hashlib.sha256(
            (inputs / "spatial/tables.npz").read_bytes()
        ).hexdigest(),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Whole original pose table loaded only by separate verifier. Group regeneration differs from finished diagnostic NPZ decode. Counts are ndarray payloads, not allocation/traffic totals; no runtime cache or native admission.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "controls", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    replay(a.inputs, a.controls, a.output)

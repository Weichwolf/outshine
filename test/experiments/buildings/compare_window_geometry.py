"""Matched raw-geometry controls and complete-template costs; no native format proposal."""

import argparse
import hashlib
import json
import os
import resource
import time
import tracemalloc
from functools import lru_cache
from pathlib import Path
from unittest.mock import patch

import numpy as np
import opening_details
from compare_window_detail_demand import fingerprint, stage
from evidence import revision
from terraced_facade_details import details
from terraced_facade_prepare import positions
from test_window_detail_demand import cases
from window_detail_demand import buffer_bytes, window_closed
from window_geometry_plan import compile_geometry
from window_geometry_templates import frozen, load, save


def originals(prepared, shared):
    original = opening_details.template
    cache = lru_cache(maxsize=16)(original) if shared else original
    with patch.object(opening_details, "template", cache):
        result = tuple(
            {k: frozen(v) for k, v in window_closed(prepared, w).items()}
            for w in prepared.facade.windows
        )
    if shared:
        cache.cache_clear()
    return result


def batch(prepared):
    raw = details(prepared.local, prepared.facade, legacy_trim=False)
    return raw | {"vertices": positions(raw["vertices"], prepared.mass.core)}


def save_originals(output, prepared, values):
    vcounts = [len(x["vertices"]) for x in values]
    fcounts = [len(x["faces"]) for x in values]
    offsets = np.array(
        [np.cumsum([0] + vcounts), np.cumsum([0] + fcounts)], dtype=np.uint32
    ).T
    data = {k: np.concatenate([x[k] for x in values]) for k in values[0]}
    np.savez_compressed(output / "original.npz", offsets=offsets, **data)
    (output / "original.json").write_text(
        json.dumps(
            {
                "source_id": prepared.mass.identifier,
                "recipe": prepared.mass.kind,
                "identifiers": [w.identifier for w in prepared.facade.windows],
            },
            indent=2,
        )
        + "\n"
    )
    return int(offsets.nbytes)


def verify(prepared, asset, expected):
    point_matches, triangle_matches, worst = 0, 0, 0.0
    for window, reference in zip(prepared.facade.windows, expected, strict=True):
        value = asset.emit(window.identifier)
        a = np.unique(value["vertices"].astype(np.float32), axis=0)
        b = np.unique(reference["vertices"].astype(np.float32), axis=0)
        point_matches += int(a.shape == b.shape and a.tobytes() == b.tobytes())
        a, b = (
            np.unique(value["vertices"], axis=0),
            np.unique(reference["vertices"], axis=0),
        )
        distances = np.linalg.norm(a[:, None] - b[None], axis=2)
        worst = max(
            worst, float(max(distances.min(axis=0).max(), distances.min(axis=1).max()))
        )
        a = triangle_rows(value)
        b = triangle_rows(reference)
        triangle_matches += int(a.shape == b.shape and a.tobytes() == b.tobytes())
    return {
        "same_f32_vertex_sets": point_matches,
        "same_f32_triangles_modulo_cyclic_start": triangle_matches,
        "worst_double_vertex_distance_m": worst,
    }


def triangle_rows(raw):
    points = raw["vertices"][raw["faces"]].astype(np.float32)
    rows = []
    for p, material in zip(points, raw["materials"], strict=True):
        row = min((np.roll(p, i, axis=0).reshape(-1) for i in range(3)), key=tuple)
        rows.append(np.r_[row, material])
    values = np.asarray(rows)
    return values[np.lexsort(values[:, ::-1].T)]


def one(output, prepared, trace, check, reverse):
    output.mkdir(parents=True, exist_ok=False)
    if trace:
        tracemalloc.start()
    costs, values = {}, {}
    methods = ["original", "frame_shared", "batch", "complete_template"]
    if reverse:
        methods.reverse()
    for method in methods:
        call = (
            (lambda: originals(prepared, False))
            if method == "original"
            else (lambda: originals(prepared, True))
            if method == "frame_shared"
            else (lambda: batch(prepared))
            if method == "batch"
            else (lambda: compile_geometry(prepared))
        )
        values[method], costs[method] = stage(call, trace)
    asset = values["complete_template"]
    emitted, costs["emission_all"] = stage(
        lambda: tuple(asset.emit(i) for i in asset.identifiers), trace
    )
    if trace:
        tracemalloc.stop()
    reference = values["original"]
    for first, second in zip(reference, values["frame_shared"], strict=True):
        assert fingerprint(first) == fingerprint(second)
    offset_bytes = save_originals(output, prepared, reference)
    start = time.perf_counter()
    save(asset, output / "asset")
    save_seconds = time.perf_counter() - start
    start = time.perf_counter()
    restored = load(output / "asset", asset.source_id, asset.recipe)
    read_seconds = time.perf_counter() - start
    hashes = [fingerprint(value) for value in emitted]
    assert hashes == [fingerprint(restored.emit(i)) for i in restored.identifiers]
    return {
        "source_id": asset.source_id,
        "recipe": asset.recipe,
        "windows": len(asset.identifiers),
        "templates": len(asset.templates),
        "method_order": methods,
        "costs": costs,
        "raw_original_array_bytes": sum(buffer_bytes(x) for x in reference),
        "raw_offset_bytes": offset_bytes,
        "batch_array_bytes": buffer_bytes(values["batch"]),
        "template_array_bytes": asset.array_bytes,
        "emitted_array_bytes_logical": sum(buffer_bytes(x) for x in emitted),
        "instance_new_position_bytes": sum(x["vertices"].nbytes for x in emitted),
        "template_save_seconds": save_seconds,
        "template_read_seconds": read_seconds,
        "original_snapshot_bytes": (output / "original.npz").stat().st_size
        + (output / "original.json").stat().st_size,
        "template_snapshot_bytes": sum(
            p.stat().st_size for p in (output / "asset").iterdir()
        ),
        "emitted_hashes": hashes,
        "geometry_comparison": verify(prepared, asset, reference) if check else None,
    }


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze geometry benchmark sources before running")
    args.output.mkdir(parents=True, exist_ok=False)
    load_average = os.getloadavg()
    start = time.perf_counter()
    plans = cases()
    preparation = time.perf_counter() - start
    records = [
        one(
            args.output / f"case-{i}",
            prepared,
            args.trace,
            args.verify,
            bool((i % 2) ^ args.reverse),
        )
        for i, prepared in enumerate(plans)
    ]
    fixtures = [
        Path("test/experiments/buildings/data") / name
        for name in ("stepped-profiles.json", "terraced-admission-controls.json")
    ]
    report = proof | {
        "trace": args.trace,
        "verify": args.verify,
        "host_load_average_at_start": load_average,
        "plan_preparation_seconds": preparation,
        "records": records,
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in fixtures
        },
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Trusted experimental NPZ snapshots, not native serialization/DB/SSD/UMA/GPU residency. All four raw controls charged; batch has whole-source part namespace and lacks independent per-window output, so it is a geometry/work context. Complete-template changes triangle ordering/UV-related draw semantics; only tested as outer geometry with original visible details. Source/metadata/IDs shared; Python-object overhead not in logical array sizes. Raw emission allocates positions; immutable topology/material/part arrays share template storage. All diagnostics and held controls enter process RSS; scoped trace misses some native heaps. No same-quality winner inferred before actual image comparison.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"producer": proof["commit"], "cases": len(records)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--reverse", action="store_true")
    run(parser.parse_args())

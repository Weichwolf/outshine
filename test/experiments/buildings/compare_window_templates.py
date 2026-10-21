"""Frozen original singleton generator versus bounded update-local template reuse."""

import argparse
import hashlib
import importlib.util
import json
import os
import resource
import subprocess
import sys
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import opening_details
import shapely
import window_detail_demand as shared
from compare_window_detail_demand import fingerprint, requests, stage
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from terraced_facade_probe import materials
from test_window_detail_demand import cases

BASELINE_COMMIT = "97862e7162b7f8d254d2d81ab6730cbf56259c80"
BASELINE_PATH = "test/experiments/buildings/window_detail_demand.py"


def baseline(output):
    data = subprocess.check_output(
        ["git", "show", f"{BASELINE_COMMIT}:{BASELINE_PATH}"]
    )
    path = output / "baseline_window_detail_demand.py"
    path.write_bytes(data)
    spec = importlib.util.spec_from_file_location("window_detail_baseline", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Cannot load the frozen baseline")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module.__name__] = module
    spec.loader.exec_module(module)
    return module, hashlib.sha256(data).hexdigest()


def inputs():
    started = time.perf_counter()
    plans = cases()
    elapsed = time.perf_counter() - started
    varied = replace(
        plans[0],
        facade=replace(
            plans[0].facade,
            windows=tuple(
                replace(w, width_m=w.width_m - 0.005 * i)
                for i, w in enumerate(plans[0].facade.windows[:21])
            ),
        ),
    )
    result = []
    for name, p in (("courts", plans[0]), ("concave", plans[5]), ("compact", plans[8])):
        poses, selections = requests(p)
        result.append((name, p, poses[0], selections))
    poses, _ = requests(plans[0])
    ids = tuple(w.identifier for w in varied.facade.windows)
    result.append(("distinct-dimensions", varied, poses[0], [ids, ids, (), ids]))
    return result, elapsed


def measure(output, name, prepared, view, selections, module, traced, proof):
    if traced:
        tracemalloc.start()
    product, coarse = stage(lambda: module.WindowDetails(prepared), traced)
    template_builds, template_bytes = 0, 0
    original = opening_details.template

    def template(*key):
        nonlocal template_builds, template_bytes
        result = original(*key)
        template_builds += 1
        template_bytes += sum(value.nbytes for value in result.values())
        return result

    rows, snapshots = [], {}
    with patch.object(opening_details, "template", template):
        for i, ids in enumerate(selections):
            before = template_builds, template_bytes
            draw, cost = stage(lambda ids=ids: product.update(ids), traced)
            rows.append(
                cost
                | {
                    "selected": list(draw.detailed_ids),
                    "generated": list(draw.generated_ids),
                    "released": list(draw.released_ids),
                    "fallback": list(draw.fallback_ids),
                    "template_builds": template_builds - before[0],
                    "template_raw_bytes_built": template_bytes - before[1],
                    "generated_detail_bytes": draw.generated_buffer_bytes,
                    "retained_detail_bytes": draw.resident_detail_bytes,
                }
            )
            if i in (0, len(selections) - 1):
                # Diagnostic materialization/export is outside every update timer.
                snapshots[str(i)] = shared.materialize(draw)
            del draw
    if traced:
        tracemalloc.stop()
    hashes = {i: fingerprint(mesh) for i, mesh in snapshots.items()}
    for i, mesh in snapshots.items():
        np.savez_compressed(output / f"{name}-state-{i}.npz", **mesh)
    if name == "courts":
        write(
            output,
            "Near",
            snapshots["0"],
            {
                "camera": camera(view),
                "source_id": prepared.mass.identifier,
                "source": {"material_definitions": materials()},
                "facade_materials": {},
                "scope": "Identical explicit diagnostic subset and full original coarse hull; no new LOD/radiance/shadow qualification.",
            },
            proof,
        )
    return {
        "name": name,
        "source_id": prepared.mass.identifier,
        "kind": prepared.mass.kind,
        "source_wkb_sha256": hashlib.sha256(prepared.mass.source_wkb).hexdigest(),
        "windows": len(prepared.facade.windows),
        "coarse": coarse,
        "coarse_bytes": shared.buffer_bytes(product.coarse.mesh),
        "coarse_hashes": fingerprint(product.coarse.mesh),
        "states": rows,
        "snapshots": hashes,
    }


def run(args):
    load = os.getloadavg()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the measured implementation before running")
    args.output.mkdir(parents=True, exist_ok=False)
    old, source_sha = baseline(args.output)
    prepared, preparation_seconds = inputs()
    results = {}
    order = (
        (("shared", shared), ("unshared", old))
        if args.shared_first
        else (("unshared", old), ("shared", shared))
    )
    for method, module in order:
        destination = args.output / method
        destination.mkdir()
        results[method] = [
            measure(destination, *values, module, args.trace, proof)
            for values in prepared
        ]
    for first, second in zip(results["shared"], results["unshared"], strict=True):
        assert first["snapshots"] == second["snapshots"]
        assert first["coarse_hashes"] == second["coarse_hashes"]
        for a, b in zip(first["states"], second["states"], strict=True):
            for key in (
                "selected",
                "generated",
                "released",
                "fallback",
                "generated_detail_bytes",
                "retained_detail_bytes",
            ):
                assert a[key] == b[key], key
    fixtures = [
        Path("test/experiments/buildings/data") / name
        for name in ("stepped-profiles.json", "terraced-admission-controls.json")
    ]
    report = {
        "producer": proof["commit"],
        "host_load_average_at_start": load,
        "trace": args.trace,
        "method_order": [item[0] for item in order],
        "baseline_commit": BASELINE_COMMIT,
        "baseline_source_sha256": source_sha,
        "python": sys.version,
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in fixtures
        },
        "plan_preparation_seconds": preparation_seconds,
        "results": results,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Linux shared-host Python generator comparison. Same explicit demand, coarse hull, frozen original per-window manager and all draw fields. All-original geometry equality is a separate test. First/last diagnostic copies and serialization outside timers, but held snapshots/imports/plans included in process RSS. Counters include same template-call instrumentation; native GEOS heap not fully traced. Raw template totals count construction buffers, not all allocation/copy traffic. No DB, native frame, device or application-memory claim. No persistent template cache or instancing representation.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "producer": proof["commit"],
                "same_fields": True,
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--shared-first", action="store_true")
    run(parser.parse_args())

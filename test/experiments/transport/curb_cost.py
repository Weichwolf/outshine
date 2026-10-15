"""Isolated normal/allocation costs on actual footprints admitted by both methods."""

import argparse
import contextlib
import gc
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_experiment import builder
from curb_inputs import FIELD_PIN, GROUND_PIN
from curb_plan import HEIGHT_M
from curb_verify import actual
from outshine_ref.contracts import Mesh
from rail_instance_cost import profile_note


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh curb cost outputs required")
    report = json.loads((args.products / "comparison.json").read_text())
    rows = {
        m: {r["candidate"]: r for r in report["comparison"][m]["rows"] if r["valid"]}
        for m in ["fine", "field"]
    }
    common = sorted(set(rows["fine"]) & set(rows["field"]))
    if report["dirty"] or not common:
        raise ValueError("clean shared successful curb comparison required")
    filename, pin = (
        ("ground.npz", GROUND_PIN)
        if args.method == "fine"
        else ("field.npz", FIELD_PIN)
    )
    path = args.network / filename
    if digest(path) != pin:
        raise ValueError("actual curb height input changed")
    with np.load(path) as d:
        mesh = Mesh(d["vertices"], d["faces"])
    source = builder(mesh)
    shapes = [
        shapely.from_wkb(bytes.fromhex(report["candidates"][i]["footprint_wkb_hex"]))
        for i in common
    ]
    expected = []
    for i in common:
        row = rows[args.method][i]
        path = Path(row["path"])
        if digest(path) != row["sha256"]:
            raise ValueError("saved curb comparison changed")
        value, _ = actual(path)
        expected.append(value[0])
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings = []
    products = None
    for _ in range(1 if args.profile else 7):
        del products
        gc.collect()
        began = time.perf_counter()
        with tracker:
            values = [source.build(s, minimum_height_m=HEIGHT_M - 1e-7) for s in shapes]
            products = [p for p, n in values]
        timings.append((time.perf_counter() - began) * 1000)
    if any(p is None for p in products):
        raise ValueError("measured curb method lost a shared valid case")
    for p, q in zip(products, expected, strict=True):
        if not np.array_equal(p.body.vertices, q.body.vertices) or not np.array_equal(
            p.body.triangles, q.body.triangles
        ):
            raise ValueError("measured construction differs from actual saved geometry")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "method": args.method,
        "profiled": args.profile is not None,
        "shared_candidates": common,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "preparation_seconds": source.preparation_seconds,
        "source_height_faces": len(mesh.triangles),
        "body_vertices": sum(len(p.body.vertices) for p in products),
        "body_faces": sum(len(p.body.triangles) for p in products),
        "owned_body_array_bytes": sum(
            p.body.vertices.nbytes + p.body.triangles.nbytes for p in products
        ),
        "owned_contact_array_bytes": sum(
            m.vertices.nbytes + m.triangles.nbytes
            for p in products
            for m in [p.lower_contact, p.upper_contact]
        ),
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only repeated whole-contact volume construction after source/index preparation on the actual same footprints admitted by both normal methods. Each method matches its own saved triangulation exactly; physical same-height quality verified separately. Failed/one-sided cases remain in the full comparison. No equal-topology, complete copy count, native/device/frame claim.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["products", "network", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["fine", "field"], required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    measure(args)

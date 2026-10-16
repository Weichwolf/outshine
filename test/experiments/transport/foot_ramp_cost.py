"""Equivalent zero-mouth weld costs on the actual saved finite ramp caps only."""

import argparse
import contextlib
import gc
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from curb_verify import actual
from foot_ramp_geometry import weld
from rail_instance_cost import profile_note


def raw_shell(product):
    low, high = product.lower_contact, product.upper_contact
    if not np.array_equal(low.triangles, high.triangles) or not np.array_equal(
        low.vertices[:, :2], high.vertices[:, :2]
    ):
        raise ValueError("actual shared-XY ramp caps required")
    faces, n = high.triangles, len(high.vertices)
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    _keys, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    a, b = edges[counts[inverse] == 1].T
    walls = np.vstack([np.c_[a, b + n, b], np.c_[a, a + n, b + n]])
    return np.vstack([high.vertices, low.vertices]), np.vstack(
        [faces, faces[:, ::-1] + n, walls]
    )


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh ramp weld cost outputs required")
    report = json.loads((args.products / "comparison.json").read_text())
    if report["dirty"] or digest(args.products / "ramps.npz") != report["ramps_sha256"]:
        raise ValueError("actual clean saved ramp caps required")
    products, _data = actual(args.products / "ramps.npz")
    sources = [raw_shell(p) for p in products]
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    values, timings = None, []
    for _ in range(1 if args.profile else 7):
        del values
        gc.collect()
        began = time.perf_counter()
        with tracker:
            values = [weld(v, f, args.method) for v, f in sources]
        timings.append((time.perf_counter() - began) * 1000)
    for result, product in zip(values, products, strict=True):
        if not np.array_equal(
            result.vertices, product.body.vertices
        ) or not np.array_equal(result.triangles, product.body.triangles):
            raise ValueError(
                "equivalent weld changed the actual saved indexed ramp body"
            )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "method": args.method,
        "profiled": args.profile is not None,
        "ramp_bodies": len(products),
        "input_vertices": sum(len(v) for v, _f in sources),
        "input_faces": sum(len(f) for _v, f in sources),
        "input_array_bytes": sum(v.nbytes + f.nbytes for v, f in sources),
        "output_vertices": sum(len(v.vertices) for v in values),
        "output_faces": sum(len(v.triangles) for v in values),
        "owned_output_array_bytes": sum(
            v.vertices.nbytes + v.triangles.nbytes for v in values
        ),
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only exact indexed zero-mouth welding/zero-index-face removal after actual saved cap decode and raw-shell preparation. No source selection, profile meshing, border edit, contact audit, packing or render. Normal/profiled times separate; internal full copies not measured. No whole-generator/native/device claim.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["products", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["vector", "scalar"], required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    measure(parser.parse_args())

"""Identical prepared geometry/quality; accepted output and rejected attempt costs."""

import argparse
import contextlib
import gc
import json
import resource
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from stair_blend_audit import inspect
from stair_blend_solve import candidate, validate_candidate
from stair_outer_inputs import load
from stair_terrain_experiment import frozen
from stair_terrain_layout import selected_surfaces
from stair_terrain_solid import retaining_body


def inputs(products):
    report = json.loads((products / "comparison.json").read_text())
    verified = json.loads((products / "verification.json").read_text())
    if (
        not verified["valid_local_terrain"]
        or digest(products / "plan.npz") != report["artifacts"]["plan.npz"]
        or verified["products_report_sha256"] != digest(products / "comparison.json")
        or digest(products / "membrane-field.npz")
        != report["artifacts"]["membrane-field.npz"]
        or digest(products / "membrane-body.npz")
        != report["artifacts"]["membrane-body.npz"]
    ):
        raise ValueError("verified fixed terrain plan required")
    source = Path(report["network_products"]) / "after/body.npz"
    if digest(source) != report["source_body_sha256"]:
        raise ValueError("measured original whole network changed")
    data = dict(np.load(source))
    _case, rows, _nodes, _stairs, _context, _recipe = load()
    ground, upper = selected_surfaces(data, rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    reference = dict(np.load(products / "membrane-field.npz"))
    return dict(np.load(products / "plan.npz")), ground, upper, footprint, reference


def accept(layout, ground, upper, reference, heights, model, note):
    validate_candidate(layout, heights, model, note)
    if not note["accepted_field"]:
        return False, False
    identical = np.array_equal(heights, reference["heights"])
    if not identical:
        body, parts = retaining_body(dict(layout, targets=heights))
        inspect(body, parts, ground, upper)
    return True, identical


def measure(args):
    commit = frozen()
    if args.output.exists() or (args.profile is not None and args.profile.exists()):
        raise ValueError("fresh explicit cost/profile output required")
    layout, ground, upper, footprint, reference = inputs(args.products)
    warmups = []
    for smoothing in (0, 16):
        heights, model, note = candidate(layout, upper, footprint, smoothing=smoothing)
        good, identical = accept(layout, ground, upper, reference, heights, model, note)
        warmups.append(
            {
                "smoothing_m2": smoothing,
                "accepted_field": good,
                "actual_reference_identical": identical,
                "solver_status": note["solver_status"],
            }
        )
    del heights, model, note
    tracker = contextlib.nullcontext()
    if args.profile is not None:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings, reused = [], []
    for _ in range(1 if args.profile is not None else args.repetitions):
        gc.collect()
        began = time.perf_counter()
        with tracker:
            heights, model, note = candidate(
                layout, upper, footprint, smoothing=args.smoothing
            )
        timings.append((time.perf_counter() - began) * 1000)
        good, identical = accept(layout, ground, upper, reference, heights, model, note)
        reused.append(identical)
        work = dict(note)
        del heights, model
        if not good:
            break
    result = {
        "commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "plan_sha256": digest(args.products / "plan.npz"),
        "smoothing_m2": args.smoothing,
        "phase_ms": timings,
        "accepted_field": good,
        "median_ms": float(np.median(timings)) if good else None,
        "warmups": warmups,
        "verified_reference_byte_identity": reused,
        "work": work,
        "profiled": args.profile is not None,
        "output_array_bytes": work["height_output_bytes"] if good else 0,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Matrix/whole-bound preparation and one bounded solve after imports/decode/fixed mesh and two warm candidates. Every result checked outside timing; byte-identical heights reuse the verified whole-body proof, otherwise the body is rebuilt/audited. Normal and profiled times separate. No native/frame/device or whole-generator factor.",
        "copy_scope": "Retained P/A bytes and output bytes counted; internal solver/copy totals unobserved. Validation quantization and geometry audits are outside the measured phase.",
    }
    if not good:
        result["rejected_attempt_ms"] = timings[-1]
        result["scope"] += " Rejected attempt; no valid-output median or throughput."
    if args.profile is not None:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in ("accepted_field", "median_ms", "worker_peak_rss_bytes")
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--smoothing", type=int, choices=(0, 16), required=True)
    parser.add_argument("--repetitions", type=int, choices=range(1, 22), default=7)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    measure(parser.parse_args())

"""Same saved whole terrain: implied-cone elimination versus every Float32 cone."""

import argparse
import contextlib
import gc
import json
import resource
import statistics
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from stair_blend_audit import inspect
from stair_blend_solve import candidate, validate_candidate
from stair_cycle_inputs import load
from stair_terrain_experiment import frozen
from stair_terrain_layout import selected_surfaces
from stair_terrain_solid import retaining_body


def inputs(products, mode):
    report = json.loads((products / "comparison.json").read_text())
    verified = json.loads((products / "verification.json").read_text())
    if not verified["valid_local_candidate"] or verified[
        "products_report_sha256"
    ] != digest(products / "comparison.json"):
        raise ValueError("verified complete terrain exports required")
    for name, expected in report["artifacts"].items():
        if digest(products / name) != expected:
            raise ValueError("measured terrain artifact changed")
    if digest(products / "after/body.npz") != report["after"]["body_sha256"]:
        raise ValueError("measured whole road body changed")
    fixture = Path(report["fixture_path"])
    if digest(fixture) != report["fixture_sha256"]:
        raise ValueError("measured source instance changed")
    _case, rows, _nodes, _stairs, _context, _sample = load(fixture)
    ground, upper = selected_surfaces(dict(np.load(products / "after/body.npz")), rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    layout = dict(np.load(products / "terrain-plan.npz"))
    layout["precision_mode"] = np.array(mode)
    checked = [dict(np.load(products / "membrane-field.npz"))["heights"]]
    return layout, ground, upper, footprint, checked


def admit(layout, ground, upper, checked, heights, model, note):
    validate_candidate(layout, heights, model, note)
    if not note["accepted_field"]:
        return False, False
    same = any(np.array_equal(heights, reference) for reference in checked)
    if not same:
        body, parts = retaining_body(dict(layout, targets=heights))
        inspect(body, parts, ground, upper)
        checked.append(heights)
    return True, same


def measure(args):
    commit = frozen()
    if args.output.exists() or (args.profile and args.profile.exists()):
        raise ValueError("fresh explicit terrain cost/profile outputs required")
    if args.output.with_suffix(".npz").exists():
        raise ValueError("fresh explicit terrain example output required")
    layout, ground, upper, footprint, checked = inputs(args.products, args.mode)
    heights, model, note = candidate(layout, upper, footprint)
    warm, _same = admit(layout, ground, upper, checked, heights, model, note)
    del heights, model
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    times, reused, good = [], [], False
    for _ in range(1 if args.profile else 7):
        gc.collect()
        began = time.perf_counter()
        with tracker:
            heights, model, note = candidate(layout, upper, footprint)
        times.append(1000 * (time.perf_counter() - began))
        good, same = admit(layout, ground, upper, checked, heights, model, note)
        example = heights
        reused.append(same)
        del heights, model
        if not good:
            break
    result = {
        "commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "plan_sha256": digest(args.products / "terrain-plan.npz"),
        "mode": args.mode,
        "phase_ms": times,
        "median_ms": statistics.median(times) if good else None,
        "actual_field_valid": good,
        "warm_field_valid": warm,
        "whole_body_proof_reused_by_exact_height_identity": reused,
        "independently_checked_height_fields": len(checked),
        "work": note,
        "profiled": args.profile is not None,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Same fixed source/mesh/objective/height interval/class/export bounds and 8s/100-iteration solver limit. Matrix preparation plus solve after imports/decode and one warm fit; field and whole-body validation outside timing. Exact-height identity only reuses an independently checked body. No native/device/frame or global-optimality claim. Internal copy totals unobserved.",
    }
    if example is not None:
        target = args.output.with_suffix(".npz")
        np.savez_compressed(target, heights=example)
        result["example_field_sha256"] = digest(target)
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({k: result[k] for k in ("mode", "actual_field_valid", "median_ms")})
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--mode", choices=("selective", "all"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(parser.parse_args())

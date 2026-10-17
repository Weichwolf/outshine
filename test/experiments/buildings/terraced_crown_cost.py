"""Fresh whole-owner feature/demand costs, separate from independent checking/rendering."""

import argparse
import gc
import json
import resource
import time
from collections.abc import Mapping
from dataclasses import fields, is_dataclass
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan as terrace_plan


def payload(state):
    stack, seen, intervals = [state], set(), []
    while stack:
        value = stack.pop()
        if id(value) in seen:
            continue
        seen.add(id(value))
        if isinstance(value, np.ndarray):
            while isinstance(value.base, np.ndarray):
                value = value.base
            intervals.append(np.lib.array_utils.byte_bounds(value))
        elif isinstance(value, Mapping):
            stack.extend(value.values())
        elif isinstance(value, (tuple, list)):
            stack.extend(value)
        elif is_dataclass(value) and not isinstance(value, type):
            stack.extend(getattr(value, f.name) for f in fields(value))
        elif type(value).__module__ in (
            "terraced_crown_geometry",
            "terraced_terrace_geometry",
            "terraced_facade_products",
        ):
            stack.extend(vars(value).values())
    size, end = 0, 0
    for low, high in sorted(intervals):
        size += max(0, high - max(end, low))
        end = max(end, high)
    return {
        "referenced_numpy_bytes": int(size),
        "numpy_storage_intervals": len(set(intervals)),
    }


def build(row, kind, mode):
    if mode == "before":
        owner = TerraceProducts(terrace_plan(mass_plan(row, kind)))
        return owner, (owner.draw(True),), {"mode": "old whole near"}
    owner = RoofedProducts(plan(row, kind))
    if mode in ("full", "compact"):
        return owner, owner.parts(mode == "full", mode == "full"), {"mode": mode}
    radius = owner.plan.source_top_m - owner.plan.body.top_m
    distance = 1212 * np.sqrt(0.35 * radius) / 0.5
    target = np.r_[row["polygon"].centroid.coords[0], owner.plan.source_top_m / 2]
    view = View(
        target + [0, -distance, distance * 0.2],
        target,
        width=1920,
        height=1080,
        fov_deg=48,
    )
    parts, decision = owner.resolve(view)
    if decision["mode"] != "mixed":
        raise ValueError(
            "Measured demand control did not request roof-only near detail"
        )
    return owner, parts, decision


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=int, choices=range(3), required=True)
    parser.add_argument(
        "--mode", choices=("before", "full", "mixed", "compact"), required=True
    )
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the cost producer first")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    row, kind = cases()[args.source]
    samples, state = [], None
    for _ in range(3):
        state = None
        gc.collect()
        start, cpu = time.perf_counter(), time.process_time()
        state = build(row, kind, args.mode)
        samples.append(
            {"wall_s": time.perf_counter() - start, "cpu_s": time.process_time() - cpu}
        )
    result = payload(state)
    result["draw_triangles"] = sum(len(p["faces"]) for p in state[1])
    result["decision"] = state[2]
    owner = state[0]
    body = owner.body if isinstance(owner, RoofedProducts) else owner
    result["closed_body_built"] = body.facade.near is not None
    result["closed_roof_components"] = (
        len(owner.roof_components or ()) if isinstance(owner, RoofedProducts) else 0
    )
    del owner, body
    if args.trace:
        state = None
        gc.collect()
        import memray

        trace = args.output.with_suffix(".bin")
        with memray.Tracker(trace, trace_python_allocators=True, native_traces=False):
            state = build(row, kind, args.mode)
        if payload(state) != {
            k: result[k] for k in ("referenced_numpy_bytes", "numpy_storage_intervals")
        }:
            raise ValueError("Profiled replay changed retained NumPy storage")
        result["trace"] = str(trace)
    result |= proof | {
        "source_id": row["id"],
        "programme": kind,
        "mode": args.mode,
        "unprofiled_runs": samples,
        "worker_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Source/imports excluded; whole 2D planning, coarse products, requested closed parts and cached draw parts included. Before/full differ in roof quality and coupled storey geometry. Mixed/full share the same logical plan and unchanged position-only demand threshold, not a general radiance/shadow/LOD admission. NumPy union of referenced root byte intervals excludes Python/GEOS/kernel heap and is not RSS; separate Memray tracks requests/heap, not exact copies or device/frame cost.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: result[k] for k in ("mode", "draw_triangles", "referenced_numpy_bytes")}
        )
    )


if __name__ == "__main__":
    main()

"""Same full programme with retained-reference versus early-omitted legacy trim."""

import argparse
import gc
import json
import resource
import time
from contextlib import nullcontext
from pathlib import Path
from unittest.mock import patch

import terraced_facade_details as details
import terraced_facade_surface as surface
from evidence import revision
from facade_programme_plan import PROGRAMMES, plan_programme
from facade_programme_products import programme_products
from terraced_city_parts import fingerprint
from terraced_crown_cost import payload
from terraced_crown_plan import plan
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terraces


def construct(crown, planned, method, near):
    owner = programme_products(crown, planned, omit_trim=method == "early")
    if near:
        owner.body.facade.physical()
    mesh = owner.draw(body_near=near, roof_near=near)
    return owner, mesh


def identity(state):
    owner, mesh = state
    return {
        "draw_arrays_sha256": fingerprint(mesh),
        "closed_frames_sha256": fingerprint(owner.body.facade.near.frames)
        if owner.body.facade.near is not None
        else None,
    }


def work_replay(call):
    counts = {
        "closed_trim_calls": 0,
        "closed_trim_vertices": 0,
        "closed_trim_triangles": 0,
        "compact_trim_calls": 0,
        "compact_trim_quads": 0,
    }
    solid, accents = details.solid, surface.accents

    def counted_solid(storage, polygon, low, high, material):
        points, faces = len(storage[0]), len(storage[1])
        result = solid(storage, polygon, low, high, material)
        if material == 9:
            counts["closed_trim_calls"] += 1
            counts["closed_trim_vertices"] += len(storage[0]) - points
            counts["closed_trim_triangles"] += len(storage[1]) - faces
        return result

    def counted_accents(writer, *args, **kwargs):
        before = len(writer.quads)
        result = accents(writer, *args, **kwargs)
        counts["compact_trim_calls"] += 1
        counts["compact_trim_quads"] += len(writer.quads) - before
        return result

    with (
        patch.object(details, "solid", counted_solid),
        patch.object(surface, "accents", counted_accents),
    ):
        state = call()
    return identity(state), counts


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the facade omission cost producer first")
    paths = [args.output] + ([args.trace] if args.trace else [])
    if any(p.exists() or not p.resolve().is_relative_to(Path("/tmp")) for p in paths):
        raise ValueError("Fresh explicit private outputs required")
    row = sources()[args.source]
    kind = "cascade" if args.source == 0 else "paired-wings"
    crown = plan(row, kind)
    terrace = terraces(crown.body)
    planned = plan_programme(terrace.prepared, args.programme, portals=terrace.portals)
    if not planned.cells:
        raise ValueError("Cost comparison requires an actual complete programme")
    near = args.mode == "near"

    def call():
        return construct(crown, planned, args.method, near)

    state, times = None, []
    for _index in range(args.repetitions):
        del state
        gc.collect()
        cpu, began = time.process_time(), time.perf_counter()
        state = call()
        times.append(
            {
                "wall_seconds": time.perf_counter() - began,
                "process_cpu_seconds": time.process_time() - cpu,
            }
        )
    expected = identity(state)
    owner, mesh = state
    retained = payload(
        (owner.source, owner.body.facade.base, owner.body.facade.near, mesh)
    )
    triangles = len(mesh["faces"])
    del state, owner, mesh
    gc.collect()
    if args.trace:
        import memray

        tracker = memray.Tracker(
            str(args.trace), trace_python_allocators=True, native_traces=False
        )
    else:
        tracker = nullcontext()
    if args.trace:
        with tracker:
            state = call()
        if identity(state) != expected:
            raise ValueError("Allocation replay changed the complete output")
        del state
        gc.collect()
    actual, counts = work_replay(call)
    if actual != expected:
        raise ValueError("Counter replay changed the complete output")
    if args.method == "early" and any(counts.values()):
        raise ValueError("Early construction still builds old decorative trim")
    result = proof | {
        "source_id": row["id"],
        "programme": args.programme,
        "mode": args.mode,
        "method": args.method,
        "identity": expected,
        "unprofiled_runs": times,
        "retained": retained,
        "draw_triangles": triangles,
        "work_counter_replay": counts,
        "trace": str(args.trace) if args.trace else None,
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Same fixed source/roof/programme plan, imports/acquisition/planning excluded. Fresh coarse owner plus requested complete closed collision and draw included. Normal wall/CPU observations precede separate allocation/counter replays. NumPy referenced storage is not RSS/heap. Legacy paired-pier polygon counts still reserve old opening part IDs without extrusion. Internal GEOS/kernel copies, GPU/frame/native budgets unmeasured.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({"method": args.method, "mode": args.mode, **expected}), flush=True
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=int, choices=(0, 3), required=True)
    parser.add_argument("--programme", choices=PROGRAMMES, required=True)
    parser.add_argument("--method", choices=("reference", "early"), required=True)
    parser.add_argument("--mode", choices=("near", "compact"), required=True)
    parser.add_argument("--repetitions", type=int, choices=range(1, 17), default=3)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

"""Equal nineteen-source facet budgets: bounded dual simplex versus interior point."""

import argparse
import contextlib
import gc
import json
import resource
import statistics
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from rail_instance_cost import profile_note
from stair_cycle_contacts import contacts
from stair_cycle_inputs import load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_junction_verify import landing_proof
from stair_network_geometry import interactions, vertex_links
from stair_terrain_experiment import frozen


def checked(plan, rows, nodes, heights, reference):
    if heights is None:
        return False, False, "no solver output"
    if np.array_equal(heights, reference):
        return True, True, "byte-identical verified reference heights"
    try:
        _body, data, _geometry = build(plan, heights)
        pairs, _whole = interactions(rows, nodes)
        audit(
            data, plan["domains"], limits=[r["gradient"] for r in rows], crossings=pairs
        )
        vertex_links(data["body_vertices"], data["body_faces"])
        contacts(data, rows)
        fields = [Mesh(np.c_[plan["xy"], heights], faces) for faces in plan["parts"]]
        landing_proof(
            fields, rows, plan["domains"], nodes, [s[0] for s in plan["stairs"]]
        )
    except ValueError as error:
        return False, False, str(error)
    return True, False, "new whole candidate checked outside timing"


def measure(args):
    commit = frozen()
    if args.output.exists() or (args.profile is not None and args.profile.exists()):
        raise ValueError("fresh explicit cycle cost outputs required")
    report = json.loads((args.products / "comparison.json").read_text())
    verified = json.loads((args.products / "verification.json").read_text())
    target = args.products / "after/height-input.npz"
    if (
        not verified["valid_local_candidate"]
        or verified["products_report_sha256"]
        != digest(args.products / "comparison.json")
        or report["after"]["height_input_sha256"] != digest(target)
        or report["after"]["body_sha256"] != digest(args.products / "after/body.npz")
    ):
        raise ValueError("verified cycle height/body inputs required")
    _case, rows, nodes, stairs, _context, _sample = load()
    plan = layout(rows, "width", junctions=nodes, stairs=stairs)
    reference = dict(np.load(target))["heights"]
    warm, _note = solve(plan, solver=args.solver)
    warm_good, _same, _reason = checked(plan, rows, nodes, warm, reference)
    del warm
    tracker = contextlib.nullcontext()
    if args.profile is not None:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    times, identity, notes, good = [], [], [], False
    for _ in range(1 if args.profile else 7):
        gc.collect()
        began = time.perf_counter()
        with tracker:
            heights, note = solve(plan, solver=args.solver)
        times.append(1000 * (time.perf_counter() - began))
        good, same, reason = checked(plan, rows, nodes, heights, reference)
        identity.append(same)
        notes.append({**note, "actual_body_valid": good, "validation_reason": reason})
        if not good:
            break
    result = {
        "commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "solver": args.solver,
        "phase_ms": times,
        "median_ms": statistics.median(times) if good else None,
        "actual_body_valid": good,
        "warm_output_valid": warm_good,
        "verified_reference_identity": identity,
        "runs": notes,
        "output_height_bytes": heights.nbytes if good else 0,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "profiled": args.profile is not None,
        "scope": "Same fixed full mesh, class/contact/clearance/precision bounds and 5s solver limit. Matrix preparation plus solve timed, after imports/decode/layout and one warm solve. Whole body checks outside timing; identical heights reuse verified proof. No terrain/native/frame/device speed factor. Unknown solver-internal copies remain unmeasured.",
    }
    if args.profile is not None:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("actual_body_valid", "median_ms")}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--solver", choices=("highs-ds", "highs-ipm"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(parser.parse_args())

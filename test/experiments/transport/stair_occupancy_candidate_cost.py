"""Same complete queries: eager, numeric flat, product bounds and bounded facet reuse."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

from rail_instance_cost import profile_note
from stair_local_replay import load_chain
from stair_occupancy_candidate_experiment import query_inputs
from stair_occupancy_candidate_reuse import retained
from stair_occupancy_candidate_sources import from_state
from stair_occupancy_candidates import CandidateQuery
from stair_occupancy_experiment import prepare_volumes
from stair_occupancy_geometry import AffineOccupancy
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import actual_sources
from stair_terrain_experiment import frozen


def prepare(state, sources, method):
    if method == "eager":
        _view, assets, supports, contacts, manifest = state
        rows, *_ = prepare_volumes(assets, supports, contacts, sources, manifest, ())
        return OccupancyQuery(rows), None
    catalog = from_state(state, sources)
    info = None
    if method == "retained":
        catalog, info = retained(catalog, 8192)
    return CandidateQuery(
        catalog, method="flat" if method == "flat" else "coarse"
    ), info


def measured(state, sources, queries, method):
    calls = [0]
    original = AffineOccupancy.__post_init__

    def counted(row):
        calls[0] += 1
        original(row)

    with patch.object(AffineOccupancy, "__post_init__", counted):
        start = time.perf_counter()
        query, info = prepare(state, sources, method)
        prepared = time.perf_counter()
        cuts, work = [], []
        for footprint in queries:
            cut, note = query.conflict(footprint)
            cuts.append(cut)
            work.append(note)
        finished = time.perf_counter()
    return {
        "preparation_seconds": prepared - start,
        "query_seconds": finished - prepared,
        "total_seconds": finished - start,
        "affine_constructor_calls": calls[0],
        "query_count": len(queries),
        "face_tests": sum(
            w.get("metadata_face_tests", w.get("face_tests", 0)) for w in work
        ),
        "retained": info()._asdict() if info else None,
        "conflict_wkb_sha256": hashlib.sha256(
            b"".join(c.wkb for c in cuts)
        ).hexdigest(),
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
    }


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private cost output requires /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    state = load_chain(args.resident, args.prior_module)
    context, sources, *_ = actual_sources()
    queries, omitted = query_inputs(state, args.dem, context)
    profile = args.output / "allocations.bin"
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(profile), trace_python_allocators=True, native_traces=False
        )
    with tracker:
        result = measured(state, sources, queries, args.method)
    allocation = profile_note(profile) if args.profile else None
    if allocation:
        allocation["native_traces"] = False
        allocation["scope"] = (
            "Metadata/preparation and complete queries after input decode/imports; constructor-counter bookkeeping included; cumulative allocations are not live or copied bytes."
        )
    report = {
        "commit": commit,
        "method": args.method,
        "sources": len(state[4]["source_ids"]),
        "normal_priority": not args.profile,
        "probe_omissions": omitted,
        "result": result,
        "allocations": allocation,
        "scope": "One fresh process; shared host load; imports, state/source decode and DEM/query preparation excluded; RSS includes those earlier phases. No native/A18/GPU admission.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resident", type=Path)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--dem", type=Path, required=True)
    parser.add_argument(
        "--method", choices=("eager", "flat", "coarse", "retained"), required=True
    )
    parser.add_argument("--profile", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (600, 600))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())

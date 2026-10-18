"""Whole immutable-volume and height-query comparison with bounded pre-plane demand."""

import argparse
import json
import resource
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from stair_local_replay import load_chain
from stair_occupancy_candidate_reference import PartitionReference
from stair_occupancy_candidate_sources import from_state
from stair_occupancy_candidates import CandidateQuery
from stair_occupancy_geometry import AffineOccupancy
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import actual_sources, buildings, full_tile_sampler
from stair_occupancy_pipeline import probes
from stair_terrain_experiment import frozen


def query_inputs(state, dem, context):
    sample = full_tile_sampler(dem, context)
    originals, _ledger, _receipt = buildings(context, sample)
    controls, omitted = probes(state[1], sample)
    return (*originals, *controls), omitted


def whole_records(catalog, reference):
    seen = {}
    for source in catalog.sources:
        for index in range(len(source.bounds)):
            for row, keys, _error in source.materialize(index):
                if (row, keys) != reference.by_id[row.owner_id]:
                    raise ValueError("whole original volume/source binding changed")
                if row.owner_id in seen:
                    raise ValueError("duplicate original volume identity")
                seen[row.owner_id] = row
    if set(seen) != set(reference.by_id):
        raise ValueError("whole raw-facet catalog omitted an original volume")
    return len(seen)


def compare(catalog, reference, queries, method):
    work, candidates = [], 0
    query = CandidateQuery(catalog, method=method)
    for footprint in queries:
        result = catalog.query(catalog.frame, footprint, method=method)
        if not result.complete:
            raise ValueError("actual complete query deferred: " + result.reason)
        for row, keys in zip(result.volumes, result.source_ids, strict=True):
            if (row, keys) != reference.by_id[row.owner_id]:
                raise ValueError("candidate changed an original record or source")
        actual, note = query.conflict(footprint)
        expected, _note = reference.conflict(footprint)
        if not actual.equals(expected):
            raise ValueError("whole independently evaluated conflict geometry changed")
        work.append(note)
        candidates = max(candidates, result.work["raw_candidates"])
    return {
        "queries": len(work),
        "maximum_raw_candidates": candidates,
        "face_tests": sum(
            w.get("metadata_face_tests", w.get("face_tests", 0)) for w in work
        ),
        "materializer_calls": sum(
            w.get("metadata_materializer_calls", w.get("materializer_calls", 0))
            for w in work
        ),
    }


def overflow(catalog, reference):
    boxes = np.concatenate([s.bounds for s in catalog.sources])
    lo, hi = boxes[:, :3].min(0) - 1, boxes[:, 3:].max(0) + 1
    footprint = BuildingFootprint(
        "whole-query-countercase",
        shapely.box(lo[0], lo[1], hi[0], hi[1]),
        HeightInterval(lo[2], hi[2]),
    )
    result = catalog.query(catalog.frame, footprint)
    if len(reference.rows) > 8192:
        if result.complete or result.work["materializer_calls"]:
            raise ValueError("oversize query built volumes or exposed free space")
        try:
            OccupancyQuery(reference.rows)
        except ValueError:
            pass
        else:
            raise ValueError("old flat limit was changed")
    return {"complete": result.complete, "reason": result.reason, "work": result.work}


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private experiment output requires /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    state = load_chain(args.resident, args.prior_module)
    context, sources, *_ = actual_sources()
    reference = PartitionReference(args.baseline, state, sources)
    constructor = AffineOccupancy.__post_init__
    calls = []

    def counted(row):
        calls.append(1)
        constructor(row)

    with patch.object(AffineOccupancy, "__post_init__", counted):
        catalog = from_state(state, sources)
    if calls:
        raise ValueError("metadata preparation constructed affine volumes")
    count = whole_records(catalog, reference)
    queries, omitted = query_inputs(state, args.dem, context)
    result = {
        "commit": commit,
        "source_count": len(state[4]["source_ids"]),
        "whole_identical_records": count,
        "products": len(catalog.sources),
        "metadata_array_bytes": sum(s.bounds.nbytes for s in catalog.sources),
        "query_probe_omissions": omitted,
        "methods": {
            m: compare(catalog, reference, queries, m) for m in ("flat", "coarse")
        },
        "whole_query": overflow(catalog, reference),
        "valid_whole_immutable_records_and_queries": True,
        "scope": "Experiment only; independently verified baseline plus unchanged-builder new road partitions; no shared Plan migration, consumer or native admission.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resident", type=Path)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--dem", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (600, 600))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())

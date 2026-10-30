"""Actual Buildings producer and independent saved-body reader on candidate demand."""

import argparse
import json
import resource
from pathlib import Path

from camera_height_inputs import digest
from stair_consumer_buildings import buildings
from stair_consumer_experiment import save, save_meshes
from stair_consumer_inputs import candidate_inputs as data_for
from stair_consumer_verify import check_bodies, meshes
from stair_local_replay import load_chain
from stair_occupancy_candidate_reference import PartitionReference
from stair_occupancy_candidate_reuse import retained
from stair_occupancy_candidate_sources import from_state
from stair_occupancy_candidates import CandidateQuery
from stair_occupancy_inputs import actual_sources
from stair_terrain_experiment import frozen


def generate(args, state, data, sources, *, catalog=None):
    catalog, info = retained(
        from_state(state, sources) if catalog is None else catalog, 8192
    )
    before, after, occupied, ledger, context = buildings(data, CandidateQuery(catalog))
    source_rows = {r["id"]: r for r in ledger if r.get("state") == "prepared"}
    reservations = [
        {
            "body_index": i,
            "owner_id": r.owner_id,
            "source_id": source_rows[r.owner_id]["source_id"],
            "source_part": source_rows[r.owner_id]["part"],
            "geometry_wkb": r.geometry.wkb_hex,
            "heights_m": [r.heights.lower_m, r.heights.upper_m],
            "purpose": r.purpose,
        }
        for i, r in enumerate(occupied)
    ]
    artifacts = {
        "buildings-before.npz": save_meshes(
            args.output, "buildings-before.npz", before
        ),
        "buildings-after.npz": save_meshes(args.output, "buildings-after.npz", after),
        "building-ledger.json": save(args.output, "building-ledger.json", ledger),
        "context.json": save(args.output, "context.json", context),
        "building-reservations.json": save(
            args.output, "building-reservations.json", reservations
        ),
    }
    report = {
        "commit": frozen(),
        "baseline": str(args.baseline.resolve()),
        "baseline_report_sha256": digest(args.baseline / "comparison.json"),
        "resident": str(args.resident.resolve()),
        "prior_modules": [str(p.resolve()) for p in args.prior_module],
        "source_ids": state[4]["source_ids"],
        "frame": data["frame"].identifier,
        "actual_building_bodies": len(after),
        "artifacts": artifacts,
        "retained_facets": info()._asdict(),
        "scope": "Actual unchanged Buildings recipes, current full terrain, bounded experimental query adapter. Shared Plan and native runtime unchanged. Plants require a separate actual consumer proof.",
    }
    save(args.output, "comparison.json", report)
    return report


def verify(args):
    report = json.loads((args.products / "comparison.json").read_text())
    baseline = Path(report["baseline"])
    if digest(baseline / "comparison.json") != report["baseline_report_sha256"]:
        raise ValueError("verified source baseline changed")
    for name, expected in report["artifacts"].items():
        if digest(args.products / name) != expected:
            raise ValueError("complete saved consumer artifact changed: " + name)
    state = load_chain(
        Path(report["resident"]), [Path(p) for p in report["prior_modules"]]
    )
    if state[4]["source_ids"] != report["source_ids"]:
        raise ValueError("whole current transport source ownership changed")
    data = data_for(baseline, state)
    _context, sources, *_ = actual_sources()
    checked = check_bodies(
        data,
        json.loads((args.products / "building-ledger.json").read_text()),
        meshes(args.products / "buildings-before.npz"),
        meshes(args.products / "buildings-after.npz"),
        json.loads((args.products / "building-reservations.json").read_text()),
        PartitionReference(baseline, state, sources),
    )
    return {
        "reader_commit": frozen(),
        "producer_commit": report["commit"],
        "report_sha256": digest(args.products / "comparison.json"),
        **checked,
        "scope": "Fresh independent unchanged-volume full scans in bounded partitions; real saved bodies, original source attributes and current roots. Not plant/shared Plan/native admission.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resident", type=Path, nargs="?")
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (600, 600))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh explicit private /tmp output required")
    frozen()
    if args.products:
        result = verify(args)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    else:
        if not args.resident or not args.baseline:
            parser.error("generation requires resident and --baseline")
        args.output.mkdir(parents=True, exist_ok=False)
        state = load_chain(args.resident, args.prior_module)
        _context, sources, *_ = actual_sources()
        result = generate(args, state, data_for(args.baseline, state), sources)
    print(json.dumps(result, indent=2))

"""Actual prepared-state queries and new generation, followed by the strict cold reader."""

import argparse
import json
import resource
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_coverage_boundary_scope import validation
from stair_local_generate import run as generate
from stair_local_replay import load_chain
from stair_local_verify import verify as cold_verify
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_experiment import compare
from stair_patch_inputs import fresh
from stair_patch_verify import compare_queries
from stair_resident_snapshot_cost import state_hash
from stair_resident_snapshot_verify import equal_patches
from stair_terrain_experiment import frozen


def verify(args):
    commit = frozen()
    fresh(args.output)
    with validation("general", []):
        strict = load_chain(args.resident)
    records = []
    with validation(args.coverage, records):
        actual = load_chain(args.resident)
    expected_hash = state_hash(strict)
    if state_hash(actual) != expected_hash:
        raise ValueError("boundary coverage changed complete prepared state")
    equal_patches(actual[0], strict[0])
    equality = compare(
        actual[0],
        read_arrays(args.resident / "world-layout.npz"),
        read_arrays(args.resident / "world-field.npz"),
    )
    world = actual[0].query(actual[0].patches[0].bounds_xy_m, max_faces=65536)
    if world.mesh is None:
        raise ValueError("complete prepared terrain query failed")
    queries = compare_queries(actual[0], world.mesh)
    options = SimpleNamespace(
        resident=args.resident,
        fixture=args.fixture,
        output=args.output,
        prior_module=[],
        max_context_faces=8192,
    )
    with validation(args.coverage, records):
        report = generate(options)
    if not report["valid_local_module"]:
        raise ValueError("actual next whole source no longer constructs")
    original = json.loads((args.reference / "comparison.json").read_text())
    if (
        original["resident_sha256"] != report["resident_sha256"]
        or original["fixture_sha256"] != report["fixture_sha256"]
        or original["candidate_source_ids"] != report["candidate_source_ids"]
        or original["artifacts"] != report["artifacts"]
    ):
        raise ValueError(
            "boundary coverage changed actual complete generation products"
        )
    for name, expected in original["artifacts"].items():
        if digest(args.reference / name) != expected:
            raise ValueError("original complete generation artifact changed")
    with validation("general", []):
        cold_verify(options)
    receipt = json.loads((args.output / "verification.json").read_text())
    if not receipt["valid_local_module"]:
        raise ValueError("strict independent cold reader refused next generation")
    result = {
        "reader_commit": commit,
        "coverage": args.coverage,
        "resident_sha256": digest(args.resident / "resident.json"),
        "original_report_sha256": digest(args.reference / "comparison.json"),
        "state_sha256": expected_hash,
        "same_patch_ids_arrays_masks_attributes": True,
        "whole_reference": equality,
        "queries": queries,
        "coverage_calls": records,
        "source_count": len(report["candidate_source_ids"]),
        "same_next_generation_artifacts": report["artifacts"],
        "cold_receipt_sha256": digest(args.output / "verification.json"),
        "valid_actual_boundary_replay": True,
        "scope": "Complete real prepared state, index/scan and D/F32 sampling, "
        "new generation with experimental coverage followed by unchanged strict verification. "
        "Original artifact hashes include full road/terrain/planning/source products. "
        "No native/device admission or new source/visual improvement.",
    }
    write_json(args.output / "coverage-verification.json", result)
    print(
        json.dumps({k: v for k, v in result.items() if k != "coverage_calls"}),
        flush=True,
    )
    return result


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--coverage", choices=("mesh", "mesh-batched", "library"), default="library"
    )
    for name in ("resident", "reference", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    verify(parser.parse_args())

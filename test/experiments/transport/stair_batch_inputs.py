"""Bounded sequential seed research inputs and exact recipe identities."""

import argparse
import hashlib
import json
import math
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_local_replay import verified
from stair_module_inputs import FIXTURE


def identity(value):
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def candidate(selected, tried):
    for index, recipe in enumerate(selected["recipes"]):
        if identity(recipe) not in tried:
            return index, recipe
    return None


def namespace(config):
    result = dict(config)
    for key in ("resident", "reference", "prepared_reference"):
        if result.get(key) is not None:
            result[key] = Path(result[key])
    result["prior_module"] = [Path(p) for p in result["prior_module"]]
    return SimpleNamespace(**result)


def config_for(args, prior):
    return {
        "resident": str(args.resident.resolve()),
        "prior_module": [str(p.resolve()) for p in prior],
        "reference": str(args.reference.resolve()),
        "method": args.method,
        "strategy": "prefix",
        "schedule": "roots_first",
        "family": "seed",
        "seed_root_form": "all",
        "seed_grid_m": 2**-11,
        "seed_crossings": "walk",
        "prepared_reference": (
            str(args.prepared_reference.resolve()) if args.prepared_reference else None
        ),
        "reference_sha256": args.reference_sha256,
        "prepared_variant": args.prepared_variant,
    }


def prefix(args):
    if len(args.prior_module) > 24 or len(set(args.prior_module)) != len(
        args.prior_module
    ):
        raise ValueError("at most 24 unique prior modules leave eight bounded slots")
    if args.prior_module:
        path = args.prior_module[-1]
        verified(path)
        report = json.loads((path / "comparison.json").read_text())
        keys = report["candidate_source_ids"]
    else:
        keys = json.loads((args.resident / "resident.json").read_text())["source_ids"]
    return list(keys), digest(args.resident / "resident.json")


def limits(args):
    if type(args.max_attempts) is not int or not 1 <= args.max_attempts <= 8:
        raise ValueError("one to eight isolated attempts required")
    for value in (args.seconds, args.attempt_seconds):
        if isinstance(value, bool) or not math.isfinite(value) or not 0 < value <= 600:
            raise ValueError("positive finite wall budget at most 600 seconds required")
    if bool(args.prepared_reference) != bool(args.reference_sha256):
        raise ValueError(
            "prepared reference and external pin must be supplied together"
        )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verification-output", type=Path)
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    parser.add_argument("--max-attempts", type=int, default=8)
    parser.add_argument("--seconds", type=float, default=300)
    parser.add_argument("--attempt-seconds", type=float, default=60)
    parser.add_argument("--prepared-reference", type=Path)
    parser.add_argument("--reference-sha256")
    parser.add_argument(
        "--prepared-variant", choices=("stored", "compressed"), default="stored"
    )
    return parser.parse_args()

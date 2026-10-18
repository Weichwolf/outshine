"""Measure complete prepared loads with only the coverage composition changed."""

import argparse
import json
import resource
from pathlib import Path

from stair_composition_scope import composition
from stair_module_experiment import write_json
from stair_resident_snapshot_cost import run


def measure(args):
    work_output = args.output.with_name(args.output.stem + "-composition.json")
    if work_output.exists():
        raise ValueError("fresh separate composition work report required")
    records = []
    with composition(args.method, records):
        result = run(
            argparse.Namespace(
                products=args.products,
                output=args.output,
                profile=args.profile,
                mode="snapshot",
            )
        )
    report = {
        "commit": result["commit"],
        "method": args.method,
        "state_sha256": result["state_sha256"],
        "regions": records,
        "region_pairs": len(records),
        "selected_triangles": sum(r["before"] + r["after"] for r in records),
        "scope": "Actual complete load including roads/profile metadata, patches/masks, "
        "whole supports/caps and logical headroom. Only the already-validated-region "
        "equality predicate changes; existing constructors, XYZ boundary checks, removals "
        "and immutable state updates remain. No native/cache/device admission.",
    }
    write_json(work_output, report)
    print(json.dumps({k: v for k, v in report.items() if k != "regions"}), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument(
        "--method", choices=("general", "coverage", "oriented"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    measure(parser.parse_args())

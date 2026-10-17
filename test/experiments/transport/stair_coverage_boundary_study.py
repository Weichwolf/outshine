"""Compare full prepared loads with general or boundary-based coverage validation."""

import argparse
import json
import resource
from pathlib import Path

from stair_coverage_boundary_scope import validation
from stair_module_experiment import write_json
from stair_resident_snapshot_cost import run


def measure(args):
    records = []
    coverage_output = args.output.with_name(args.output.stem + "-coverage.json")
    if coverage_output.exists():
        raise ValueError("fresh separate coverage-work output required")

    with validation(args.coverage, records):
        result = run(
            argparse.Namespace(
                products=args.products,
                output=args.output,
                profile=args.profile,
                mode="snapshot",
            )
        )
    summary = {
        "commit": result["commit"],
        "coverage": args.coverage,
        "state_sha256": result["state_sha256"],
        "calls": records,
        "proved_patches": sum(r["proved"] for r in records),
        "general_calls": sum(not r["proved"] for r in records),
        "input_polygons": sum(r["input_polygons"] for r in records),
        "general_polygons": sum(
            r["input_polygons"] for r in records if not r["proved"]
        ),
        "scope": "Actual complete loading, including roads/profiles, terrain/masks, supports/caps, "
        "and logical headroom. Only the coverage predicate changes in this isolated process. "
        "Polygon adapter reconstructs XY/index arrays; mesh alternative reuses the owned mesh. No input trust flag, "
        "persistent proof, constructor bypass, changed geometry or lib/native API. "
        "General coverage remains the fallback. Instrumented host results are not device budgets.",
    }
    write_json(coverage_output, summary)
    print(
        json.dumps({k: v for k, v in summary.items() if k not in ("calls", "scope")}),
        flush=True,
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument(
        "--coverage", choices=("general", "boundary", "mesh"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    measure(parser.parse_args())

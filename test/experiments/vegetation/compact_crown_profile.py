"""Equal cold whole-crown output for whole-process heap/traffic profiling."""

import argparse
import json
from pathlib import Path

import numpy as np
from compact_crown import CompactCrown
from compact_crown_probe import output_digest
from crown_demand import CrownDemand
from crown_demand_probe import cameras
from spatial_group_source import load_source


def run(inputs, method):
    source = load_source(inputs)
    if method == "original":
        plan = CrownDemand(source)
        _, result = plan.prepare((cameras()["whole"],), max_leaves=100000)
        persistent = plan.lower.nbytes + plan.upper.nbytes
    else:
        product = CompactCrown.build(source)
        selected = cameras()["whole"].visible(
            product.data["lower"], product.data["upper"]
        )
        result = product.evaluate(selected, max_leaves=100000)
        persistent = sum(v.nbytes for v in product.data.values())
    assert len(result["ids"]) == 90000
    print(
        json.dumps(
            {
                "method": method,
                "source_identity": source.identity,
                "output_sha256": output_digest(result),
                "final_output_array_bytes": sum(v.nbytes for v in result.values()),
                "persistent_product_array_bytes": persistent,
                "numpy": np.__version__,
                "scope": "Whole-process profile includes same imports, cold source decode and one complete build/query; compact also compiles saved continuous parameters. Not query-only heap counts, total system traffic or uninstrumented time.",
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--method", choices=("original", "compact"), required=True)
    args = parser.parse_args()
    run(args.inputs, args.method)

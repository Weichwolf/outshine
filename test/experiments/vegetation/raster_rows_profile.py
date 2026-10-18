"""Single equal-query process for external allocation profilers, not frame timing."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from grass_projection_instances import instanced_contacts
from grass_projection_reader import read_product


def run(inputs, sampler):
    report, stand, _ = read_product(inputs)
    recipe = json.loads((inputs / "stand.json").read_text())["recipe"]
    compact = {k: v for k, v in stand.items() if k.startswith(("pose_", "form_"))}
    direction = np.asarray(
        next(r["direction"] for r in report["views"] if r["name"] == "east")
    )
    lower, upper = np.asarray(report["footprint_m"])
    result = instanced_contacts(
        recipe, compact, lower, upper, direction, 512, sampler=sampler
    )
    digest = hashlib.sha256()
    for key in ("face", "height", "normal", "coverage"):
        digest.update(result[key].tobytes())
    print(
        json.dumps(
            {
                "sampler": sampler,
                "result_sha256": digest.hexdigest(),
                "stand_identity": report["stand_identity"],
                "size": 512,
                "scope": "Entire process profiler includes imports/decode and one equal query; profile time is not an uninstrumented runtime cost",
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--sampler", choices=("boxes", "rows"), required=True)
    args = parser.parse_args()
    run(args.inputs, args.sampler)

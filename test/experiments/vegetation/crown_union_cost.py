"""Compare cached solid representations under the same complete-output checks."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from crown_union import join_sources
from crown_union_experiment import prepared


def run(inputs, plan, output):
    cases = {}
    for representation in ("limbs", "cells"):
        start = time.perf_counter()
        sources, _ = prepared(inputs, plan, representation)
        preparation = time.perf_counter() - start
        times, work = [], None
        for _ in range(3):
            start = time.perf_counter()
            try:
                mesh, work = join_sources(sources)
            except ValueError as error:
                cases[representation] = {
                    "accepted": False,
                    "reason": str(error),
                    "wall_s": time.perf_counter() - start,
                    "source_preparation_s": preparation,
                }
                break
            times.append(time.perf_counter() - start)
        else:
            cases[representation] = {
                "accepted": True,
                "wall_s": times,
                "median_s": float(np.median(times)),
                "source_preparation_s": preparation,
                "source_components": len(sources),
                "source_faces": sum(len(s["faces"]) for s in sources),
                "known_kernel_input_numpy_arrays": 3 * len(sources),
                "known_kernel_geometry_copy_bytes": work[
                    "kernel_input_geometry_copy_bytes"
                ],
                "known_kernel_face_id_write_bytes": 8
                * sum(len(s["faces"]) for s in sources),
                "output_array_bytes": sum(a.nbytes for a in mesh.values()),
                "removed_flat_fragments": len(work["discarded_flat_fragments"]),
            }
    report = {
        "cases": cases,
        "scope": "Same original sampled side surfaces and shoot overlap. Only representations passing all output checks admitted. Cell union defines the material as the union of local loft cells, with subprecision flat-shell cleanup; not a universal exact-output/robustness certificate. Known Python arrays/copies are lower bounds; kernel allocations, work and traffic unmeasured. Process CPU/RSS separately.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "plan", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.plan, args.output)

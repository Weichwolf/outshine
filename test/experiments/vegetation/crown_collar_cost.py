"""Same requested fork fields, exact sampler products, measured work and ray quality."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from crown_collar_accuracy import ray_accuracy
from crown_collars import demanded_collars


def run(inputs, products, output):
    recipe = json.loads((products / "comparison.json").read_text())["recipe"]
    cases, reference = {}, None
    for method in ("dense", "blocked"):
        times = []
        for _ in range(3):
            start = time.perf_counter()
            meshes, rows = demanded_collars(
                inputs,
                recipe["nodes"],
                recipe["cells"],
                method,
                recipe["strength"],
                recipe["samples"],
                recipe["curve_tolerance_m"],
            )
            times.append(time.perf_counter() - start)
            if reference is None:
                reference = meshes
            for before, after in zip(reference, meshes, strict=True):
                for key in before:
                    np.testing.assert_array_equal(before[key], after[key])
        cases[method] = {
            "wall_s": times,
            "median_s": float(np.median(times)),
            "field_points": sum(p["field_point_evaluations"] for p in rows),
            "interval_blocks": sum(p["interval_block_evaluations"] for p in rows),
            "volume_bytes": sum(p["volume_bytes"] for p in rows),
            "mask_bytes": sum(p["classification_mask_bytes"] for p in rows),
            "result_bytes": sum(v.nbytes for m in meshes for v in m.values()),
        }
    saved_plans = json.loads((products / "collar-plans.json").read_text())
    quality = []
    for mesh, plan in zip(reference, saved_plans, strict=True):
        with np.load(products / f"collar-{plan['fork_node']}.npz") as z:
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], z[key])
        quality.append({"fork_node": plan["fork_node"]} | ray_accuracy(mesh, plan))
    selected = min(cases, key=lambda key: cases[key]["median_s"])
    report = {
        "cases": cases,
        "selected": selected,
        "quality": quality,
        "scope": "Exact same local mesh arrays at equal field/resolution. Work counts distinguish point from block queries; internal allocations and full traffic unmeasured. Finite bracketed first-exit probes, no all-ray/global shape bound or native/runtime admission.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.products, args.output)

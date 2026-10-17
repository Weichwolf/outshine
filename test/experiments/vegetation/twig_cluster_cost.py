"""Equal-output individual/shared leaf placement and scalar/batched twig geometry."""

import argparse
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from tree_entry import load
from twig_cluster_experiment import read_plan
from twig_cluster_mesh import add_twigs


def basis_count(plan):
    keys = set()
    for host in plan.host_ids:
        twigs = plan.twigs(int(host), 32)
        keys.update(zip(map(int, twigs["counts"]), map(int, twigs["variants"])))
    return len(keys)


def compare(inputs):
    _, products = load(inputs / "recursive")
    reference_leaf, reference_wood = None, None
    rows = []
    for repeat in range(3):
        for reuse in (False, True) if repeat % 2 == 0 else (True, False):
            plan = read_plan(inputs)
            start = time.perf_counter()
            result = plan.tables(100000, reuse=reuse)
            elapsed = time.perf_counter() - start
            if reference_leaf is None:
                reference_leaf = result
            for key in result:
                np.testing.assert_array_equal(result[key], reference_leaf[key])
            rows.append(
                {
                    "kind": "leaf_groups",
                    "reuse": reuse,
                    "repeat": repeat,
                    "seconds": elapsed,
                    "shoot_placements": len(plan._prototypes)
                    if reuse
                    else plan.summary()["twigs"],
                    "returned_bytes": sum(a.nbytes for a in result.values()),
                    "retained_group_bytes": sum(
                        a.nbytes for d in plan._groups.values() for a in d.values()
                    ),
                    "prototype_bytes": sum(
                        a.nbytes for d in plan._prototypes.values() for a in d.values()
                    ),
                }
            )
        for method in (
            ("scalar", "batched") if repeat % 2 == 0 else ("batched", "scalar")
        ):
            plan = read_plan(inputs)
            start = time.perf_counter()
            result = add_twigs(plan, products["wood.npz"], 16384, method=method)
            elapsed = time.perf_counter() - start
            if reference_wood is None:
                reference_wood = result
            for key in ("faces", "corner_uv", "surface"):
                np.testing.assert_array_equal(result[key], reference_wood[key])
            np.testing.assert_allclose(
                result["vertices"], reference_wood["vertices"], atol=1e-12, rtol=0
            )
            np.testing.assert_allclose(
                result["normals"], reference_wood["normals"], atol=1e-10, rtol=0
            )
            rows.append(
                {
                    "kind": "wood",
                    "method": method,
                    "repeat": repeat,
                    "seconds": elapsed,
                    "axis_mesh_calls": plan.summary()["twigs"]
                    if method == "scalar"
                    else basis_count(plan),
                    "returned_bytes": sum(a.nbytes for a in result.values()),
                }
            )
    return rows


def allocations(inputs):
    result = []
    for reuse in (False, True):
        plan = read_plan(inputs)
        tracemalloc.start()
        rows = plan.tables(100000, reuse=reuse)
        current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        result.append(
            {
                "reuse": reuse,
                "traced_current_bytes": current,
                "traced_peak_bytes": peak,
                "traced_live_blocks": blocks,
                "result_bytes": sum(a.nbytes for a in rows.values()),
            }
        )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {
        "comparisons": compare(args.inputs),
        "allocations": allocations(args.inputs),
        "scope": "Same inputs/outputs, three alternating orders, comparisons outside timed intervals. Traced live blocks/peak are not cumulative calls or complete native allocation traffic; whole-process peak is separate.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)

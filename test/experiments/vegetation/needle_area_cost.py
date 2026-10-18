"""Compare analytical area counting with bounded enumeration of the same IDs."""

import argparse
import hashlib
import json
import resource
import statistics
import time
from contextlib import nullcontext
from pathlib import Path
from unittest.mock import patch

import numpy as np
from experiment_output import private_output
from mature_density import needle_areas
from outshine_ref.vegetation import NeedledPlant, PlantPlan


def enumerated(plant, areas):
    counts = np.zeros(len(areas), dtype="u8")
    orders, first = [], 0
    for count, pattern in zip(plant.needle_counts, plant.patterns):
        local = np.zeros(len(areas), dtype="u8")
        for start in range(first, first + count, 65536):
            ids = np.arange(min(65536, first + count - start), dtype="u8")
            ids += (start + plant.wood.seed) % len(areas)
            ids %= len(areas)
            local += np.bincount(ids.astype(np.intp), minlength=len(areas)).astype("u8")
        variance = pattern.scale_variation**2 / 3 if pattern is not None else 0
        orders.append(float(local @ areas) * (1 + variance))
        counts += local
        first += count
    return {
        "variant_counts": counts,
        "expected_area_by_order_m2": orders,
        "expected_area_m2": sum(orders),
    }


def run(args):
    output = private_output(args.output)
    if output.exists():
        raise ValueError("fresh private output required")
    output.mkdir(parents=True)
    areas, inputs = needle_areas(args.species, args.source)
    inputs[str(args.recipe)] = hashlib.sha256(args.recipe.read_bytes()).hexdigest()
    plant = NeedledPlant.from_recipe(json.loads(args.recipe.read_text()))
    expected = plant.area_summary(areas)
    procedure = (
        plant.area_summary
        if args.method == "analytical"
        else lambda a: enumerated(plant, a)
    )
    tracker = nullcontext()
    if args.track:
        import memray

        tracker = memray.Tracker(
            str(output / "allocations.bin"), trace_python_allocators=True
        )
    elapsed = []
    with patch.object(PlantPlan, "axis", side_effect=AssertionError("area axis")):
        with tracker:
            for _ in range(args.repeats):
                started = time.perf_counter()
                actual = procedure(areas)
                elapsed.append(time.perf_counter() - started)
        np.testing.assert_array_equal(
            actual["variant_counts"], expected["variant_counts"]
        )
        np.testing.assert_allclose(
            actual["expected_area_by_order_m2"],
            expected["expected_area_by_order_m2"],
            rtol=2e-15,
            atol=0,
        )
    if plant.wood._axes or plant._groups:
        raise ValueError("area counting placed geometry")
    total = sum(plant.needle_counts)
    report = {
        "method": args.method,
        "identity": plant.identity,
        "input_sha256": inputs,
        "repeats": args.repeats,
        "instrumented": args.track,
        "median_seconds": statistics.median(elapsed),
        "elapsed_seconds": elapsed,
        "orders": len(plant.patterns),
        "variants": len(areas),
        "represented_needles": total,
        "enumerated_ids_per_call": total if args.method == "enumerated" else 0,
        "explicit_cast_copied_bytes_per_call": total * 8
        if args.method == "enumerated"
        else 0,
        "maximum_id_batch": min(total, 65536) if args.method == "enumerated" else 0,
        "peak_process_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "expected_area_m2": float(actual["expected_area_m2"]),
        "scope": "Preloaded area query only. Cast bytes exclude other allocations, input decode and native/GPU work; use separate Memray stats for allocation totals.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--species", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("analytical", "enumerated"), required=True)
    parser.add_argument("--repeats", type=int, choices=range(1, 10001), default=5)
    parser.add_argument("--track", action="store_true")
    run(parser.parse_args())

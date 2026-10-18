"""Equal-quality positive-interior predicates on real queries and underflow controls."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import shapely
from camera_height_inputs import digest
from stair_occupancy_precision_cases import actual_cases
from stair_terrain_experiment import frozen


def pairs(products, reference):
    report = json.loads((reference / "comparison.json").read_text())
    ledger = reference / "cases.json"
    if (
        digest(ledger) != report["case_ledger_sha256"]
        or digest(products / "volumes.json") != report["saved_volume_sha256"]
    ):
        raise ValueError("complete independent same-model predicate reference required")
    expected = {
        r["case"]: r["footprint_positive_conflict"]
        for r in json.loads(ledger.read_text())
        if r["kind"] == "actual_height_query"
    }
    actual, _count, _queries = actual_cases(products, facet_limit=0)
    if {c[0] for c in actual} != expected.keys():
        raise ValueError(
            "predicate comparison does not cover all actual height queries"
        )
    result = []
    for name, _kind, row, heights, footprint, _cells in actual:
        if row.overlaps(footprint) != expected[name]:
            raise ValueError(
                "public predicate differs from independent actual-volume reference"
            )
        result.append(
            (row.conflict_geometry(heights), footprint.geometry, expected[name])
        )
    for c in report["source_area_underflow_control"]["positive_conflict_cases"]:
        result.append(
            (
                shapely.from_wkb(c["domain_wkb"]),
                shapely.from_wkb(c["footprint_wkb"]),
                True,
            )
        )
    square = shapely.box(0, 0, 1, 1)
    result.extend(
        (
            (square, shapely.box(1, 0, 2, 1), False),
            (square, shapely.box(1, 1, 2, 2), False),
            (shapely.Polygon(), square, False),
        )
    )
    return result


def measured(inputs, method, loops):
    began = time.perf_counter()
    result = []
    for _ in range(loops):
        for a, b, _wanted in inputs:
            value = (
                shapely.relate_pattern(a, b, "2********")
                if method == "matrix"
                else shapely.intersects(a, b) and not shapely.touches(a, b)
            )
            result.append(bool(value))
    seconds = time.perf_counter() - began
    if result != [wanted for _ in range(loops) for _a, _b, wanted in inputs]:
        raise ValueError(
            "positive-interior alternative violates same-quality reference"
        )
    return {
        "seconds": seconds,
        "calls": len(result),
        "result_sha256": hashlib.sha256(bytes(result)).hexdigest(),
    }


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private predicate comparison must live under /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    inputs = pairs(args.products, args.reference)
    runs = {m: [] for m in ("matrix", "two_predicates")}
    for i in range(3):
        for method in (
            ("matrix", "two_predicates") if i % 2 == 0 else ("two_predicates", "matrix")
        ):
            runs[method].append(measured(inputs, method, 16))
    if args.profile:
        import memray

        for method in runs:
            with memray.Tracker(
                str(args.output / (method + ".bin")),
                trace_python_allocators=True,
                native_traces=False,
            ):
                measured(inputs, method, 16)
    report = {
        "commit": commit,
        "reference_report_sha256": digest(args.reference / "comparison.json"),
        "cases": len(inputs),
        "loops": 16,
        "paired_runs": runs,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Equivalent interior-intersection predicates, including containment, underflow and pure touch. Masks prepared outside timers/traces. No geometric intersection allocation for either alternative; full field/gen/frame/device time and internal copied bytes unmeasured.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(parser.parse_args())

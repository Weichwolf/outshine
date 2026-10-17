"""Replay one pinned kernel against the independently exported exact-area ledger."""

import argparse
import hashlib
import inspect
import json
import subprocess
from fractions import Fraction
from pathlib import Path

from camera_height_inputs import digest
from stair_occupancy_geometry import AffineOccupancy, OccupancyError
from stair_occupancy_precision_cases import actual_cases, controls
from stair_terrain_experiment import frozen


def source_area_cases(reference):
    import shapely
    from outshine_ref.contracts import BuildingFootprint, HeightInterval

    records = []
    for i, item in enumerate(reference["positive_conflict_cases"]):
        if Fraction(item["native_area_m2_rational"]) <= 0:
            raise ValueError("independent positive source-area reference required")
        shape = shapely.from_wkb(item["domain_wkb"])
        query = shapely.from_wkb(item["footprint_wkb"])
        try:
            row = AffineOccupancy(
                "underflow", shape, "control", (0, 0), (100, 0, 0), (101, 0, 0)
            )
            overlap = row.overlaps(
                BuildingFootprint("underflow-query", query, HeightInterval(99, 102))
            )
            state, reason, wrong = "positive" if overlap else "empty", None, not overlap
        except OccupancyError as error:
            state, reason, wrong = "deferred", str(error), False
        records.append(
            {
                "case": "area-underflow:" + str(i),
                "kind": "area_underflow_control",
                "expected_positive": True,
                "state": state,
                "reason": reason,
                "wrong": wrong,
            }
        )
    return records


def run(args):
    controller_commit = frozen()
    module = Path(inspect.getfile(AffineOccupancy))
    pinned = subprocess.check_output(
        [
            "git",
            "show",
            args.kernel_commit
            + ":test/experiments/transport/stair_occupancy_geometry.py",
        ]
    )
    if hashlib.sha256(pinned).hexdigest() != digest(module):
        raise ValueError(
            "loaded kernel differs from the explicitly pinned source commit"
        )
    report = json.loads((args.reference / "comparison.json").read_text())
    ledger = args.reference / "cases.json"
    if (
        digest(ledger) != report["case_ledger_sha256"]
        or digest(args.products / "volumes.json") != report["saved_volume_sha256"]
    ):
        raise ValueError("complete independent reference/model bindings required")
    reference = {r["case"]: r for r in json.loads(ledger.read_text())}
    actual, _count, _queries = actual_cases(args.products)
    cases = [*controls(), *actual]
    if (
        len(reference) != report["checked_cases"]
        or {c[0] for c in cases} != reference.keys()
    ):
        raise ValueError(
            "kernel comparison does not cover the complete independent ledger"
        )
    records = []
    for name, kind, row, heights, _footprint, _cells in cases:
        positive = Fraction(reference[name]["native_area_m2_rational"]) > 0
        try:
            mask = row.conflict_geometry(heights)
            state = "positive" if not mask.is_empty else "empty"
            reason = None
        except OccupancyError as error:
            state, reason = "deferred", str(error)
        wrong = state != "deferred" and (state == "positive") != positive
        if wrong or reason:
            records.append(
                {
                    "case": name,
                    "kind": kind,
                    "expected_positive": positive,
                    "state": state,
                    "reason": reason,
                    "wrong": wrong,
                }
            )
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private baseline comparison must live under /tmp")
    if args.output.exists():
        raise ValueError("fresh baseline comparison output required")
    underflow = report.get("source_area_underflow_control")
    if underflow:
        records.extend(source_area_cases(underflow))
    result = {
        "controller_commit": controller_commit,
        "kernel_commit": args.kernel_commit,
        "loaded_kernel": str(module),
        "kernel_sha256": digest(module),
        "reference_report_sha256": digest(args.reference / "comparison.json"),
        "checked_cases": len(cases)
        + (len(underflow["positive_conflict_cases"]) if underflow else 0),
        "checked_mask_cases": len(cases),
        "wrong_empty_or_positive_decisions": sum(r["wrong"] for r in records),
        "explicit_deferrals": sum(r["state"] == "deferred" for r in records),
        "cases": records,
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--kernel-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

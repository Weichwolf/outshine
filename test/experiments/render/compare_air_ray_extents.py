"""Compare shared physical and opaque-limited ray partitions for #266.

This uses the clear-gas coefficient model and original FP64 camera/reference data.
It cannot establish native LUT/filter/FP32 error, cloud coupling or image quality.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air
from air_path_prefix import AirPrefix, build_cells
from compare_air_depth_tables import load_reference
from compare_air_paths import RADIANCE_FLOOR_PER_SR
from compare_air_prefix import probe_references

ROOT = Path(__file__).resolve().parents[3]


def compare(medium, cases, rows, intervals):
    errors = {"background_200_km": [], "physical_boundary": []}
    work = {name: [] for name in errors}
    for case in cases:
        radius_bound = depth_limit(medium, case.height_km, case.arguments()[1], np.inf)
        opaque_bound = min(200.0, radius_bound)
        references = sorted(
            (row for row in rows if row["case"] == case.name),
            key=lambda row: row["distance_km"],
        )
        if not references:
            raise ValueError(f"missing reference for {case.name}")
        for name, bound in (
            ("background_200_km", opaque_bound),
            ("physical_boundary", radius_bound),
        ):
            cells = build_cells(medium, case.arguments(), bound, intervals, "quadratic")
            prefix = AirPrefix(cells)
            for row in references:
                distance = row["distance_km"]
                if not 0 <= distance <= opaque_bound:
                    raise ValueError("reference is outside the original bounded path")
                radiance, through = prefix.advance(distance)
                target = np.array(row["reference_rgb_per_sr"])
                error = np.max(
                    np.abs(radiance - target)
                    / np.maximum(target, RADIANCE_FLOOR_PER_SR)
                )
                through_error = np.max(np.abs(through - row["reference_t_rgb"]))
                errors[name].append(
                    {
                        "case": case.name,
                        "distance_km": distance,
                        "extent_km": bound,
                        "radiance_relative": float(error),
                        "transmittance_absolute": float(through_error),
                    }
                )
            work[name].append(
                {
                    "case": case.name,
                    "extent_km": bound,
                    "source_positions": cells.source_evaluations,
                    "partial_integrals": int(prefix.partial_integrals),
                }
            )
        print(f"completed {case.name}: {len(references)} queries", flush=True)
    summary = {}
    for name, values in errors.items():
        worst = max(values, key=lambda row: row["radiance_relative"])
        max_t = max(row["transmittance_absolute"] for row in values)
        summary[name] = {
            "queries": len(values),
            "worst_radiance": worst,
            "maximum_transmittance_absolute": max_t,
            "source_positions_max": max(row["source_positions"] for row in work[name]),
            "partial_integrals": sum(row["partial_integrals"] for row in work[name]),
            "meets_isolated_targets": worst["radiance_relative"] <= 0.01
            and max_t <= 0.002,
        }
    return {"summary": summary, "work": work, "errors": errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--dense", type=Path)
    parser.add_argument("--intervals", type=int, default=22)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.intervals != 22:
        raise ValueError(
            "this contract comparison fixes the existing 22-interval candidate"
        )
    root = args.root.resolve()
    reference = load_reference(args.reference, root)
    cases, _ = place_cases(root)
    medium = engine_clear_air(root / "src/world/sky/Atmosphere.h")
    rows = (
        reference["references"] if "references" in reference else reference["results"]
    )
    if args.dense:
        dense = json.loads(args.dense.read_text())
        if (
            not dense["reference_meets_targets"]
            or dense["inputs_sha256"] != reference["inputs_sha256"]
        ):
            raise ValueError("dense distances need the same verified inputs")
        rows = []
        for case in cases:
            bound = depth_limit(medium, case.height_km, case.arguments()[1], 200.0)
            rows.extend(probe_references(medium, case, bound, reference, dense))
            print(f"refined {case.name}", flush=True)
    if any(
        row["reference_refinement_relative_max"] > 0.001
        or row["reference_refinement_t_max"] > 0.0002
        for row in rows
    ):
        raise ValueError("reference refinement did not meet the existing limits")
    result = compare(medium, cases, rows, args.intervals)
    result.update(
        scope=__doc__,
        reference_sha256=hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        dense_sha256=hashlib.sha256(args.dense.read_bytes()).hexdigest()
        if args.dense
        else None,
        maximum_reference_refinement_relative=max(
            row["reference_refinement_relative_max"] for row in rows
        ),
        inputs_sha256=reference["inputs_sha256"],
        intervals=args.intervals,
        requirements={
            "radiance_relative_max": 0.01,
            "radiance_floor_per_sr": RADIANCE_FLOOR_PER_SR,
            "transmittance_absolute_max": 0.002,
        },
        source_sha256={
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "compare_air_ray_extents.py",
                "air_path_prefix.py",
                "air_path_integral.py",
                "air_path_segments.py",
            )
        },
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["summary"]))


if __name__ == "__main__":
    main()

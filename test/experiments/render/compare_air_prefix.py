"""Compare reusable bounded coefficient prefixes, independently of native bindings (#206).

NumPy required; explicit external output, execute under the shared host lock.
The Python proof materializes cells; no GPU memory or frame-time claim follows.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air, gauss_path
from air_path_prefix import AirPrefix, build_cells
from air_path_segments import view_edges
from compare_air_depth_tables import load_reference
from compare_air_paths import RADIANCE_FLOOR_PER_SR

ROOT = Path(__file__).resolve().parents[3]


def probe_references(medium, case, limit, report, dense=None):
    prior = report.get("references", report["results"])
    rows = {row["distance_km"]: row for row in prior if row["case"] == case.name}
    distances = (
        []
        if dense is None
        else [
            row["distance_km"]
            for row in dense["results"]["quadratic_32_fp16"]
            if row["case"] == case.name
        ]
    )
    for edge in view_edges(medium, *case.arguments(), limit):
        distances.extend((edge - 0.001, edge, edge + 0.001))
    for distance in distances:
        if not 0.0 < distance <= limit or distance in rows:
            continue
        arguments = (medium, *case.arguments(), distance)
        reference, reference_t = gauss_path(
            *arguments, 64, optical_samples=16, segmented=True
        )
        refined, refined_t = gauss_path(
            *arguments, 128, optical_samples=32, segmented=True
        )
        scale = np.maximum(reference, RADIANCE_FLOOR_PER_SR)
        rows[distance] = {
            "case": case.name,
            "distance_km": float(distance),
            "reference_rgb_per_sr": reference.tolist(),
            "reference_t_rgb": reference_t.tolist(),
            "reference_refinement_relative_max": float(
                np.max(np.abs(refined - reference) / scale)
            ),
            "reference_refinement_t_max": float(
                np.max(np.abs(refined_t - reference_t))
            ),
        }
    for distance, row in rows.items():
        direct = {}
        for count in (4, 8):
            radiance, through = gauss_path(
                medium, *case.arguments(), distance, count, segmented=True
            )
            reference = np.array(row["reference_rgb_per_sr"])
            direct[f"gauss_{count}"] = {
                "radiance_relative_max": float(
                    np.max(
                        np.abs(radiance - reference)
                        / np.maximum(reference, RADIANCE_FLOOR_PER_SR)
                    )
                ),
                "transmittance_absolute_max": float(
                    np.max(np.abs(through - row["reference_t_rgb"]))
                ),
                "sources_per_query": count
                * (len(view_edges(medium, *case.arguments(), distance)) - 1),
            }
        row["direct_comparison"] = direct
    return [rows[distance] for distance in sorted(rows)]


def compare_prefixes(medium, cases, report, dense=None):
    errors, references, counts = {}, [], {}
    for case in cases:
        limit = depth_limit(medium, case.height_km, case.arguments()[1], 200.0)
        rows = probe_references(medium, case, limit, report, dense)
        references.extend(rows)
        for interpolation in ("constant", "linear", "quadratic"):
            if dense is not None and interpolation != "quadratic":
                continue
            for count in (
                (4, 8, 16, 19) if interpolation == "quadratic" else (8, 16, 32, 47)
            ):
                if dense is not None and count != 19:
                    continue
                key = f"{interpolation}_{count}"
                cells = build_cells(
                    medium, case.arguments(), limit, count, interpolation
                )
                prefix = AirPrefix(cells)
                counts.setdefault(key, []).append(cells.source_evaluations)
                for row in rows:
                    radiance, through = prefix.advance(row["distance_km"])
                    reference = np.array(row["reference_rgb_per_sr"])
                    scale = np.maximum(reference, RADIANCE_FLOOR_PER_SR)
                    errors.setdefault(key, []).append(
                        {
                            "case": case.name,
                            "distance_km": row["distance_km"],
                            "radiance_relative_max": float(
                                np.max(np.abs(radiance - reference) / scale)
                            ),
                            "transmittance_absolute_max": float(
                                np.max(np.abs(through - row["reference_t_rgb"]))
                            ),
                        }
                    )
        print(f"completed {case.name}: {len(rows)} depths", flush=True)
    summary = {
        key: {
            "radiance_relative_max": max(row["radiance_relative_max"] for row in rows),
            "transmittance_absolute_max": max(
                row["transmittance_absolute_max"] for row in rows
            ),
            "source_evaluations_max": max(counts[key]),
        }
        for key, rows in errors.items()
    }
    return summary, errors, references


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dense-depths", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private prefix outputs belong outside the repository")
    reference_report = load_reference(args.reference)
    medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
    cases, _ = place_cases(ROOT)
    dense = None if args.dense_depths is None else load_reference(args.dense_depths)
    summary, errors, references = compare_prefixes(
        medium, cases, reference_report, dense
    )
    refinement = max(row["reference_refinement_relative_max"] for row in references)
    refinement_t = max(row["reference_refinement_t_max"] for row in references)
    report = {
        "scope": "Single Rayleigh scattering and ozone, clear case inputs from #212. "
        "Fixed quadratic coefficient cells split at physical boundaries. Linear or constant "
        "local coefficients, including positive quadratic Bernstein coefficients; "
        "four-point source integration normalized to exact polynomial cell opacity, "
        "shared across queries. "
        "No multiple scattering, native LUT approximation, runtime image or GPU costs.",
        "requirements": reference_report["requirements"],
        "source_budget_candidate": 64,
        "direct_comparison": {
            name: {
                "radiance_relative_max": max(
                    row["direct_comparison"][name]["radiance_relative_max"]
                    for row in references
                ),
                "transmittance_absolute_max": max(
                    row["direct_comparison"][name]["transmittance_absolute_max"]
                    for row in references
                ),
                "sources_per_query_max": max(
                    row["direct_comparison"][name]["sources_per_query"]
                    for row in references
                ),
                "scope": "Per query; repeated cloud-event queries would repeat these source evaluations.",
            }
            for name in ("gauss_4", "gauss_8")
        },
        "reference_meets_targets": refinement <= 0.001 and refinement_t <= 0.0002,
        "maximum_reference_refinement_relative_error": refinement,
        "maximum_reference_transmittance_refinement_error": refinement_t,
        "summary": summary,
        "meeting_requirements": [
            name
            for name, row in summary.items()
            if refinement <= 0.001
            and refinement_t <= 0.0002
            and row["radiance_relative_max"] <= 0.01
            and row["transmittance_absolute_max"] <= 0.002
            and row["source_evaluations_max"] <= 64
        ],
        "results": errors,
        "references": references,
        "inputs_sha256": reference_report["inputs_sha256"],
        "reference_sha256": hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        "dense_depths_sha256": None
        if args.dense_depths is None
        else hashlib.sha256(args.dense_depths.read_bytes()).hexdigest(),
        "sources_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in Path(__file__).parent.glob("*air*.py")
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "prefix-comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "rays": len(references),
                "reference_valid": report["reference_meets_targets"],
                "summary": summary,
            }
        )
    )


if __name__ == "__main__":
    main()

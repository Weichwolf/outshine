"""Measure bounded depth interpolation against the converged finite-air reference (#206).

NumPy is required. Pass an explicit external --output; run under the shared host lock.
Only the depth axis is tested: this cannot validate a frustum volume or a GPU budget.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import build_column, depth_nodes, sample_column
from air_path_integral import engine_clear_air, gauss_path
from air_path_segments import view_edges
from compare_air_paths import RADIANCE_FLOOR_PER_SR

ROOT = Path(__file__).resolve().parents[3]


def validation_distances(medium, case, limit, sparse_rows):
    distances = [row["distance_km"] for row in sparse_rows if row["case"] == case.name]
    distances.extend(np.geomspace(0.001, limit, 96))
    distances.extend(np.random.default_rng(206).uniform(0.001, limit, 32))
    for mapping in ("linear", "quadratic"):
        for count in (32, 64, 128):
            nodes = depth_nodes(count, limit, mapping)
            distances.extend((nodes[:-1] + nodes[1:]) * 0.5)
    for edge in view_edges(medium, *case.arguments(), limit):
        distances.extend([edge - 0.001, edge, edge + 0.001])
    return np.unique([d for d in distances if 0.0 < d <= limit])


def evaluate_columns(medium, case, columns, distances, errors):
    refinement, refinement_t = 0.0, 0.0
    for distance in distances:
        arguments = (medium, *case.arguments(), distance)
        reference, reference_t = gauss_path(
            *arguments, 64, optical_samples=16, segmented=True
        )
        refined, refined_t = gauss_path(
            *arguments, 128, optical_samples=32, segmented=True
        )
        scale = np.maximum(reference, RADIANCE_FLOOR_PER_SR)
        refinement = max(refinement, float(np.max(np.abs(refined - reference) / scale)))
        refinement_t = max(refinement_t, float(np.max(np.abs(refined_t - reference_t))))
        for key, (column, mapping) in columns.items():
            radiance, through = sample_column(column, distance, mapping)
            errors.setdefault(key, []).append(
                {
                    "case": case.name,
                    "distance_km": float(distance),
                    "radiance_relative_max": float(
                        np.max(np.abs(radiance - reference) / scale)
                    ),
                    "transmittance_absolute_max": float(
                        np.max(np.abs(through - reference_t))
                    ),
                    "view_source_evaluations": column[3],
                }
            )
    return refinement, refinement_t


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError(
            "private table comparison outputs belong outside the repository"
        )
    medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
    cases, _ = place_cases(ROOT)
    reference_report = json.loads(args.reference.read_text())
    if not reference_report["reference_meets_targets"]:
        raise ValueError("depth comparison requires a converged reference")
    for name, expected in reference_report["inputs_sha256"].items():
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"reference input changed: {name}")
    for name in ("air_path_integral.py", "air_path_segments.py", "air_path_cases.py"):
        expected = reference_report["sources_sha256"][name]
        if (
            hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
            != expected
        ):
            raise ValueError(f"reference physics changed: {name}")
    errors = {}
    refinement, refinement_t = 0.0, 0.0
    for case in cases:
        columns = {
            f"{mapping}_{count}_{precision}": (
                build_column(
                    medium,
                    case.arguments(),
                    count,
                    mapping,
                    half_storage=precision == "fp16",
                ),
                mapping,
            )
            for mapping in ("linear", "quadratic")
            for count in (32, 64, 128)
            for precision in ("fp64", "fp16")
        }
        limit = next(iter(columns.values()))[0][2]
        distances = validation_distances(
            medium, case, limit, reference_report["results"]
        )
        refined_l, refined_t = evaluate_columns(
            medium, case, columns, distances, errors
        )
        refinement, refinement_t = (
            max(refinement, refined_l),
            max(refinement_t, refined_t),
        )
    summary = {
        key: {
            "radiance_relative_max": max(row["radiance_relative_max"] for row in rows),
            "transmittance_absolute_max": max(
                row["transmittance_absolute_max"] for row in rows
            ),
            "view_source_evaluations_max": max(
                row["view_source_evaluations"] for row in rows
            ),
        }
        for key, rows in errors.items()
    }
    report = {
        "scope": "Depth axis only, exact angular rays, single Rayleigh scattering and ozone. "
        "A per-ray top/ground bound scales each column; no angular interpolation, "
        "multiple scattering, native transmittance-LUT error, runtime image or GPU cost proof.",
        "units": {"radiance": "sr^-1 per solar illuminance", "distance": "km"},
        "radiance_relative_floor_per_sr": RADIANCE_FLOOR_PER_SR,
        "reference_sha256": hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        "inputs_sha256": reference_report["inputs_sha256"],
        "reference": "Converged report from compare_air_paths.py; inputs and physics "
        "hashes verified. Piecewise interpolation uses physical-distance weights; "
        "the quadratic index alone is not used as the final linear texture weight.",
        "summary": summary,
        "validation": "Original distances plus 96 logarithmic and 32 deterministic random "
        "depths, all tested grid-interval midpoints and physical boundaries +/- 1 m. "
        "Every additional reference receives the unchanged independent refinement guard.",
        "reference_meets_targets": refinement <= 0.001 and refinement_t <= 0.0002,
        "maximum_reference_refinement_relative_error": refinement,
        "maximum_reference_transmittance_refinement_error": refinement_t,
        "results": errors,
        "sources_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in Path(__file__).parent.glob("*air*.py")
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "depth-tables.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    main()

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
from air_path_depth_table import build_column, sample_column
from air_path_integral import engine_clear_air
from compare_air_paths import RADIANCE_FLOOR_PER_SR

ROOT = Path(__file__).resolve().parents[3]


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
        for row in reference_report["results"]:
            if row["case"] != case.name:
                continue
            distance = row["distance_km"]
            reference, reference_t = (
                np.array(row["reference_rgb_per_sr"]),
                np.array(row["reference_t_rgb"]),
            )
            scale = np.maximum(reference, RADIANCE_FLOOR_PER_SR)
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

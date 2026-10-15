"""Compare native streamed RGB output to the unchanged dense physical reference."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np

RADIANCE_FLOOR_PER_SR = 1e-5
MAXIMUM_RADIANCE_ERROR = 0.01
MAXIMUM_TRANSMITTANCE_ERROR = 0.002
MAXIMUM_SOURCE_POSITIONS = 63
SAMPLE = np.dtype(
    [("radiance", "<f4", 3), ("through", "<f4", 3), ("sources", "<u4"), ("pad", "<u4")]
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--analytic", action="store_true")
    args = parser.parse_args()
    metadata_path = args.fixtures / "rays.json"
    if metadata_path.exists():
        metadata = json.loads(metadata_path.read_text())
    else:
        with gzip.open(args.fixtures / "rays.json.gz", "rt") as source:
            metadata = json.load(source)
    radiance_limit = 1e-5 if args.analytic else MAXIMUM_RADIANCE_ERROR
    through_limit = 2e-6 if args.analytic else MAXIMUM_TRANSMITTANCE_ERROR
    samples = np.fromfile(args.samples, dtype=SAMPLE)
    if len(samples) != metadata["query_count"]:
        raise ValueError("native output extent differs from the fixed query set")
    reference = np.asarray(
        [row["reference_rgb_per_sr"] for row in metadata["references"]]
    )
    reference_t = np.asarray([row["reference_t_rgb"] for row in metadata["references"]])
    errors = np.max(
        np.abs(samples["radiance"] - reference)
        / np.maximum(reference, RADIANCE_FLOOR_PER_SR),
        axis=1,
    )
    errors_t = np.max(np.abs(samples["through"] - reference_t), axis=1)
    finite_positive = bool(
        np.isfinite(samples["radiance"]).all()
        and np.isfinite(samples["through"]).all()
        and (samples["radiance"] >= 0).all()
        and (samples["through"] >= 0).all()
        and (samples["through"] <= 1).all()
    )
    source_bound = bool((samples["sources"] <= MAXIMUM_SOURCE_POSITIONS).all())
    distances = np.fromfile(args.fixtures / "queries.bin", dtype="<f4")
    repeats = 0
    idempotent = True
    for case in metadata["descriptions"]:
        start, end = case["begin"], case["end"]
        for index in range(start + 1, end):
            if distances[index] == distances[index - 1]:
                repeats += 1
                idempotent &= samples[index].tobytes() == samples[index - 1].tobytes()
    worst = int(np.argmax(errors))
    worst_t = int(np.argmax(errors_t))
    report = {
        "scope": "Homogeneous source closed forms; no atmospheric multiple-scattering convergence claim."
        if args.analytic
        else "Native streaming plus FP32 geometry and native-equation solar LUT error against original FP64 scenario references. No full-frame, native multiple-scattering convergence or Place-image claim.",
        "fixture_scope": metadata["scope"],
        "records": len(samples),
        "finite_positive": finite_positive,
        "source_bound": source_bound,
        "maximum_source_positions": int(samples["sources"].max()),
        "equal_query_pairs": repeats,
        "idempotent": bool(idempotent),
        "radiance_relative_max": float(errors.max()),
        "transmittance_absolute_max": float(errors_t.max()),
        "requirements": {
            "radiance_floor_per_sr": RADIANCE_FLOOR_PER_SR,
            "radiance_relative_max": radiance_limit,
            "transmittance_absolute_max": through_limit,
            "source_positions_max": MAXIMUM_SOURCE_POSITIONS,
        },
        "worst_radiance": {
            **metadata["references"][worst],
            "native_l_rgb": samples["radiance"][worst].tolist(),
            "native_t_rgb": samples["through"][worst].tolist(),
            "query_f32_km": float(distances[worst]),
        },
        "worst_transmittance": {
            **metadata["references"][worst_t],
            "native_l_rgb": samples["radiance"][worst_t].tolist(),
            "native_t_rgb": samples["through"][worst_t].tolist(),
            "query_f32_km": float(distances[worst_t]),
        },
        "samples_sha256": hashlib.sha256(args.samples.read_bytes()).hexdigest(),
    }
    report["meets_targets"] = bool(
        finite_positive
        and source_bound
        and idempotent
        and repeats > 0
        and errors.max() <= radiance_limit
        and errors_t.max() <= through_limit
    )
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: value
                for key, value in report.items()
                if not key.startswith("worst_")
            }
        )
    )
    raise SystemExit(0 if report["meets_targets"] else 1)


if __name__ == "__main__":
    main()

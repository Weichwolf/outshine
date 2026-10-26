"""Measure actual sampled solar transmission against independent physical probes."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_vertical_reference import VerticalAir
from check_air_prefix_gpu import SAMPLE
from export_solar_lookup_probes import visible


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    metadata = json.loads((args.fixtures / "probes.json").read_text())
    for name, digest in metadata["files_sha256"].items():
        if hashlib.sha256((args.fixtures / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"probe input changed: {name}")
    samples = np.fromfile(args.samples, dtype=SAMPLE)
    looks = np.fromfile(args.fixtures / "looks.bin", dtype="<f4").reshape(-1, 2)
    expected = np.fromfile(args.fixtures / "reference.bin", dtype="<f8").reshape(-1, 3)
    if len(samples) != metadata["records"] or len(expected) != len(samples):
        raise ValueError("sample extent differs from the independent probes")
    actual = samples["through"].astype(float)
    if not np.array_equal(samples["radiance"], samples["through"]):
        raise ValueError("zero query/multiple-scatter control changed the solar output")
    error = np.max(np.abs(actual - expected), axis=1)
    blocked = ~visible(VerticalAir.read(args.fixtures / "medium.bin"), looks)
    valid = bool(
        np.isfinite(actual).all() and (actual >= 0).all() and (actual <= 1).all()
    )
    identity = bool((actual[blocked] == 0).all())
    worst = int(np.argmax(error))
    report = {
        "scope": "Actual LUT generation, RGBA16F storage and GPU sampling; solar transfer only. Absolute errors are measurements, not a new acceptance threshold.",
        "records": len(samples),
        "finite_bounded": valid,
        "blocked_zero": identity,
        "maximum_absolute_T_error": float(error.max()),
        "p99_absolute_T_error": float(np.quantile(error, 0.99)),
        "rms_absolute_T_error": float(np.sqrt(np.mean((actual - expected) ** 2))),
        "worst": {
            "index": worst,
            "radius_cosine": looks[worst].tolist(),
            "expected": expected[worst].tolist(),
            "actual": actual[worst].tolist(),
        },
        "samples_sha256": hashlib.sha256(args.samples.read_bytes()).hexdigest(),
        "reference_refinement_absolute_T": metadata["reference_refinement_absolute_T"],
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
    return 0 if valid and identity else 1


if __name__ == "__main__":
    raise SystemExit(main())

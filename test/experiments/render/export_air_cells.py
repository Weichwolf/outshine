"""Export explicit private FP32 cell inputs and independent FP64 expectations for the GPU probe."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air
from air_path_prefix import AirCells, build_cells, cell_integral
from compare_air_depth_tables import load_reference

ROOT = Path(__file__).resolve().parents[3]


def expectations(rows):
    samples = []
    for row in rows:
        width, distance = float(row[0]), float(row[1])
        extinction = np.repeat(row[2:5, None], 3, axis=1)[None].astype(np.float64)
        emission = np.repeat(row[5:8, None], 3, axis=1)[None].astype(np.float64)
        cells = AirCells(np.array([0.0, width]), extinction, emission, 0)
        radiance, through = cell_integral(cells, 0, distance)
        samples.append([radiance[0], through[0]])
    return np.asarray(samples, dtype="<f8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private cell fixtures belong outside the repository")
    reference = load_reference(args.reference)
    code = Path(__file__).with_name("air_path_prefix.py")
    if (
        hashlib.sha256(code.read_bytes()).hexdigest()
        != reference["sources_sha256"][code.name]
    ):
        raise ValueError("the verified prefix implementation changed")
    medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
    cases, _ = place_cases(ROOT)
    rows = []
    for case in cases:
        limit = depth_limit(medium, case.height_km, case.arguments()[1], 200.0)
        cells = build_cells(medium, case.arguments(), limit, 19, "quadratic")
        for index, width in enumerate(np.diff(cells.edges)):
            for fraction in (0.0, 0.001, 0.25, 0.5, 1.0):
                for channel in range(3):
                    rows.append(
                        [
                            width,
                            width * fraction,
                            *cells.extinction[index, :, channel],
                            *cells.emission[index, :, channel],
                        ]
                    )
    rng = np.random.default_rng(206)
    for _ in range(1024):
        width = float(10.0 ** rng.uniform(-3.0, 1.0))
        rows.append(
            [
                width,
                width * rng.uniform(),
                *10.0 ** rng.uniform(-12.0, 2.0, 3),
                *10.0 ** rng.uniform(-12.0, 0.0, 3),
            ]
        )
    for extinction in (0.0, 1e-12, 1e-8, 0.001, 0.1, 1.0, 100.0):
        for fraction in (0.0, 0.0001, 0.01, 0.25, 0.75, 1.0):
            rows.append(
                [
                    10.0,
                    10.0 * fraction,
                    extinction,
                    extinction,
                    extinction,
                    0.03,
                    0.03,
                    0.03,
                ]
            )
    inputs = np.asarray(rows, dtype="<f4")
    expected = expectations(inputs)
    output.mkdir(parents=True, exist_ok=True)
    inputs.tofile(output / "inputs.bin")
    expected.tofile(output / "expected.bin")
    metadata = {
        "scope": "One-cell mathematics on quantized FP32 coefficients; independent NumPy FP64 expectations. No source-LUT, streaming-state, GPU-time or runtime-image proof.",
        "count": len(inputs),
        "input_stride_bytes": 32,
        "expected_stride_bytes": 16,
        "endianness": "little",
        "reference_sha256": hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        "files_sha256": {
            name: hashlib.sha256((output / name).read_bytes()).hexdigest()
            for name in ("inputs.bin", "expected.bin")
        },
    }
    (output / "cells.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    main()

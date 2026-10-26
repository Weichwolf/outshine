"""Counterexample: absolute FP32 metre positions cannot retain fine asphalt phase."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from asphalt_field import parameters, point_field
from asphalt_filter import srgb


def compare():
    rows = []
    pitch = 0.024
    for anchor in (0, 1000, 10000, 100000, 1000000):
        centre = np.float32(anchor + 0.013)
        step = float(np.spacing(centre))
        pairs = []
        for index in range(32):
            at = float(np.float32(centre + index * step))
            low, high = at - 0.49 * step, at + 0.49 * step
            points = np.array([[low, 0.019], [high, 0.019]])
            supplied = points.astype("<f4")
            np.testing.assert_array_equal(supplied[0], supplied[1])
            field = point_field(points / pitch)
            colour, _ = parameters(field)
            minimum_error = float(
                np.max(abs(srgb(colour[0]) - srgb(colour[1]))) * 255 / 2
            )
            full = points / pitch
            cell = np.floor(full).astype(np.int64)
            local = (full - cell).astype("<f4")
            reconstructed = cell + local.astype(float)
            recovered_colour, _ = parameters(point_field(reconstructed))
            recovery_error = float(
                np.max(abs(srgb(recovered_colour) - srgb(colour))) * 255
            )
            pairs.append(
                {
                    "true_metres": points.tolist(),
                    "fp32_bits": supplied.view("<u4").tolist(),
                    "any_function_of_fp32_only_minimum_max_error_srgb_lsb": minimum_error,
                    "cell_plus_local_fp32_colour_error_srgb_lsb": recovery_error,
                }
            )
        rows.append(
            {
                "anchor_m": anchor,
                "spacing_m": step,
                "pairs": pairs,
                "largest_proven_minimum_error_lsb": max(
                    p["any_function_of_fp32_only_minimum_max_error_srgb_lsb"]
                    for p in pairs
                ),
            }
        )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    output = args.output.resolve()
    if output.is_relative_to(root) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("output must be outside the checkout in system temp")
    output.parent.mkdir(parents=True, exist_ok=True)
    rows = compare()
    if max(r["largest_proven_minimum_error_lsb"] for r in rows) <= 1:
        raise ValueError(
            "phase counterexample failed to demonstrate lost input information"
        )
    report = {
        "scope": "CPU input indistinguishability proof, not actual engine frame/UV execution",
        "pitch_m": 0.024,
        "cases": rows,
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print([(r["anchor_m"], r["largest_proven_minimum_error_lsb"]) for r in rows])


if __name__ == "__main__":
    main()

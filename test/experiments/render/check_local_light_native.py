"""Compare actual native direct-light images and exact emitter-support counts (#448)."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from local_light_inputs import ROOT

WIDTH, HEIGHT, CHANNELS = 1920, 1080, 4


def capture(path):
    if path.stat().st_size != WIDTH * HEIGHT * CHANNELS * 4:
        raise ValueError(f"incomplete native image: {path}")
    return np.memmap(path, mode="r", dtype="<f4", shape=(HEIGHT, WIDTH, CHANNELS))


def compare(folder, support_counter=True):
    records = []
    for plane, depth in enumerate([25, 250, 500, 5000]):
        reference = capture(folder / f"brute-{plane}.rgba32")
        for method in ["tiles", "log"]:
            actual = capture(folder / f"{method}-{plane}.rgba32")
            delta = np.abs(actual[..., :3] - reference[..., :3])
            limits = np.maximum(1e-7, np.abs(reference[..., :3]) * 1e-5)
            records.append(
                {
                    "plane_m": depth,
                    "method": method,
                    "nonfinite": int(np.count_nonzero(~np.isfinite(actual)))
                    + int(np.count_nonzero(~np.isfinite(reference))),
                    "negative_components": int(np.count_nonzero(actual < 0)),
                    "rgb_failures": int(np.count_nonzero(delta > limits)),
                    "support_mismatches": int(
                        np.count_nonzero(actual[..., 3] != reference[..., 3])
                        if support_counter
                        else 0
                    ),
                    "support_nonintegers": int(
                        np.count_nonzero(actual[..., 3] != np.floor(actual[..., 3]))
                        if support_counter
                        else 0
                    ),
                    "maximum_abs_rgb": float(delta.max()),
                    "maximum_relative_floor_1e_minus5": float(
                        (delta / np.maximum(1e-5, np.abs(reference[..., :3]))).max()
                    ),
                    "reference_rgb_max": float(reference[..., :3].max()),
                    "reference_nonzero_pixels": int(
                        np.count_nonzero(np.any(reference[..., :3] > 0, axis=-1))
                    ),
                    "reference_support_max": float(reference[..., 3].max())
                    if support_counter
                    else None,
                    "actual_sha256": hashlib.sha256(actual).hexdigest(),
                    "reference_sha256": hashlib.sha256(reference).hexdigest(),
                }
            )
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--without-counter", action="store_true")
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private results belong outside the repository")
    records = compare(args.input, support_counter=not args.without_counter)
    keys = [
        "nonfinite",
        "negative_components",
        "rgb_failures",
        "support_mismatches",
        "support_nonintegers",
    ]
    failed = any(record[key] for record in records for key in keys)
    report = {
        "scope": "Actual existing direct-shader full sum versus enumerated candidates, including geometric/angular/shadow support before material-facing tests. Not an independent BRDF reference or runtime/Place/device acceptance.",
        "profile_px": [WIDTH, HEIGHT],
        "diagnostic_support_counter": not args.without_counter,
        "records": records,
        "failed": failed,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "failed": failed,
                "rgb_failures": sum(r["rgb_failures"] for r in records),
                "support_mismatches": sum(r["support_mismatches"] for r in records),
            }
        )
    )
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())

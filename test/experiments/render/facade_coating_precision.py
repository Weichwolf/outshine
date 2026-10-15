"""Measure fine coating loss in the existing Earth-sized FP32 material frame."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_native import ROOT, capture, prepare_input, shaders
from rock_direction import noise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    latitude, longitude = np.deg2rad([48.2, 16.37])
    radius = 6378137 / np.sqrt(1 - 6.6943799901413165e-3 * np.sin(latitude) ** 2)
    origin = np.array(
        [
            (radius + 200) * np.cos(latitude) * np.cos(longitude) - 6378137,
            (radius + 200) * np.cos(latitude) * np.sin(longitude),
            -((1 - 6.6943799901413165e-3) * radius + 200) * np.sin(latitude),
        ]
    )
    offsets = np.arange(81) * 0.025
    points = np.concatenate([origin + offsets[:, None] * axis for axis in np.eye(3)])
    metric = np.zeros((len(points), 3, 3))
    metric[:, 0] = points
    values = np.zeros((len(points), 7))
    colour = prepare_input(output, values, metric, np.zeros(len(points)))
    shaders(output)
    actual = capture(args.binary.resolve(), output, 1, "native", len(values), 13)
    expected = noise(points / 1.5) - 0.5
    np.savez_compressed(
        output / "cases.npz", points=points, actual=actual, expected=expected
    )
    report = {
        "scope": __doc__,
        "origin_m": origin.tolist(),
        "step_m": 0.025,
        "cases": len(points),
        "native_executed": True,
        "max_ideal_field_error": float(np.max(np.abs(actual[:, 7] - expected))),
        "max_ideal_rgb_error": float(
            np.max(np.abs(actual[:, 7] - expected)) * (0.16 * colour).max()
        ),
        "unchanged_neighbour_fraction": float(
            np.mean(np.diff(actual[:, 7].reshape(3, -1), axis=1) == 0)
        ),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

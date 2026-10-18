"""Independent global cubic diagnostic; not an admitted long-tube producer."""

import argparse
import json
from pathlib import Path

import numpy as np
from limb_paths import dominant_paths
from oak_scaffold import tapered_radii
from oak_skin_experiment import source_crown
from scipy.interpolate import CubicHermiteSpline, PchipInterpolator


def root_probe(crown, radii, path):
    p = crown.positions[np.array(path)]
    if not 2 <= len(p) <= 1025:
        raise ValueError("bounded root-path diagnostic required")
    derivatives = []
    for i in range(len(p)):
        if i == 0:
            d = p[1] - p[0]
        elif i == len(p) - 1:
            d = p[-1] - p[-2]
        else:
            a, b = p[i] - p[i - 1], p[i + 1] - p[i]
            d = a / np.linalg.norm(a) + b / np.linalg.norm(b)
            d = d / np.linalg.norm(d) * min(np.linalg.norm(a), np.linalg.norm(b))
        derivatives.append(d)
    knots = np.linspace(0, 1, len(p))
    curve = CubicHermiteSpline(knots, p, np.array(derivatives) * (len(p) - 1))
    values = radii[np.array(path)].copy()
    values[-1] *= 0.35
    profile = PchipInterpolator(knots, values)
    u = np.linspace(0, 1, 2 * (len(p) - 1) + 1)
    velocity, acceleration = curve(u, 1), curve(u, 2)
    speed = np.linalg.norm(velocity, axis=1)
    tangent = velocity / speed[:, None]
    curvature = (
        acceleration - np.sum(acceleration * tangent, axis=1)[:, None] * tangent
    ) / speed[:, None] ** 2
    condition = profile(u) * np.linalg.norm(curvature, axis=1)
    peak = int(np.argmax(condition))
    interval = min(peak // 2, len(path) - 2)
    return {
        "source_nodes": len(path),
        "maximum_sampled_radius_curvature": float(condition[peak]),
        "maximum_source_interval": list(path[interval : interval + 2]),
        "failed_samples": int((condition >= 1).sum()),
        "samples": len(u),
    }


def run(inputs, output):
    study = json.loads((inputs / "comparison.json").read_text())
    crown, hints = source_crown(inputs, study)
    radii = tapered_radii(
        crown.parents,
        study["recipe"]["root_radius_m"],
        study["recipe"]["terminal_radius_m"],
    )
    old = dominant_paths(crown.positions, crown.parents, radii, max_nodes=16384)
    new = dominant_paths(
        crown.positions, crown.parents, radii, max_nodes=16384, continuations=hints
    )
    r = {
        "radius_only_root": root_probe(crown, radii, old[0]),
        "source_axis_root": root_probe(crown, radii, new[0]),
        "source_continuation_count": int((hints >= 0).sum()),
        "maximum_source_path_nodes": max(map(len, new)),
        "scope": "global SciPy Hermite diagnostic, not a production-capacity bypass, continuous regularity or self-intersection proof",
    }
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private diagnostics under/tmp")
    output.write_text(json.dumps(r, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)

"""Compare thin-cell transport against direct converged FP64 quadrature (#206).

The fixed input records are the existing quantized cell fixtures. Thick cells
retain the previous four-node rule. This experiment measures no device time.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
FLOOR = 1e-5


def restrict(rows):
    fraction = rows[:, 1] / rows[:, 0]
    controls = []
    for first in (2, 5):
        a, b, c = rows[:, first : first + 3].T
        controls.append(
            np.stack(
                (
                    a,
                    a * (1 - fraction) + b * fraction,
                    a * (1 - fraction) ** 2
                    + 2 * b * fraction * (1 - fraction)
                    + c * fraction**2,
                ),
                axis=1,
            )
        )
    return controls


def gauss(distance, sigma, source, count, normalized=False):
    nodes, weights = np.polynomial.legendre.leggauss(count)
    unit, weights = (nodes + 1) / 2, weights / 2
    a, b, c = (sigma[:, i, None] for i in range(3))
    depth = distance[:, None] * (
        a * (unit - unit**2 + unit**3 / 3)
        + b * (unit**2 - 2 * unit**3 / 3)
        + c * unit**3 / 3
    )
    value = lambda controls: (
        controls[:, 0, None] * (1 - unit) ** 2
        + 2 * controls[:, 1, None] * unit * (1 - unit)
        + controls[:, 2, None] * unit**2
    )
    weighted = weights * np.exp(-depth)
    light = np.sum(weighted * value(source), axis=1)
    if not normalized:
        return distance * light
    tau = distance * sigma.sum(axis=1) / 3
    denominator = np.sum(weighted * value(sigma), axis=1)
    return np.divide(
        -np.expm1(-tau) * light,
        denominator,
        out=distance * light,
        where=denominator > 0,
    )


def moment_rule(distance, sigma, source):
    tau = distance * sigma.sum(axis=1) / 3
    integral = distance * source.sum(axis=1) / 3
    matrix = np.array([[10, 4, 1], [16, 10, 4], [19, 16, 10]]) / 180
    moment = distance**2 * np.einsum("ni,ij,nj->n", source, matrix, sigma)
    opacity_scale = np.divide(
        -np.expm1(-tau), tau, out=np.ones_like(tau), where=tau > 0
    )
    return opacity_scale * (integral - moment) / (1 - tau / 2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--samples", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if ROOT == args.output.resolve() or ROOT in args.output.resolve().parents:
        raise ValueError("private results belong outside the repository")
    rows = np.fromfile(args.inputs, dtype="<f4").reshape(-1, 8).astype(float)
    if (
        not np.isfinite(rows).all()
        or np.any(rows[:, 0] <= 0)
        or np.any(rows[:, 1:] < 0)
        or np.any(rows[:, 1] > rows[:, 0])
    ):
        raise ValueError(
            "expected valid positive coefficient cells and bounded distances"
        )
    distance = rows[:, 1]
    sigma, source = restrict(rows)
    tau = distance * sigma.sum(axis=1) / 3
    reference = gauss(distance, sigma, source, 32)
    refined = gauss(distance, sigma, source, 64)
    scale = np.maximum(abs(refined), FLOOR)
    reports = []
    for name, values, limits in (
        (
            "normalized_gauss2",
            gauss(distance, sigma, source, 2, True),
            (1e-4, 1e-3, 1e-2),
        ),
        (
            "normalized_first_moment",
            moment_rule(distance, sigma, source),
            (1e-4, 1e-3, 0.003),
        ),
    ):
        for limit in limits:
            eligible = (distance > 0) & (tau <= limit)
            reports.append(
                {
                    "rule": name,
                    "maximum_optical_depth": limit,
                    "eligible_nonzero": int(eligible.sum()),
                    "relative_L_error": float(
                        (abs(values - refined) / scale)[eligible].max()
                    ),
                    "reference_refinement": float(
                        (abs(reference - refined) / scale)[eligible].max()
                    ),
                }
            )
    chosen = (distance > 0) & (tau <= 1e-3)
    bound = max(0.5 * 1e-6 * np.exp(0.001), 1e-6 / (6 * (1 - 0.001 / 2)))
    measured = float(
        (abs(moment_rule(distance, sigma, source) - refined) / scale)[chosen].max()
    )
    refinement = float((abs(reference - refined) / scale)[chosen].max())
    if measured > bound + 2e-14 or refinement > 1e-10:
        raise ValueError("moment rule violates its bound or reference is not converged")
    result = {
        "scope": "Thin positive quadratic cells only; no source-fit, native GPU cost or Place proof.",
        "inputs_sha256": hashlib.sha256(args.inputs.read_bytes()).hexdigest(),
        "records": len(rows),
        "nonzero_records": int((distance > 0).sum()),
        "comparisons": reports,
        "exact_arithmetic_relative_bound": float(bound),
        "saved_quadrature_nodes_for_first_moment": int(4 * chosen.sum()),
        "requirements": {
            "native_relative_L_error": 1e-5,
            "native_absolute_T_error": 2e-6,
        },
    }
    if args.samples:
        actual = np.fromfile(args.samples, dtype="<f4").reshape(-1, 2)
        if len(actual) != len(rows):
            raise ValueError("native result count differs from the fixed input set")
        if (
            not np.isfinite(actual).all()
            or np.any(actual < 0)
            or np.any(actual[:, 1] > 1)
        ):
            raise ValueError("native transport must be finite, positive and T <= 1")
        native_l = float((abs(actual[:, 0] - refined) / scale)[chosen].max())
        native_t = float(abs(actual[:, 1] - np.exp(-tau))[chosen].max())
        result["native_thin_error"] = {"relative_L": native_l, "absolute_T": native_t}
        if native_l > 1e-5 or native_t > 2e-6:
            raise ValueError("native thin-cell result exceeds the unchanged tolerances")
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

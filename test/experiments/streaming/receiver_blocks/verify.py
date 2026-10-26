"""Check block extrema and per-graph roundoff against exact rational projections."""

import argparse
import itertools
import json
import random
from fractions import Fraction
from pathlib import Path

import correlated
import prepared
from probe import apply


def exact_apply(matrix, point):
    m = [Fraction(v) for v in matrix]
    return [
        m[12 + row] + sum(m[column * 4 + row] * point[column] for column in range(3))
        for row in range(4)
    ]


def reference(point, inverse, eye, lights):
    exact_h = exact_apply(inverse, [Fraction(v) for v in point])
    actual_h = apply(inverse, point)
    exact_world = [exact_h[i] / exact_h[3] - Fraction(eye[i]) for i in range(3)]
    actual_world = [actual_h[i] / actual_h[3] - eye[i] for i in range(3)]
    exact, actual = [], []
    for light in lights:
        e = exact_apply(light, exact_world)
        a = apply(light, actual_world)
        exact.append([e[i] / e[3] for i in range(3)])
        actual.append([a[i] / a[3] for i in range(3)])
    return exact, actual


def identity():
    return [1.0 if i % 5 == 0 else 0.0 for i in range(16)]


def cases(frame):
    inv = frame["inverse_camera_clip"]
    eye = frame["eye"]
    lights = [r["projection"] for r in frame["shadow"]["regions"]]
    yield "captured", [(-0.2, 0.4), (-0.4, 0.25), (-0.8, -0.2)], inv, eye, lights
    inv = identity()
    yield "identity", [(-1.0, 1.0)] * 3, inv, [0.0] * 3, [identity()]
    inv[11] = -0.125
    light = identity()
    light[0], light[5], light[10], light[12] = 0.003, -0.002, 0.1, 0.123
    yield (
        "perspective-camera",
        [(-0.75, 0.8)] * 3,
        inv,
        [42.3, -17.5, 1000.125],
        [light],
    )
    light[3], light[7], light[11], light[15] = 1e-6, -2e-6, 1e-6, 2.0
    yield "perspective-light", [(-0.75, 0.8)] * 3, inv, [42.3, -17.5, 1000.125], [light]
    inv = identity()
    inv[12], inv[13], inv[14] = 1e8, -1e8, 1e8
    yield "cancellation", [(-0.2, 0.4)] * 3, inv, [1e8, -1e8, 1e8], [identity()]
    inv = identity()
    inv[15], inv[11] = 1e-9, 1e-9
    yield "small-positive-w", [(0.1, 0.8)] * 3, inv, [1e8] * 3, [identity()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frame", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    frame = json.loads(args.frame.read_text())
    rng = random.Random(1847703)
    checked = []
    for name, box, inverse, eye, lights in cases(frame):
        try:
            bounds = correlated.project(box, inverse, eye, lights)
            errors = correlated.errors(box, inverse, eye, lights)
            prepared_bounds = prepared.Prepared(inverse, eye, lights, box)(box)
        except ArithmeticError:
            checked.append({"name": name, "unknown": True})
            continue
        points = list(itertools.product(*box))
        points += [
            tuple(lo + (hi - lo) * rng.random() for lo, hi in box) for _ in range(64)
        ]
        comparisons = 0
        for point in points:
            exact, actual = reference(point, inverse, eye, lights)
            for i, (e, a) in enumerate(zip(exact, actual, strict=True)):
                for axis in range(3):
                    lo, hi = bounds[i][axis]
                    if (
                        not Fraction(lo) <= e[axis] <= Fraction(hi)
                        or not lo <= a[axis] <= hi
                    ):
                        raise ValueError(
                            f"projective bound escaped: {name}, {point}, {i}, {axis}"
                        )
                    plo, phi = prepared_bounds[i][axis]
                    if (
                        not Fraction(plo) <= e[axis] <= Fraction(phi)
                        or not plo <= a[axis] <= phi
                    ):
                        raise ValueError(
                            f"prepared bound escaped: {name}, {point}, {i}, {axis}"
                        )
                    if abs(Fraction(a[axis]) - e[axis]) > Fraction(errors[i][axis]):
                        raise ValueError(
                            f"roundoff bound escaped: {name}, {point}, {i}, {axis}"
                        )
                    comparisons += 1
        checked.append(
            {
                "name": name,
                "points": len(points),
                "axis_comparisons": comparisons,
                "unknown": False,
            }
        )
    invalid = identity()
    invalid[15], invalid[11] = 0.0, 1.0
    try:
        correlated.project([(-1.0, 1.0)] * 3, invalid, [0.0] * 3, [identity()])
    except ArithmeticError:
        rejected = True
    else:
        raise ValueError("zero-crossing w accepted")
    args.output.write_text(
        json.dumps(
            {
                "cases": checked,
                "zero_crossing_rejected": rejected,
                "success": True,
                "scope": "Finite exact rational and sequential float64 checks under round-to-nearest gradual-underflow model; not a raster proof",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()

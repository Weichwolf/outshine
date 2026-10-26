"""Compare affine foundation extrema on unchanged native footprint fixtures."""

import argparse
import hashlib
import json
import math
import sys
import tempfile
import time
from decimal import Decimal, localcontext
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def sampled(ring, plane):
    values = []
    for p, q in zip(ring, ring[1:] + ring[:1], strict=True):
        steps = 1 + int(math.dist(p, q) / 2.0)
        for step in range(steps):
            fraction = step / steps
            x = p[0] + fraction * (q[0] - p[0])
            y = p[1] + fraction * (q[1] - p[1])
            values.append(plane[0] + plane[1] * x + plane[2] * y)
    return min(values), max(values), len(values)


def corners(ring, plane):
    values = [plane[0] + plane[1] * x + plane[2] * y for x, y in ring]
    return min(values), max(values), len(values)


def exact(ring, plane):
    with localcontext() as context:
        context.prec = 80
        a, b, c = map(Decimal, plane)
        values = [a + b * Decimal(x) + c * Decimal(y) for x, y in ring]
        return min(values), max(values)


def compare(fixture, repeats):
    document = json.loads(fixture.read_text())
    assert document["schema"] == "outshine.hip-plan-reference/1"
    assert len(document["records"]) == 48
    cases = []
    for row in document["records"]:
        ox, oy = row["frame"]["origin_xy_m"]
        local = [(x - ox, y - oy) for x, y in row["outer"]]
        if local[0] == local[-1]:
            local.pop()
        for shift in ((0.0, 0.0), (100000.0, -200000.0)):
            ring = [(x + shift[0], y + shift[1]) for x, y in local]
            for plane in ((0.0, 0.0, 0.0), (12.0, 0.35, -0.22), (-30.0, -0.8, 0.4)):
                cases.append((row["id"], ring, plane))
    old_evaluations = new_evaluations = 0
    largest_change = largest_error = 0.0
    for _, ring, plane in cases:
        old = sampled(ring, plane)
        new = corners(ring, plane)
        oracle = exact(ring, plane)
        scale = max(
            1.0,
            *(abs(plane[0]) + abs(plane[1] * x) + abs(plane[2] * y) for x, y in ring),
        )
        guard = 32 * sys.float_info.epsilon * scale
        change = max(abs(old[i] - new[i]) for i in range(2))
        error = max(float(abs(Decimal(new[i]) - oracle[i])) for i in range(2))
        assert change <= guard and error <= guard
        largest_change = max(largest_change, change)
        largest_error = max(largest_error, error)
        old_evaluations += 2 * old[2]
        new_evaluations += new[2]
    elapsed = {}
    for name, method, passes in (
        ("sampled_twice", sampled, 2),
        ("corners_once", corners, 1),
    ):
        began = time.perf_counter()
        for _ in range(repeats):
            for _, ring, plane in cases:
                for _ in range(passes):
                    method(ring, plane)
        elapsed[name] = (time.perf_counter() - began) / repeats
    return {
        "scope": "Affine boundary extrema only; real footprint shapes, controlled planes. Not native timing, original fitted planes, full asset work, or a GPU/frame-budget proof.",
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "footprints": len(document["records"]),
        "cases": len(cases),
        "old_height_evaluations": old_evaluations,
        "new_height_evaluations": new_evaluations,
        "evaluation_reduction": 1 - new_evaluations / old_evaluations,
        "maximum_range_change_m": largest_change,
        "maximum_corner_error_vs_decimal_m": largest_error,
        "seconds_per_corpus": elapsed,
        "repeats": repeats,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).parent / "reference/hip-plans.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    output = args.output.resolve()
    temporary_roots = (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    if not any(
        output.is_relative_to(path) for path in temporary_roots
    ) or output.is_relative_to(ROOT):
        parser.error(
            "output must be inside the system temp directory, outside the checkout"
        )
    if not 1 <= args.repeats <= 100:
        parser.error("repeats must be between 1 and 100")
    result = compare(args.fixture, args.repeats)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()

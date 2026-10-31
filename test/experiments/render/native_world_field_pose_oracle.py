"""Check native whole-capture World bounds using exact encoded rational operands."""

import argparse
import json
import subprocess
import tempfile
from fractions import Fraction as F
from itertools import product
from pathlib import Path


def dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def cross(a, b):
    return tuple(
        a[(i + 1) % 3] * b[(i + 2) % 3] - a[(i + 2) % 3] * b[(i + 1) % 3]
        for i in range(3)
    )


def vector(values):
    return tuple(F(float(value)) for value in values)


def matrix(values):
    return tuple(
        tuple(F(float(values[column * 4 + row])) for column in range(4))
        for row in range(4)
    )


def samples(case):
    centre, direction = vector(case["centre"]), vector(case["direction"])
    rows = (*map(vector, case["capture"]), direction)
    columns = (
        cross(rows[1], rows[2]),
        cross(rows[2], rows[0]),
        cross(rows[0], rows[1]),
    )
    determinant = dot(rows[0], columns[0])
    extent = F(float(case["extent"]))
    for signs in product((-1, 0, 1), repeat=3):
        point = tuple(
            centre[i]
            + sum(
                columns[j][i] * signs[j] * extent * (2 if j == 2 else 1) / determinant
                for j in range(3)
            )
            for i in range(3)
        )
        height = dot(
            tuple(x - y for x, y in zip(point, centre, strict=True)), direction
        ) / dot(direction, direction)
        yield (
            point,
            tuple(x - height * y for x, y in zip(point, direction, strict=True)),
        )


def project(point, frame, eye, basis, projection):
    world = tuple(dot(row, (*point, F(1))) for row in frame[:3])
    relative = tuple(x - y for x, y in zip(world, eye, strict=True))
    camera = (*[dot(axis, relative) for axis in basis], F(1))
    return tuple(dot(row, camera) for row in projection)


def verify(case):
    frame, projection = matrix(case["frame"]), matrix(case["projection"])
    eye, basis = vector(case["eye"]), tuple(map(vector, case["basis"]))
    bound, total = vector(case["bound"]), F(float(case["l1"]))
    count = 0
    maximum = F(0)
    for original, support in samples(case):
        clips = [
            project(point, frame, eye, basis, projection)
            for point in (original, support)
        ]
        for clip in clips:
            assert clip[3] > 0 and 0 <= clip[2] <= clip[3], (
                "native depth-inside claim contradicted"
            )
        error = tuple(
            abs(F(size, 2) * (clips[0][i] / clips[0][3] - clips[1][i] / clips[1][3]))
            for i, size in enumerate(case["raster"])
        )
        assert all(a <= b for a, b in zip(error, bound, strict=True)), (
            "native axis bound contradicted"
        )
        assert sum(error) <= total, "native L1 bound contradicted"
        maximum = max(maximum, sum(error))
        count += 1
    if "unsafe_l1" in case:
        assert F(float(case["unsafe_l1"])) < maximum, (
            "early Float32 false admission was not reproduced"
        )
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="outshine-world-pose-oracle-") as temporary:
        report = Path(temporary) / "operands.jsonl"
        run = subprocess.run(
            [str(args.binary.resolve()), str(report)],
            capture_output=True,
            text=True,
            check=True,
        )
        cases = [json.loads(line) for line in report.read_text().splitlines()]
        assert len(cases) >= 60, (
            "geographic/tilted/moving perspective and orthographic cases missing"
        )
        assert sum("unsafe_l1" in case for case in cases) == 1, (
            "Float32 counterexample missing"
        )
        samples_checked = sum(verify(case) for case in cases)
        result = {
            "cases": len(cases),
            "exact_source_support_pairs": samples_checked,
            "failures": 0,
            "native_result": run.stdout.strip(),
            "scope": "Exact encoded mathematical projection and complete-domain extrema, no GPU rounding/coverage/material/World admission.",
        }
        if args.output:
            args.output.write_text(
                json.dumps({"result": result, "operands": cases}, indent=2) + "\n"
            )
        print(json.dumps(result))


if __name__ == "__main__":
    main()

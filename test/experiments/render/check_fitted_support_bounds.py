import argparse
import itertools
import json
import subprocess
import tempfile
from fractions import Fraction as F
from pathlib import Path

from native_world_field_pose_oracle import (
    cross,
    dot,
    matrix,
    vector,
)
from native_world_field_pose_oracle import (
    project as project_clip,
)


def add(a, b):
    return [x + y for x, y in zip(a, b, strict=True)]


def subtract(a, b):
    return [x - y for x, y in zip(a, b, strict=True)]


def scale(a, value):
    return [x * value for x in a]


def project(point, case):
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    basis = [
        vector(case["camera_right"]),
        vector(case["camera_up"]),
        scale(vector(case["camera_forward"]), -1),
    ]
    clip = project_clip(
        point,
        matrix(case.get("world_from_field", identity)),
        vector(case["eye"]),
        basis,
        matrix(case["projection"]),
    )
    if not 0 <= clip[2] <= clip[3] or clip[3] <= 0:
        raise AssertionError("native DepthInside does not enclose the exact projection")
    return [clip[i] / clip[3] * F(case["raster"][i]) / 2 for i in range(2)]


def check(case):
    centre = vector(case["centre"])
    direction = vector(case["direction"])
    normal = vector(case["support_normal"])
    support_centre = vector(case["support_centre"])
    rows = [vector(case[key]) for key in ["right", "up", "direction"]]
    columns = [
        cross(rows[1], rows[2]),
        cross(rows[2], rows[0]),
        cross(rows[0], rows[1]),
    ]
    determinant = dot(rows[0], columns[0])
    extent = F(case["half_extent"])
    count = 0
    for x, y, z in itertools.product([-1, 0, 1], [-1, 0, 1], [-2, -1, 0, 1, 2]):
        coordinates = [x * extent, y * extent, z * extent]
        original = add(
            centre,
            [
                sum(columns[j][i] * coordinates[j] / determinant for j in range(3))
                for i in range(3)
            ],
        )
        distance = dot(subtract(original, support_centre), normal) / dot(
            direction, normal
        )
        support = subtract(original, scale(direction, distance))
        a, b = project(original, case), project(support, case)
        error = [abs(a[i] - b[i]) for i in range(2)]
        if any(error[i] > F(case["axis_upper_px"][i]) for i in range(2)) or sum(
            error
        ) > F(case["l1_upper_px"]):
            raise AssertionError(
                "native fitted-support bound is below an exact rational projection"
            )
        count += 1
    return count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(
        prefix="outshine-tests.fitted-support."
    ) as directory:
        operands = Path(directory) / "operands.json"
        subprocess.run(
            [str(args.native.resolve()), "--operands", str(operands)],
            check=True,
            timeout=30,
        )
        cases = json.loads(operands.read_text())
        if len(cases) != 144:
            raise AssertionError("native fitted-support case inventory changed")
        points = sum(check(case) for case in cases)
    print(
        f"exact rational fitted-support projections: {len(cases)} cases, {points} points"
    )


if __name__ == "__main__":
    main()

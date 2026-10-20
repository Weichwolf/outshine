"""Compare native affine course targets with the Python field and a full-pair oracle."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import time
from pathlib import Path

import numpy as np
from river_profiles import RiverProfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def nearest_oracle(segments, points, selected):
    direction = segments[:, 1] - segments[:, 0]
    lengths = np.sum(direction**2, axis=1)
    tests = 0
    for first in range(0, len(points), 64):
        offset = points[first : first + 64, None] - segments[None, :, 0]
        t = np.sum(offset * direction, axis=2) / lengths
        delta = offset - np.clip(t, 0, 1)[:, :, None] * direction
        distance = np.sum(delta**2, axis=2)
        actual = distance[np.arange(len(offset)), selected[first : first + len(offset)]]
        np.testing.assert_allclose(actual, distance.min(axis=1), rtol=1e-12, atol=1e-9)
        tests += distance.size
    return tests


def compare(path, output):
    output.mkdir(parents=True, exist_ok=True)
    inputs = np.load(path, allow_pickle=False)
    segments = inputs["segment_nodes"]
    levels = inputs["segment_levels"]
    points = inputs["boundary"]
    profile = RiverProfile(segments, levels)
    started = time.perf_counter()
    height, references = profile.sample(points, "blend-affine")
    python_ms = (time.perf_counter() - started) * 1000
    if not np.isfinite(height).all():
        raise ValueError("reference input exceeds the native local-reference budget")
    command = shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++23",
        "-O2",
        "-ffp-contract=off",
        "-fno-exceptions",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(ROOT / "include"),
        "-I" + str(ROOT / "src/base/spatial"),
        "-I" + str(ROOT / "src/generators/water"),
        str(HERE / "native_course_profile_probe.cpp"),
        str(ROOT / "src/generators/water/WaterCourseProfile.cpp"),
        str(ROOT / "src/base/spatial/PlanHierarchy.cpp"),
        "-o",
        str(output / "native-course-profile"),
    ]
    with (output / "build.log").open("w") as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    native_input = output / "input.txt"
    with native_input.open("w") as stream:
        stream.write(f"{len(segments)} {len(points)}\n")
        np.savetxt(stream, np.c_[segments.reshape(-1, 4), levels], fmt="%.17g")
        np.savetxt(stream, points, fmt="%.17g")
    with native_input.open() as stream, (output / "native.log").open("w") as log:
        subprocess.run(
            [str(output / "native-course-profile"), str(output / "native.json")],
            stdin=stream,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=10,
        )
    report = json.loads((output / "native.json").read_text())
    samples = np.asarray(report.pop("samples"))
    error = float(np.max(abs(samples[:, 0] - height), initial=0))
    np.testing.assert_allclose(samples[:, 0], height, rtol=0, atol=1e-9)
    np.testing.assert_array_equal(samples[:, 2], references)
    pair_tests = nearest_oracle(segments, points, samples[:, 1].astype(int))
    sources = [
        HERE / "native_course_profile_probe.cpp",
        HERE / "river_profiles.py",
        ROOT / "src/generators/water/WaterCourseProfile.cpp",
        ROOT / "src/generators/water/WaterCourseProfile.h",
        ROOT / "src/base/spatial/PlanHierarchy.cpp",
        ROOT / "src/base/spatial/PlanHierarchy.h",
    ]
    report.update(
        segments=len(segments),
        queries=len(points),
        python_sample_ms=python_ms,
        max_height_difference_m=error,
        exact_reference_counts=True,
        references=int(references.sum()),
        maximum_references=int(references.max(initial=0)),
        nodes_visited=int(samples[:, 3].sum()),
        segment_tests=int(samples[:, 4].sum()),
        full_pair_distance_tests=pair_tests,
        nearest_distance_oracle=True,
        input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        binary_sha256=hashlib.sha256(
            (output / "native-course-profile").read_bytes()
        ).hexdigest(),
        source_sha256={
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
        build_command=command,
        scope="Finite planar target field. No hydrological, engine, GPU or frame admission. "
        "Sample timings exclude output; native persistent memory is not process peak.",
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input", type=Path, default=HERE / "fixtures/feldkirch-ill-profile.npz"
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error("private results belong outside the checkout")
    report = compare(args.input, output)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "segments",
                    "queries",
                    "query_ms",
                    "max_height_difference_m",
                    "segment_tests",
                    "full_pair_distance_tests",
                )
            }
        )
    )


if __name__ == "__main__":
    main()

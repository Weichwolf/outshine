"""Check and time identical native density points using a matching regular archive/API."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_field import Layer
from cloud_shape_field import extinction
from cloud_view_native import INCLUDES

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def points():
    x, z, h = np.meshgrid(
        np.linspace(-22400, 22400, 128),
        np.linspace(-22400, 22400, 128),
        np.linspace(0.01, 0.99, 64),
        indexing="ij",
    )
    radius = 6360000.0
    height = 2572 + 600 * h
    square = x * x + z * z
    raised = 2 * radius * height + height * height - square
    y = raised / (np.sqrt(radius * radius + raised) + radius)
    return np.stack((x, y, z, np.zeros_like(x)), axis=-1).reshape(-1, 4).astype("<f4")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--untimed", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").unlink(missing_ok=True)
    archive_root = args.archive_root.resolve()
    record = {"commands": []}

    def run(command, name, cwd=ROOT):
        command = list(map(str, command))
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=180, check=False
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": command, "cwd": str(cwd), "exit_code": result.returncode}
        )
        if result.returncode or (name.startswith("native-") and result.stderr):
            raise RuntimeError(
                f"{name} failed or emitted validation diagnostics: {output}"
            )
        return result.stdout

    producer = run(["git", "rev-parse", "HEAD"], "producer", archive_root).strip()
    if run(
        ["git", "status", "--porcelain", "--", "src", "include"],
        "producer-clean",
        archive_root,
    ):
        raise RuntimeError("regular archive production inputs must remain clean")
    archive = archive_root / "build/liboutshine.a"
    if not archive.is_file():
        parser.error("build the complete regular production archive first")
    link = shlex.split(
        run(["sh", "test/run.sh", "--native-link-flags"], "native-link", archive_root)
    )
    binary = output / "cloud-density-native"
    run(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("CXX", "c++")),
            "-std=c++23",
            "-O2",
            "-ffp-contract=off",
            "-fno-exceptions",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wshadow",
            "-Werror",
            "-Wno-unused-parameter",
            *(
                f"-I{archive_root / path if path.startswith(('src', 'include')) else ROOT / path}"
                for path in INCLUDES
            ),
            ROOT / "test/experiments/weather/cloud_density_native.cpp",
            archive,
            *link,
            "-o",
            binary,
        ],
        "build",
    )
    shader = output / "cloud-density.spv"
    run(
        [
            *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
            "-V",
            "-Isrc",
            "-Isrc/generators/weather/shaders",
            ROOT / "test/experiments/weather/cloud_density.comp",
            "-o",
            shader,
        ],
        "shader",
    )
    positions = points()
    rows = []
    for cover in (0.0, 0.45, 0.65, 1.0):
        for profile, kind in ((0, "stratus"), (1, "cell-smooth")):
            name = f"{profile}-{cover:.2f}"
            frame = np.array(
                [
                    [6360000, 2572, 600, 0.012],
                    [cover, profile, 0, 0],
                    [256.3435, 0, -148, 0],
                ],
                dtype="<f4",
            )
            path = output / f"{name}.input.bin"
            with path.open("wb") as stream:
                stream.write(frame.tobytes())
                stream.write(positions.tobytes())
            run(
                [
                    binary,
                    shader,
                    path,
                    output / f"{name}.output.bin",
                    output / f"{name}.times.json",
                    *(["untimed"] if args.untimed else []),
                ],
                f"native-{name}",
            )
            layer = Layer(
                radius_m=float(frame[0, 0]),
                base_m=float(frame[0, 1]),
                thickness_m=float(frame[0, 2]),
                extinction_per_m=float(frame[0, 3]),
                cover=float(frame[1, 0]),
            )
            expected = extinction(
                positions[:, :3].astype(float),
                layer,
                0,
                kind,
                drift_m=frame[2, :3].astype(float),
            )
            actual = np.fromfile(output / f"{name}.output.bin", "<f4")
            error = np.abs(actual - expected) / layer.extinction_per_m
            if (
                len(actual) != len(positions)
                or not np.all(np.isfinite(actual))
                or np.max(error) > 1e-4
            ):
                raise AssertionError(
                    f"{name}: normalized density error={np.max(error)}"
                )
            times = json.loads((output / f"{name}.times.json").read_text())
            durations = [row["elapsed_ns"] for batch in times["runs"] for row in batch]
            rows.append(
                {
                    "profile": profile,
                    "cover": cover,
                    "points": len(actual),
                    "max_normalized_density_error": float(np.max(error)),
                    "input_sha256": digest(path),
                    "timing_pairs": len(durations),
                    "gpu_dispatch_p50_ns": float(np.median(durations))
                    if durations
                    else None,
                    "gpu_dispatch_max_ns": max(durations) if durations else None,
                }
            )
            print(json.dumps(rows[-1]), flush=True)
    record.update(
        producer_commit=producer,
        archive_sha256=digest(archive),
        binary_sha256=digest(binary),
        shader_sha256=digest(shader),
        cases=rows,
        point_stride_bytes=16,
        output_stride_bytes=4,
        uniform_bytes=48,
        gpu_buffers_per_recording_bytes=len(positions) * (16 + 4 + 4 + 16),
        cpu_input_copy_per_recording_bytes=len(positions) * 16,
        cpu_output_copy_per_recording_bytes=len(positions) * 4,
        scope="Actual isolated density dispatches, no cloud/source/frame/p99 or device-profile acceptance",
    )
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")


if __name__ == "__main__":
    main()

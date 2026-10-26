"""Compare native sorted-pair and count/prefix/scatter light-list construction (#448)."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import export, inputs
from local_light_grid import build_grid
from local_light_inputs import ROOT

CASES = (
    ("small", "small", "street", 16 * 1024**2),
    ("street", "street", "street", 16 * 1024**2),
    ("dense", "dense", "street", 16 * 1024**2),
    ("overlap", "overlap", "street", 16 * 1024**2),
    ("overflow", "overlap", "street", 4096),
    ("turn", "street", "turn", 16 * 1024**2),
    ("ortho", "street", "ortho", 16 * 1024**2),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(folder, scene, view, budget):
    export(folder, scene, True, view, budget)
    lights, camera, packed, _, _ = inputs(scene, True, view)
    np.r_[
        camera.basis.ravel(), camera.eye, camera.half, camera.near, camera.depth_extent
    ].astype("<f8").tofile(folder / "builder-camera.bin")
    np.array(
        [camera.width, camera.height, int(camera.perspective), 64], dtype="<u4"
    ).tofile(folder / "builder-grid.bin")
    for mode in ("tiles", "log"):
        build_grid(lights, camera, mode, index_budget=budget).edges.astype(
            "<f8"
        ).tofile(folder / f"{mode}-builder-edges.bin")
    packed[:, 1, :3] = (lights.positions - camera.eye).astype("<f4")
    return packed


def compare(binary, folder, expected_packed, mode, method, budget, repeats, suffix=""):
    prefix = folder / f"native-{mode}-{method}{suffix}"
    argv = [
        str(binary),
        str(folder),
        str(prefix),
        method,
        mode,
        str(budget),
        str(repeats),
    ]
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=180, check=False
    )
    prefix.with_suffix(".stdout").write_text(result.stdout)
    prefix.with_suffix(".stderr").write_text(result.stderr)
    receipt = {"argv": argv, "exit": result.returncode}
    if result.returncode or result.stderr:
        raise AssertionError(receipt | {"stderr": result.stderr})
    actual = Path(str(prefix) + ".words.bin")
    expected = folder / f"{mode}-lists.bin"
    if actual.read_bytes() != expected.read_bytes():
        a, b = np.fromfile(actual, "<u4"), np.fromfile(expected, "<u4")
        raise AssertionError((mode, method, a.shape, b.shape, int(np.sum(a != b))))
    packed = Path(str(prefix) + ".repacked.bin")
    if packed.read_bytes() != expected_packed.tobytes():
        raise AssertionError(
            "all native camera-relative light fields must match FP64 packing"
        )
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    construction = [row for row in rows if "build_ms" in row]
    repack = [row["repack_ms"] for row in rows if "repack_ms" in row]
    warm = construction[1:]
    if any(row["own_allocations"] for row in warm):
        raise AssertionError("unchanged-capacity native rebuild must not allocate")
    return receipt | {
        "words_sha256": digest(actual),
        "repacked_sha256": digest(packed),
        "cold": construction[0],
        "warm_ms_p50_p95_p99": np.percentile(
            [r["build_ms"] for r in warm], [50, 95, 99]
        ).tolist(),
        "repack_ms_p50_p95_p99": np.percentile(repack[1:], [50, 95, 99]).tolist(),
        "warm_allocations": 0,
        "samples": repeats,
    }


def negatives(binary, folder):
    rows = []
    original = [
        binary,
        folder,
        folder / "negative-output",
        "hierarchy",
        "log",
        "4096",
        "3",
    ]
    for name, position, value in (
        ("missing", 1, folder / "missing"),
        ("method", 3, "unknown"),
        ("mode", 4, "unknown"),
        ("budget-zero", 5, "0"),
        ("budget-signed", 5, "-1"),
        ("repeats-zero", 6, "0"),
    ):
        argv = list(map(str, original))
        argv[position] = str(value)
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=30, check=False
        )
        if result.returncode != 2:
            raise AssertionError((name, result.returncode))
        rows.append({"name": name, "argv": argv, "exit": result.returncode})
    for name, file, dtype, index, value in (
        ("nonfinite-camera", "builder-camera.bin", "<f8", 0, np.nan),
        ("zero-half", "builder-camera.bin", "<f8", 12, 0),
        ("descending-edges", "log-builder-edges.bin", "<f8", 1, 0),
        ("wrong-profile", "builder-grid.bin", "<u4", 0, 1),
        ("negative-range", "lights.bin", "<f4", 7, -1),
        ("unknown-kind", "lights.bin", "<f4", 3, 3),
    ):
        path = folder / file
        saved = path.read_bytes()
        data = np.frombuffer(saved, dtype).copy()
        data[index] = value
        snapshot = folder / f"{name}.input.bin"
        data.tofile(snapshot)
        try:
            data.tofile(path)
            result = subprocess.run(
                list(map(str, original)),
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
        finally:
            path.write_bytes(saved)
        if result.returncode != 2:
            raise AssertionError((name, result.returncode, result.stderr))
        rows.append(
            {
                "name": name,
                "mutated_file": file,
                "input_sha256": digest(snapshot),
                "exit": result.returncode,
            }
        )
    (folder.parent / "negative-controls.json").write_text(
        json.dumps(rows, indent=2) + "\n"
    )


def direction_range(binary, folder):
    path = folder / "lights.bin"
    saved = path.read_bytes()
    packed = np.frombuffer(saved, "<f4").reshape(-1, 4, 4).copy()
    packed[1, 1, 3] = 1 / 32
    eye = np.fromfile(folder / "builder-camera.bin", "<f8")[9:12]
    expected = packed.copy()
    expected[:, 1, :3] = (packed[:, 1, :3].astype(float) - eye).astype("<f4")
    rows = []
    try:
        packed.tofile(folder / "direction-range.input.bin")
        packed.tofile(path)
        for mode in ("tiles", "log"):
            row = compare(
                binary,
                folder,
                expected,
                mode,
                "hierarchy",
                16 * 1024**2,
                3,
                "-direction-range",
            )
            rows.append({"mode": mode, **row})
    finally:
        path.write_bytes(saved)
    (folder.parent / "direction-range-controls.json").write_text(
        json.dumps(rows, indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=101)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT) or not 3 <= args.repeats <= 1000:
        parser.error("private output and 3..1000 repetitions required")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, scene, view, budget in CASES:
        folder = args.output / name
        expected = prepare(folder, scene, view, budget)
        if name == "small":
            negatives(args.binary, folder)
        for mode in ("tiles", "log"):
            for method in ("sort", "scatter", "hierarchy"):
                result = compare(
                    args.binary, folder, expected, mode, method, budget, args.repeats
                )
                rows.append({"case": name, "mode": mode, "method": method, **result})
                (args.output / "results.json").write_text(
                    json.dumps(rows, indent=2) + "\n"
                )
                print(name, mode, method, result["warm_ms_p50_p95_p99"], flush=True)
        if name == "small":
            direction_range(args.binary, folder)


if __name__ == "__main__":
    main()

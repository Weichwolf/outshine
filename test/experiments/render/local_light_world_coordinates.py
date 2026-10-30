"""Check actual GPU cell-local rebasing against independent FP64 camera subtraction."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT
from local_light_world import cell_positions


def capture(consumer, shader, output, positions, eye, width):
    cells, local = cell_positions(positions, width)
    camera_cells, camera_locals = cell_positions(eye[None], width)
    camera_cell, camera_local = camera_cells[0], camera_locals[0]
    reference = positions - eye
    points = np.zeros((positions.size, 4), dtype="<u4")
    points[:, 0] = local.ravel().view("<u4")
    points[:, 1] = cells.ravel().view("<u4")
    points[:, 2] = np.tile(np.arange(3, dtype="<u4"), len(positions))
    points[:, 3] = reference.astype("<f4").ravel().view("<u4")
    frame = np.zeros(12, dtype="<f4")
    frame.view("<u4")[:3] = camera_cell.view("<u4")
    frame[4:8] = [width, *camera_local]
    frame.view("<u4")[8] = len(points)
    source, result = output.with_suffix(".input.bin"), output.with_suffix(".error.bin")
    source.write_bytes(frame.tobytes() + points.tobytes())
    argv = [
        str(consumer),
        str(shader),
        str(source),
        str(result),
        str(output.with_suffix(".times.json")),
        "untimed",
    ]
    run = subprocess.run(argv, capture_output=True, text=True, timeout=120, check=False)
    output.with_suffix(".log").write_text(run.stdout + run.stderr)
    if run.returncode:
        raise AssertionError((argv, run.returncode, run.stderr))
    error = np.fromfile(result, "<f4").reshape(-1, 3).astype(float)
    unit = np.finfo(np.float32).eps / 2
    gamma = 4 * unit / (1 - 4 * unit)
    local_exact = positions - cells.astype(float) * width
    eye_exact = eye - camera_cell.astype(float) * width
    quantization = abs(local.astype(float) - local_exact) + abs(
        camera_local.astype(float) - eye_exact
    )
    bound = gamma * (
        abs((cells.astype(float) - camera_cell) * width)
        + abs(local.astype(float))
        + abs(camera_local.astype(float))
    )
    bound += quantization + abs(reference - reference.astype("<f4").astype(float))
    failed = not np.all(np.isfinite(error)) or np.any(abs(error) > bound)
    return {
        "argv": argv,
        "exit": run.returncode,
        "failed": bool(failed),
        "axes": positions.size,
        "max_error_to_direct_fp32_m": float(np.max(abs(error))),
        "max_bound_m": float(np.max(bound)),
        "nonzero_axes": int(np.count_nonzero(error)),
        "input_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "output_sha256": hashlib.sha256(result.read_bytes()).hexdigest(),
        "eye_world_m": eye.tolist(),
        "width_m": width,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--consumer", type=Path, required=True)
    parser.add_argument("--shader", type=Path, required=True)
    parser.add_argument("--bad-shader", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    _, camera, packed, _, _ = inputs("dense", True, "street")
    positions = packed[:, 1, :3].astype(float)
    rows = []
    for width in (16, 32, 64, 128):
        for translated in (False, True):
            translation = (
                np.array([6_000_000, -4_000_000, 3_200_000])
                if translated
                else np.zeros(3)
            )
            base = camera.eye + translation
            boundary = np.floor(base / width) * width + [width, width / 2, width / 3]
            for step in (None, -1 / 60, 1 / 60, 1 / 30):
                eye = base if step is None else boundary + [step, -step, step]
                row = capture(
                    args.consumer,
                    args.shader,
                    args.output / f"case-{len(rows)}",
                    positions + translation,
                    eye,
                    width,
                )
                if row["failed"]:
                    raise AssertionError(row)
                rows.append(row)
                (args.output / "results.json").write_text(
                    json.dumps(rows, indent=2) + "\n"
                )
                print(
                    width,
                    translated,
                    step,
                    row["max_error_to_direct_fp32_m"],
                    flush=True,
                )
    negative = capture(
        args.consumer,
        args.bad_shader,
        args.output / "bad-camera",
        positions,
        camera.eye,
        32,
    )
    (args.output / "negative.json").write_text(json.dumps(negative, indent=2) + "\n")
    if not negative["failed"]:
        raise AssertionError(
            "wrong camera cell must violate independent FP64-derived bounds"
        )


if __name__ == "__main__":
    main()

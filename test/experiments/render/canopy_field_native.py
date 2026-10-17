"""Run spatial canopy coefficients through the existing native material probe."""

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from canopy_field import mean_cells

ROOT = Path(__file__).resolve().parents[3]
PROBE = np.dtype(
    [("point", "<f4", 4), ("dy", "<f4", 2), ("cell", "<i4"), ("state", "<u4")]
)
assert PROBE.itemsize == 32
assert [PROBE.fields[k][1] for k in PROBE.names] == [0, 16, 24, 28]


def prepare(prepared, output, repetitions):
    report = json.loads((prepared / "result.json").read_text())
    with np.load(prepared / "coefficients.npz", allow_pickle=False) as data:
        levels = [data[f"level_{i}"] for i in range(len(data.files))]
    with np.load(prepared / "frame-diagnostics.npz", allow_pickle=False) as data:
        materials, energies, reference = (
            data[k] for k in ("materials", "energies", "reference")
        )
    assert len(report["directions"]) == 6 and all(
        a.shape[2:] == (6, 12) for a in levels
    )
    assert all(np.isfinite(a).all() for a in levels)
    sizes = [len(a) for a in levels]
    cells = sum(n * n for n in sizes)
    coefficients = np.concatenate([a.reshape(-1, 6, 12) for a in levels])
    header = np.zeros(32, "<f4")
    header.view("<u4")[:3] = 1, cells, len(materials)
    states = np.concatenate(
        (materials.reshape(len(materials), 9), energies.reshape(len(materials), 18)),
        axis=1,
    )
    parameters = np.concatenate((header, coefficients.ravel(), states.ravel())).astype(
        "<f4"
    )
    parameters.tofile(output / "parameters.f32")
    if (len(materials) * cells * repetitions + 2) * PROBE.itemsize > 64 * 1024 * 1024:
        raise ValueError("query data exceeds the native harness64MiB input bound")
    probes = np.zeros(len(materials) * cells * repetitions + 2, PROBE)
    probes["cell"][:-2] = np.tile(np.arange(cells), len(materials) * repetitions)
    probes["state"][:-2] = np.tile(
        np.repeat(np.arange(len(materials)), cells), repetitions
    )
    probes["cell"][-2:] = -1, cells
    probes.tofile(output / "probes.bin")
    expected = np.stack(
        [
            np.concatenate([mean_cells(r, n).reshape(-1, 6) for n in sizes])
            for r in reference
        ]
    )
    expected = np.tile(expected, (repetitions, 1, 1))
    np.savez(output / "reference.npz", values=expected)
    return report, sizes, expected, parameters.nbytes, probes.nbytes


def run(prepared, binary, output, timed, repetitions):
    output = output.resolve()
    if (
        output.exists()
        or not 1 <= repetitions <= 19
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output.is_relative_to(ROOT)
    ):
        raise ValueError("fresh explicit system-temp output outside checkout required")
    output.mkdir(parents=True)
    report, sizes, expected, parameter_bytes, probe_bytes = prepare(
        prepared, output, repetitions
    )
    shader = ROOT / "test/experiments/render/canopy_field_probe.comp"
    command = [
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
        "-S",
        "comp",
        str(shader),
        "-o",
        str(output / "probe.spv"),
    ]
    with (output / "compile.log").open("w") as log:
        subprocess.run(
            ["nice", "-n", "10", *command],
            check=True,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
    command = [
        str(binary.resolve()),
        str(output / "probe.spv"),
        str(output / "parameters.f32"),
        str(output / "probes.bin"),
        str(output / "result.f32"),
        "0",
    ]
    if timed:
        command.append("--gpu-timing")
    with (output / "native.log").open("w") as log:
        result = subprocess.run(
            command, check=False, stdout=log, stderr=subprocess.STDOUT, timeout=60
        )
    if result.returncode:
        raise RuntimeError(
            f"native canopy probe exited {result.returncode}; see native.log"
        )
    actual = np.fromfile(output / "result.f32", "<f4").reshape(-1, 8)
    assert np.isfinite(actual).all()
    np.testing.assert_array_equal(actual[:, [3, 7]], 0)
    np.testing.assert_array_equal(actual[-2:], 0)
    actual = actual[:-2, [0, 1, 2, 4, 5, 6]].reshape(expected.shape)
    component_error = float(np.max(abs(actual - expected)))
    total_error = float(
        np.max(
            abs(
                actual[..., :3]
                + actual[..., 3:]
                - expected[..., :3]
                - expected[..., 3:]
            )
        )
    )
    error = max(component_error, total_error)
    assert error <= 1e-6, error
    black = report["states"].index("control-0")
    np.testing.assert_array_equal(
        actual.reshape(repetitions, len(report["states"]), -1, 6)[:, black], 0
    )
    np.savez(output / "frame-diagnostics.npz", actual=actual, reference=expected)
    result = {
        "exit": result.returncode,
        "gpu_executed": True,
        "command": command,
        "max_abs_error": error,
        "component_error": component_error,
        "total_rgb_error": total_error,
        "query_repetitions": repetitions,
        "limit": 1e-6,
        "states": report["states"],
        "sizes": sizes,
        "queries": len(actual) * actual.shape[1],
        "invalid_queries": 2,
        "parameter_bytes": parameter_bytes,
        "probe_bytes": probe_bytes,
        "buffer_bytes": 2 * parameter_bytes + 4 * probe_bytes,
        "copy_bytes": parameter_bytes + 2 * probe_bytes,
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "shader_sha256": hashlib.sha256(shader.read_bytes()).hexdigest(),
        "timing_scope": "Existing driver:32 dispatches plus upload/readback batch when requested; see native.log",
        "scope": "Native fixed-view spatial D+KD probe, all stored mips; not world shader integration or Places acceptance",
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gpu-timing", action="store_true")
    parser.add_argument("--repeat-queries", type=int, default=1)
    args = parser.parse_args()
    run(args.prepared, args.binary, args.output, args.gpu_timing, args.repeat_queries)

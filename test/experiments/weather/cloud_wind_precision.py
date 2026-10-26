"""Reproduce native long-phase sticking and compare bounded lattice coordinates."""

import argparse
import hashlib
import json
import subprocess
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from cloud_field import Layer
from cloud_shape_field import extinction
from cloud_wind_coordinates import density

ROOT = Path(__file__).resolve().parents[3]
KINDS = ("direct", "modulo-hour", "split")
MONTH_S = 32 * 86400.0
VELOCITY = np.array([8.0, 0.0, 0.0])


def points(count=4096):
    rng = np.random.default_rng(191)
    xz = rng.uniform(-22400, 22400, (count, 2))
    heights = 2572 + 600 * rng.uniform(0.1, 0.9, count)
    radius = 6360000.0
    raised = 2 * radius * heights + heights * heights - np.sum(xz * xz, axis=1)
    y = raised / (np.sqrt(radius * radius + raised) + radius)
    return np.c_[xz[:, 0], y, xz[:, 1], np.zeros(count)].astype("<f4")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native(folder, native_root, positions):
    binary = native_root / "cloud-density-native"
    shader = native_root / "cloud-density.spv"
    receipt = json.loads((native_root / "receipt.json").read_text())
    if (
        digest(binary) != receipt["binary_sha256"]
        or digest(shader) != receipt["shader_sha256"]
    ):
        raise ValueError("the supplied native files must match their original receipt")
    captures = []
    rows = []
    for index, phase in enumerate((MONTH_S, MONTH_S + 1 / 60, MONTH_S + 1 / 4)):
        drift = VELOCITY * phase
        frame = np.array(
            [[6360000, 2572, 600, 0.012], [0.65, 1, 0, 0], [*drift, 0]], dtype="<f4"
        )
        path = folder / f"native-{index}.input.bin"
        path.write_bytes(frame.tobytes() + positions.tobytes())
        target = folder / f"native-{index}.density.bin"
        argv = [
            binary,
            shader,
            path,
            target,
            folder / f"native-{index}.times.json",
            "untimed",
        ]
        result = subprocess.run(
            list(map(str, argv)),
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
        (folder / f"native-{index}.stdout").write_text(result.stdout)
        (folder / f"native-{index}.stderr").write_text(result.stderr)
        if result.returncode or result.stderr:
            raise RuntimeError(
                "the actual unchanged native control failed or emitted diagnostics"
            )
        captures.append(np.fromfile(target, "<f4"))
        rows.append(
            {
                "phase_s": phase,
                "requested_drift_m": drift.tolist(),
                "packed_drift_m": frame[2, :3].astype(float).tolist(),
                "argv": list(map(str, argv)),
                "exit_code": result.returncode,
                "input_sha256": digest(path),
                "output_sha256": digest(target),
            }
        )
    return rows, captures


def compare(folder, positions):
    rows = []
    for profile, reference in ((0, "stratus"), (1, "cell-smooth")):
        for cover in (0.0, 0.45, 0.65, 1.0):
            layer = Layer(cover=cover)
            for seconds in (0.0, 37.0, MONTH_S, -MONTH_S, 315576000.0, 1792022400.0):
                expected = extinction(
                    positions, layer, seconds, reference, drift_m=VELOCITY * seconds
                )
                for kind in KINDS:
                    began = time.perf_counter()
                    actual = density(positions, layer, VELOCITY, seconds, kind, profile)
                    elapsed = time.perf_counter() - began
                    error = float(
                        np.max(np.abs(actual - expected)) / layer.extinction_per_m
                    )
                    if kind == "split" and error > 1e-4:
                        raise AssertionError(
                            f"split lattice did not preserve the independent field: {error}"
                        )
                    rows.append(
                        {
                            "profile": profile,
                            "cover": cover,
                            "phase_s": seconds,
                            "kind": kind,
                            "max_normalized_density_error": error,
                            "python_elapsed_s": elapsed,
                        }
                    )
    memory = []
    for kind in KINDS:
        tracemalloc.start()
        density(positions, Layer(cover=0.65), VELOCITY, MONTH_S, kind, 1)
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        memory.append({"kind": kind, "python_tracemalloc_peak_bytes": peak})
    (folder / "comparison.json").write_text(
        json.dumps(
            {
                "rows": rows,
                "memory": memory,
                "scope": "Simulated FP32 coordinate/density algebra; physical height shared in FP64. Python times/peaks are not GPU or native costs.",
            },
            indent=2,
        )
        + "\n"
    )
    return rows


def report(folder, positions, captures, rows):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    layer = Layer(cover=0.65)
    first = extinction(
        positions, layer, MONTH_S, "cell-smooth", drift_m=VELOCITY * MONTH_S
    )
    next_frame = extinction(
        positions,
        layer,
        MONTH_S + 1 / 60,
        "cell-smooth",
        drift_m=VELOCITY * (MONTH_S + 1 / 60),
    )
    selected = int(np.argmax(abs(next_frame - first)))
    times = np.arange(61) / 60
    point = positions[selected : selected + 1]
    curves = {
        kind: [
            float(density(point, layer, VELOCITY, MONTH_S + step, kind, 1)[0])
            for step in times
        ]
        for kind in ("direct", "split")
    }
    truth = [
        float(
            extinction(
                point,
                layer,
                MONTH_S + step,
                "cell-smooth",
                drift_m=VELOCITY * (MONTH_S + step),
            )[0]
        )
        for step in times
    ]
    with PdfPages(folder / "wind-precision.pdf") as pdf:
        fig, axis = plt.subplots(figsize=(11, 6), layout="constrained")
        axis.plot(times, truth, label="Independent FP64 field")
        for kind, values in curves.items():
            axis.plot(times, values, label=f"{kind}: simulated FP32 coordinates")
        axis.scatter(
            [0, 1 / 60, 1 / 4],
            [float(values[selected]) for values in captures],
            marker="x",
            color="black",
            label="Actual unchanged native density",
        )
        axis.set(
            xlabel="Seconds after32 days",
            ylabel="Extinction per metre",
            title="8m/s wind: native FP32 drift sticks, then jumps; split candidate remains continuous",
        )
        axis.legend()
        pdf.savefig(fig)
        plt.close(fig)
        fig, axis = plt.subplots(figsize=(11, 6), layout="constrained")
        phases = [37.0, MONTH_S, 315576000.0, 1792022400.0]
        for kind in KINDS:
            errors = [
                max(
                    row["max_normalized_density_error"]
                    for row in rows
                    if row["kind"] == kind and row["phase_s"] == phase
                )
                for phase in phases
            ]
            axis.plot(range(len(phases)), errors, "o-", label=kind)
        axis.axhline(
            1e-4, color="black", linestyle="--", label="Existing normalized tolerance"
        )
        axis.set(
            yscale="log",
            xticks=range(len(phases)),
            xticklabels=["37s", "32days", "10years", "1792022400s"],
            ylabel="Maximum normalized density error",
            title="Both profiles and all four covers; original field identity retained",
        )
        axis.legend()
        pdf.savefig(fig)
        plt.close(fig)
    return {
        "selected_point": selected,
        "native_first_frame_identical": bool(np.array_equal(captures[0], captures[1])),
        "reference_changed_points": int(np.count_nonzero(first != next_frame)),
        "reference_first_frame_max_delta": float(np.max(abs(first - next_frame))),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    folder = args.output.resolve()
    if not folder.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    folder.mkdir(parents=True, exist_ok=True)
    source = points()
    np.save(folder / "original-points.npy", source)
    positions = source[:, :3].astype(float)
    native_rows, captures = native(folder, args.native_root.resolve(), source)
    rows = compare(folder, positions)
    outcome = report(folder, positions, captures, rows)
    record = {
        "native": native_rows,
        "comparison_rows": len(rows),
        "outcome": outcome,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_dirty": subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True
        ),
        "source_sha256": {
            name: digest(ROOT / name)
            for name in (
                "test/experiments/weather/cloud_wind_precision.py",
                "test/experiments/weather/cloud_wind_coordinates.py",
                "test/experiments/weather/cloud_field.py",
                "test/experiments/weather/cloud_shape_field.py",
            )
        },
        "native_receipt": str(args.native_root.resolve() / "receipt.json"),
        "native_receipt_sha256": digest(args.native_root.resolve() / "receipt.json"),
        "scope": "Verified native counterexample and independent Python candidates; no candidate GPU/layout/default, frame or device acceptance",
    }
    (folder / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(outcome))


if __name__ == "__main__":
    main()

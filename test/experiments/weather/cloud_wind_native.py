"""Measure a private native split-lattice candidate against the unchanged field."""

import argparse
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_density_native import build_native, digest
from cloud_field import Layer
from cloud_shape_field import extinction
from cloud_wind_precision import MONTH_S, VELOCITY, points

ROOT = Path(__file__).resolve().parents[3]
PHASES = (0.0, 37.0, MONTH_S, -MONTH_S, 315576000.0, 1792022400.0)


def frame(cover, profile, velocity, phase, origin=None, split=True):
    origin = np.zeros(3) if origin is None else np.asarray(origin, dtype=float)
    result = np.zeros((7 if split else 3, 4), dtype="<f4")
    result[0] = [6360000, 2572, 600, 0.012]
    result[1, :2] = [cover, profile]
    if not split:
        result[2, :3] = velocity * phase
        return result
    result[2, :3] = origin
    for index, frequency in ((3, 1.0), (5, 3.1)):
        offset = (origin - velocity * phase) / 1400.0 * frequency
        base = np.floor(offset).astype(np.int64)
        result.view("<u4")[index, :3] = base.astype("<u4")
        result[index + 1, :3] = offset - base
    return result


def reference(positions, payload, velocity, phase):
    layer = Layer(cover=float(payload[1, 0]), extinction_per_m=float(payload[0, 3]))
    return extinction(
        positions[:, :3].astype(float),
        layer,
        phase,
        "cell-smooth" if payload[1, 1] else "stratus",
        drift_m=velocity * phase,
    )


class Probe:
    def __init__(self, output):
        self.output = output
        self.commands = []
        self.captures = []
        self.binary = output / "cloud-density-native"
        self.split_shader = output / "wind-split.spv"
        self.direct_shader = output / "cloud-density.spv"

    def run(self, command, name, cwd=ROOT):
        command = list(map(str, command))
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=180, check=False
        )
        (self.output / f"{name}.stdout").write_text(result.stdout)
        (self.output / f"{name}.stderr").write_text(result.stderr)
        self.commands.append(
            {"argv": command, "cwd": str(cwd), "exit_code": result.returncode}
        )
        if result.returncode or (name.startswith("native-") and result.stderr):
            raise RuntimeError(f"{name} failed or emitted diagnostics: {self.output}")
        return result.stdout

    def capture(self, name, payload, positions, timed=False):
        path = self.output / f"{name}.input.bin"
        path.write_bytes(payload.tobytes() + positions.tobytes())
        return self.capture_file(name, path, payload.nbytes != 48, timed)

    def capture_file(self, name, path, split, timed=False):
        target = self.output / f"{name}.density.bin"
        times_path = self.output / f"{name}.times.json"
        self.run(
            [
                self.binary,
                self.split_shader if split else self.direct_shader,
                path,
                target,
                times_path,
                "timed" if timed else "untimed",
                *(["28"] if split else []),
            ],
            f"native-{name}",
        )
        actual = np.fromfile(target, "<f4")
        if not np.all(np.isfinite(actual)):
            raise AssertionError("native densities must remain finite")
        timing = json.loads(times_path.read_text())
        self.captures.append(
            {
                "name": name,
                "input": str(path),
                "input_sha256": digest(path),
                "output_sha256": digest(target),
                "points": len(actual),
                "uniform_bytes": 112 if split else 48,
                "timing": timing,
            }
        )
        return actual


def check_density(actual, expected, payload):
    if actual.shape != expected.shape:
        raise AssertionError("all original density points must be captured")
    error = float(np.max(abs(actual - expected)) / float(payload[0, 3]))
    if error > 1e-4:
        raise AssertionError(f"native split field changed: {error}")
    return error


def field_cases(probe, positions, pack=frame):
    rows = []
    for profile in (0, 1):
        for cover in (0.0, 0.45, 0.65, 1.0):
            for phase in PHASES:
                payload = pack(cover, profile, VELOCITY, phase)
                actual = probe.capture(f"field-{len(rows)}", payload, positions)
                expected = reference(positions, payload, VELOCITY, phase)
                rows.append(
                    {
                        "profile": profile,
                        "cover": cover,
                        "phase_s": phase,
                        "max_normalized_density_error": check_density(
                            actual, expected, payload
                        ),
                    }
                )
    return rows


def temporal_cases(probe, positions, phases=PHASES[2:], prefix="step", pack=frame):
    rows = []
    for profile in (0, 1):
        for velocity in (VELOCITY, -VELOCITY, np.array([6.92820323027551, 0, -4])):
            for phase in phases:
                payload = pack(0.65, profile, velocity, phase)
                name = f"{prefix}-{len(rows)}"
                first = probe.capture(f"{name}-first", payload, positions)
                truth = reference(positions, payload, velocity, phase)
                check_density(first, truth, payload)
                for rate in (30, 60):
                    next_phase = phase + 1 / rate
                    following = pack(0.65, profile, velocity, next_phase)
                    actual = probe.capture(f"{name}-{rate}", following, positions)
                    expected = reference(positions, following, velocity, next_phase)
                    error = check_density(actual, expected, following)
                    actual_step = actual.astype(float) - first
                    expected_step = expected - truth
                    relative = float(
                        np.linalg.norm(actual_step - expected_step)
                        / np.linalg.norm(expected_step)
                    )
                    changed = int(np.count_nonzero(actual_step))
                    if not changed or relative > 0.05:
                        raise AssertionError(f"wind step did not progress: {relative}")
                    rows.append(
                        {
                            "profile": profile,
                            "velocity_ms": velocity.tolist(),
                            "phase_s": phase,
                            "rate_hz": rate,
                            "changed_points": changed,
                            "reference_changed_points": int(
                                np.count_nonzero(expected_step)
                            ),
                            "relative_step_l2_error": relative,
                            "max_normalized_density_error": error,
                        }
                    )
    return rows


def anchor_cases(probe, positions, pack=frame):
    rows = []
    for profile in (0, 1):
        for origin in ([768, -512, 1024], [-1400, 2800, -4200], [22400, 0, -22400]):
            for phase in (MONTH_S, -MONTH_S):
                payload = pack(0.65, profile, VELOCITY, phase, origin)
                actual = probe.capture(f"anchor-{len(rows)}", payload, positions)
                expected = reference(positions, payload, VELOCITY, phase)
                rows.append(
                    {
                        "profile": profile,
                        "coordinate_origin_m": origin,
                        "phase_s": phase,
                        "max_normalized_density_error": check_density(
                            actual, expected, payload
                        ),
                    }
                )
    return rows


def default_cases(probe, control_root):
    receipt = json.loads((control_root / "receipt.json").read_text())
    for name, key in (
        ("cloud-density-native", "binary_sha256"),
        ("cloud-density.spv", "shader_sha256"),
    ):
        if digest(control_root / name) != receipt[key]:
            raise ValueError("the unchanged native control must match its receipt")
    if digest(probe.direct_shader) != receipt["shader_sha256"]:
        raise ValueError("the original control shader must remain byte identical")
    rows = []
    for case in receipt["cases"]:
        name = f"{case['profile']}-{case['cover']:.2f}"
        path = control_root / f"{name}.input.bin"
        if digest(path) != case["input_sha256"]:
            raise ValueError("the original default inputs must remain unchanged")
        actual = probe.capture_file(f"default-{name}", path, False)
        expected = np.fromfile(control_root / f"{name}.output.bin", "<f4")
        if not np.array_equal(actual.view("<u4"), expected.view("<u4")):
            raise AssertionError("the original48B host/default result changed")
        rows.append({"case": name, "points": len(actual), "byte_identical": True})
    return rows


def cost_cases(probe, control_root, pack=frame):
    positions = np.fromfile(control_root / "0-0.65.input.bin", "<f4")[12:].reshape(
        -1, 4
    )
    rows = []
    for profile in (0, 1):
        for cover in (0.65, 1.0):
            for phase in (37.0, MONTH_S):
                for repeat in range(3):
                    for split in (False, True) if repeat % 2 == 0 else (True, False):
                        payload = pack(cover, profile, VELOCITY, phase, split=split)
                        name = f"cost-{len(rows)}"
                        path = (
                            probe.output
                            / f"cost-input-{profile}-{cover}-{phase}-{split}.bin"
                        )
                        if repeat == 0:
                            path.write_bytes(payload.tobytes() + positions.tobytes())
                        probe.capture_file(name, path, split, timed=True)
                        rows.append(
                            {
                                "name": name,
                                "profile": profile,
                                "cover": cover,
                                "phase_s": phase,
                                "repeat": repeat,
                                "split": split,
                            }
                        )
    return rows


def plane_cases(probe, pack=frame):
    width, height = 160, 96
    x, z = np.meshgrid(
        np.linspace(-2800, 2800, width), np.linspace(-1680, 1680, height)
    )
    radius, altitude = 6360000.0, 2782.0
    raised = 2 * radius * altitude + altitude**2 - x * x - z * z
    y = raised / (np.sqrt(radius * radius + raised) + radius)
    positions = (
        np.stack((x, y, z, np.zeros_like(x)), axis=-1).reshape(-1, 4).astype("<f4")
    )
    rows = []
    for split in (False, True):
        first = None
        for dt in (0.0, 1 / 60, 0.25, 30.0):
            payload = pack(0.65, 1, VELOCITY, MONTH_S + dt, split=split)
            name = f"plane-{len(rows)}"
            actual = probe.capture(name, payload, positions)
            expected = reference(positions, payload, VELOCITY, MONTH_S + dt)
            if split:
                check_density(actual, expected, payload)
            if first is None:
                first = actual
            changed = int(np.count_nonzero(actual != first))
            if dt == 1 / 60 and ((split and not changed) or (not split and changed)):
                raise AssertionError(
                    "the actual native before/after must expose sticking and progress"
                )
            rows.append(
                {"name": name, "split": split, "delta_s": dt, "changed_points": changed}
            )
    return {"width": width, "height": height, "altitude_m": altitude, "rows": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", type=Path, required=True)
    parser.add_argument("--control-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--measure-cost", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").unlink(missing_ok=True)
    probe = Probe(output)
    producer, archive, _ = build_native(args.archive_root.resolve(), output, probe.run)
    for source, target in (
        ("cloud_density.comp", probe.direct_shader),
        ("cloud_wind_precision.comp", probe.split_shader),
    ):
        probe.run(
            [
                *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
                "-V",
                "-Isrc",
                "-Isrc/generators/weather/shaders",
                ROOT / "test/experiments/weather" / source,
                "-o",
                target,
            ],
            source,
        )
    positions = points()
    record = {
        "default_cases": default_cases(probe, args.control_root),
        "field_cases": field_cases(probe, positions),
        "temporal_cases": temporal_cases(probe, positions),
        "boundary_cases": temporal_cases(
            probe, positions, (175.0 - 1 / 120, 175.0 / 3.1 - 1 / 120), "boundary"
        ),
        "anchor_cases": anchor_cases(probe, positions),
        "plane": plane_cases(probe),
        "cost_cases": cost_cases(probe, args.control_root) if args.measure_cost else [],
        "producer_commit": producer,
        "archive_sha256": digest(archive),
        "binary_sha256": digest(probe.binary),
        "split_shader_sha256": digest(probe.split_shader),
        "commands": probe.commands,
        "captures": probe.captures,
        "source_commit": probe.run(["git", "rev-parse", "HEAD"], "source").strip(),
        "source_dirty": probe.run(["git", "status", "--porcelain"], "dirty"),
        "source_sha256": {
            name: digest(ROOT / "test/experiments/weather" / name)
            for name in (
                "cloud_density_native.cpp",
                "cloud_density_native.py",
                "cloud_wind_native.py",
                "cloud_wind_precision.comp",
                "cloud_shape_field.py",
                "cloud_field.py",
            )
        },
        "scope": "Actual isolated native GPU densities including physical FP32 height; private112B layout, no production ABI/stages/history/device acceptance.",
    }
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps({key: len(record[key]) for key in record if key.endswith("_cases")})
    )


if __name__ == "__main__":
    main()

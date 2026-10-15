"""Replay spatial-source controls with the existing regular-archive GPU consumer."""

import argparse
import hashlib
import json
import os
import shlex
import shutil
import struct
import subprocess
from pathlib import Path

import numpy as np
from air_source_grid_reference import (
    RADIANCE_LIMIT,
    SIGMA,
    ShadowControl,
    controls,
    describe,
    queries,
    reference,
    relative_errors,
)

ROOT = Path(__file__).resolve().parents[3]
SHADER = Path(__file__).with_name("air_source_grid_probe.comp")
NATIVE_SAMPLE = np.dtype(
    [("radiance", "<f4", 3), ("through", "<f4", 3), ("sources", "<u4"), ("pad", "<u4")]
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(argv, folder, name):
    result = subprocess.run(
        list(map(str, argv)),
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=180,
        check=False,
    )
    (folder / f"{name}.stdout").write_text(result.stdout)
    (folder / f"{name}.stderr").write_text(result.stderr)
    if result.returncode or result.stderr:
        raise RuntimeError(f"{name} failed or emitted diagnostics: inspect {folder}")
    return {"argv": list(map(str, argv)), "exit": result.returncode}


def write_fixture(folder, bottom_km, top_km):
    radius = float(np.float32(bottom_km + 0.02))
    top_km = float(np.float32(top_km))
    extent = float(np.sqrt(top_km**2 - radius**2))
    medium = np.array(
        [
            bottom_km,
            top_km,
            1e20,
            1e20,
            0.1,
            0.2,
            0.3,
            0.03,
            0,
            0,
            0,
            0.04,
            25,
            15,
            0.8,
            0,
            0.1,
            0.13,
            0.07,
            0,
        ],
        dtype="<f4",
    )
    medium.tofile(folder / "medium.bin")
    for name, rgb in (("veil", (0.75, 0.5, 0.25)), ("multi", (1 / 64, 1 / 32, 1 / 16))):
        pixels = np.tile([*rgb, 1], (2, 2, 1)).astype("<f2")
        (folder / f"{name}.bin").write_bytes(
            struct.pack("<2I", 2, 2) + pixels.tobytes()
        )
    cases = (*controls(extent), ShadowControl(0.45, 0.2, 0, 1.2))
    write_fields(folder, cases)
    return radius, extent, cases


def write_fields(folder, cases, origin_x_m=0, direction_x=1):
    fields = ",\n".join(
        "vec3("
        + ",".join(
            format(float(value), ".9g")
            for value in (
                origin_x_m + direction_x * case.center_km * 1000,
                abs(direction_x) * case.width_km * 1000,
                case.cloud_optical_depth,
            )
        )
        + ")"
        for case in cases
    )
    (folder / "source_grid_cases.glsl").write_text(
        f"const vec3 probeSourceFields[{len(cases)}] = vec3[{len(cases)}](\n{fields}\n);\n"
    )
    (folder / "cases.json").write_text(
        json.dumps([describe(case) for case in cases], indent=2)
    )


def write_queries(folder, cases, radius, extent, sparse):
    values, rays, ranges, expected = [], [], [], []
    for index, case in enumerate(cases):
        distances = queries(case)[::16] if sparse else queries(case)
        begin = len(values)
        values.extend(distances)
        ranges.append((begin, len(values)))
        rays.append(
            struct.pack("<5f3I", radius, 0, 1, 0, extent + 1, begin, len(values), index)
        )
        high = reference(distances, case)
        low = reference(distances, case, 64)
        if np.max(np.abs(high - low)) > 1e-12:
            raise RuntimeError("independent source reference has not converged")
        expected.extend(high)
    suffix = "sparse" if sparse else "dense"
    np.asarray(values, "<f4").tofile(folder / f"queries-{suffix}.bin")
    (folder / f"rays-{suffix}.bin").write_bytes(b"".join(rays))
    expected = np.asarray(expected)
    np.save(folder / f"reference-{suffix}.npy", expected)
    return np.asarray(values), ranges, expected


def evaluate(args, batch, cases, shader, profile, sparse=False):
    label, count, mode, clear = profile
    folder = batch / (label + ("-sparse" if sparse else ""))
    folder.mkdir(exist_ok=True)
    suffix = "sparse" if sparse else "dense"
    sample = folder / "samples.bin"
    command = execute(
        [
            args.probe,
            shader,
            batch / f"rays-{suffix}.bin",
            batch / f"queries-{suffix}.bin",
            batch / "veil.bin",
            batch / "multi.bin",
            batch / "medium.bin",
            sample,
            "0",
        ],
        folder,
        "native",
    )
    actual = np.fromfile(sample, NATIVE_SAMPLE)
    expected = np.load(batch / f"reference-{suffix}.npy")
    distances = np.fromfile(batch / f"queries-{suffix}.bin", "<f4")
    errors = relative_errors(actual["radiance"], expected)
    through = float(
        np.max(np.abs(actual["through"] - np.exp(-distances[:, None] * SIGMA)))
    )
    counts = [
        len(queries(case)[::16]) if sparse else len(queries(case)) for case in cases
    ]
    ends = np.cumsum(counts)
    begins = np.concatenate([[0], ends[:-1]])
    failures = [
        index
        for index, (a, b) in enumerate(zip(begins, ends, strict=True))
        if np.max(errors[a:b]) > RADIANCE_LIMIT
    ]
    if (
        len(actual) != len(expected)
        or not np.isfinite(actual["radiance"]).all()
        or not np.isfinite(actual["through"]).all()
        or (actual["radiance"] < 0).any()
        or (actual["through"] < 0).any()
        or (actual["through"] > 1).any()
        or (actual["pad"] != 0).any()
        or through > 2e-6
        or (actual["sources"] > 2 * count + 19).any()
    ):
        raise RuntimeError(
            f"invalid transfer or exceeded work/numerical bounds: {folder}"
        )
    if label == "log256" and failures:
        raise RuntimeError(
            f"positive source cohort misses the unchanged 1% target: {folder}"
        )
    repeated = bool(sparse or actual[::2].tobytes() == actual[1::2].tobytes())
    if not repeated:
        raise RuntimeError("repeated query changed a native result")
    report = {
        "profile": label,
        "count": count,
        "mode": mode,
        "clear_control": clear,
        "cases": len(cases),
        "queries": len(actual),
        "sparse": sparse,
        "max_relative": float(errors.max()),
        "failing_case_indices": failures,
        "transmittance_max_abs": through,
        "max_sources": int(actual["sources"].max()),
        "repeated_bitwise": repeated,
        "command": command,
        "sha256": digest(sample),
    }
    (folder / "report.json").write_text(json.dumps(report, indent=2))
    return report


def compile_shader(args, batch, name, count, mode, clear, extra=()):
    shader = batch / f"{name}.spv"
    command = execute(
        [
            "nice",
            "-n",
            "10",
            *args.glslang,
            "-V",
            "--target-env",
            "vulkan1.3",
            f"-DAIR_PREFIX_INTERVALS={count}",
            f"-DSOURCE_GRID_LOG={int(name != 'quadratic22')}",
            f"-DSOURCE_SHADOW_MODE={mode}",
            f"-DSOURCE_CLEAR_CONTROL={clear}",
            *extra,
            "-Isrc",
            "-Isrc/render/shaders",
            "-Itest/experiments/render",
            f"-I{batch}",
            SHADER,
            "-o",
            shader,
        ],
        batch,
        f"compile-{name}",
    )
    return shader, command


def coordinate_controls(args, source_batch):
    reports = {}
    cases = (ShadowControl(0.45, 0.2, 2, 1.25), ShadowControl(0.45, 0.2, 0, 1.2))
    radius = float(np.float32(6360.02))
    origin_x = 1000.0
    radial_y = np.sqrt(radius**2 - (origin_x / 1000) ** 2)
    origin_y = (radial_y - 6360) * 1000
    for direction in (1, -1):
        batch = args.output / f"coordinates-{direction}"
        batch.mkdir(exist_ok=True)
        for name in ("medium.bin", "veil.bin", "multi.bin"):
            shutil.copy2(source_batch / name, batch / name)
        extent = (
            np.sqrt(6460**2 - radius**2 + (origin_x / 1000) ** 2)
            - direction * origin_x / 1000
        )
        write_fields(batch, cases, origin_x, direction)
        write_queries(batch, cases, radius, extent, False)
        rays = bytearray((batch / "rays-dense.bin").read_bytes())
        for index in range(len(cases)):
            struct.pack_into(
                "<2f",
                rays,
                index * 32 + 4,
                direction * origin_x / (radius * 1000),
                radial_y / radius,
            )
        (batch / "rays-dense.bin").write_bytes(rays)
        extra = [
            f"-DSOURCE_ORIGIN_X_M={origin_x:.9g}",
            f"-DSOURCE_ORIGIN_Y_M={origin_y:.9g}",
            f"-DSOURCE_DIRECTION_X={direction:.9g}",
        ]
        shader, compile_command = compile_shader(
            args, batch, "log256", 256, 0, 0, extra
        )
        correct = evaluate(args, batch, cases, shader, ("log256", 256, 0, 0))
        shader, wrong_compile = compile_shader(
            args,
            batch,
            "bad-coordinates",
            256,
            0,
            0,
            [*extra, "-DSOURCE_BAD_COORDINATES=1"],
        )
        wrong = evaluate(args, batch, cases, shader, ("bad-coordinates", 256, 0, 0))
        if not wrong["failing_case_indices"]:
            raise RuntimeError("the actual source origin/direction are not exercised")
        reports[str(direction)] = {
            "origin_m": [origin_x, origin_y, 0],
            "direction": [direction, 0, 0],
            "correct": correct,
            "negative": wrong,
            "compiles": [compile_command, wrong_compile],
        }
    return reports


def rejected_interfaces(args, batch):
    invalid = batch / "missing-context.comp"
    text = SHADER.read_text().replace(
        "ray, veil, multiScatter, coordinates)", "ray, veil, multiScatter)"
    )
    invalid.write_text(text)
    reports = []
    for label, source, count, message in (
        ("missing-context", invalid, 22, "no matching overloaded function"),
        ("zero-intervals", SHADER, 0, "AIR_PREFIX_INTERVALS must be positive"),
    ):
        target = batch / f"{label}.spv"
        target.unlink(missing_ok=True)
        command = [
            *args.glslang,
            "-V",
            f"-DAIR_PREFIX_INTERVALS={count}",
            "-Isrc",
            "-Isrc/render/shaders",
            "-Itest/experiments/render",
            f"-I{batch}",
            source,
            "-o",
            target,
        ]
        result = subprocess.run(
            list(map(str, command)),
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
            timeout=180,
        )
        (batch / f"{label}.log").write_text(result.stdout + result.stderr)
        if (
            not result.returncode
            or message not in result.stdout + result.stderr
            or target.exists()
        ):
            raise RuntimeError(
                f"malformed source interface was not rejected as expected: {label}"
            )
        reports.append(
            {"name": label, "exit": result.returncode, "expected_diagnostic": message}
        )
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--probe-receipt", type=Path, required=True)
    args = parser.parse_args()
    args.output, args.probe = args.output.resolve(), args.probe.resolve()
    if args.output.is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    provenance = json.loads(args.probe_receipt.read_text())
    if provenance["hashes"].get(str(args.probe)) != digest(args.probe):
        parser.error("GPU consumer differs from its declared regular-archive receipt")
    args.output.mkdir(exist_ok=True, parents=True)
    receipt = args.output / "receipt.json"
    receipt.unlink(missing_ok=True)
    args.glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    profiles = (
        ("quadratic22", 22, 0, 0),
        ("log22", 22, 0, 0),
        ("log256", 256, 0, 0),
        ("unshadowed", 256, 1, 0),
        ("whole-source", 256, 2, 0),
        ("whole-eye", 256, 3, 0),
        ("clear", 22, 0, 1),
    )
    record = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True
        ).strip(),
        "probe_sha256": digest(args.probe),
        "probe_receipt_sha256": digest(args.probe_receipt),
        "commands": [],
        "batches": {},
    }
    record["glslang"] = execute(
        [*args.glslang, "--version"], args.output, "tool-version"
    )
    eye = float(np.float32(6360.02))
    for label, bottom, top in (
        ("small", 1, 3),
        ("thin", 6360, np.sqrt(eye**2 + 100**2)),
        ("earth", 6360, 6460),
    ):
        batch = args.output / label
        batch.mkdir(exist_ok=True)
        radius, extent, cases = write_fixture(batch, bottom, top)
        write_queries(batch, cases, radius, extent, False)
        write_queries(batch, cases, radius, extent, True)
        reports = []
        for profile in profiles:
            name, count, mode, clear = profile
            shader, command = compile_shader(args, batch, name, count, mode, clear)
            record["commands"].append(command)
            reports.append(evaluate(args, batch, cases, shader, profile))
            if name == "log256":
                reports.append(evaluate(args, batch, cases, shader, profile, True))
                dense = np.fromfile(batch / "log256/samples.bin", NATIVE_SAMPLE)
                sparse = np.fromfile(batch / "log256-sparse/samples.bin", NATIVE_SAMPLE)
                offset, selected = 0, []
                for case in cases:
                    count_q = len(queries(case))
                    selected.extend(range(offset, offset + count_q, 16))
                    offset += count_q
                if dense[selected].tobytes() != sparse.tobytes():
                    raise RuntimeError(
                        "sparse queries changed the physical source partition"
                    )
        clear = np.fromfile(batch / "clear/samples.bin", NATIVE_SAMPLE)
        count_q = len(queries(cases[-1]))
        for name in ("quadratic22", "log22", "log256"):
            data = np.fromfile(batch / f"{name}/samples.bin", NATIVE_SAMPLE)
            if data[-count_q:].tobytes() != clear[-count_q:].tobytes():
                raise RuntimeError(
                    "null cloud differs from the unchanged clear consumer"
                )
        for report in reports:
            if report["mode"] and not report["failing_case_indices"]:
                raise RuntimeError("wrong source control was not rejected")
        record["batches"][label] = {
            "extent_km": extent,
            "cases": len(cases),
            "reports": reports,
            "null_cloud_bitwise": True,
            "sparse_bitwise": True,
        }
        print(
            label,
            [
                (r["profile"], r["max_relative"], len(r["failing_case_indices"]))
                for r in reports
            ],
            flush=True,
        )
    record["coordinate_controls"] = coordinate_controls(args, args.output / "earth")
    record["rejected_interfaces"] = rejected_interfaces(args, args.output / "small")
    record["sources_sha256"] = {
        str(path.relative_to(ROOT)): digest(path)
        for path in (
            SHADER,
            Path(__file__),
            Path(__file__).with_name("air_source_grid_reference.py"),
            ROOT / "src/render/shaders/airSource.glsl",
            ROOT / "src/render/shaders/airPrefix.glsl",
        )
    }
    record["artifact_hashes"] = {
        str(path.relative_to(args.output)): digest(path)
        for path in args.output.rglob("*")
        if path.is_file() and path.suffix in (".spv", ".glsl", ".bin", ".npy")
    }
    receipt.write_text(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()

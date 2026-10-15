"""Replay the joint prefix using the existing native two-LUT consumer and fixtures."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
from pathlib import Path

import numpy as np
from cloud_air_reference import RAY, SAMPLE, check

ROOT = Path(__file__).resolve().parents[3]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, output, name, commands, expected=0):
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=40, check=False
    )
    (output / f"{name}.stdout").write_text(result.stdout)
    (output / f"{name}.stderr").write_text(result.stderr)
    commands.append(
        {
            "command": command,
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    )
    if result.returncode != expected:
        raise RuntimeError(f"{name} failed ({result.returncode}); see {output}")


def repeated_queries(case):
    rays = np.fromfile(case / "rays.bin", dtype=RAY)
    queries = np.fromfile(case / "queries.bin", dtype="<f4")
    samples = np.fromfile(case / "native.bin", dtype=SAMPLE)
    repeats = 0
    for ray in rays:
        for index in range(int(ray["begin"]) + 1, int(ray["end"])):
            if queries[index] == queries[index - 1]:
                assert samples[index].tobytes() == samples[index - 1].tobytes()
                repeats += 1
    return repeats


def compile_probe(args, case, commands, definitions):
    includes = [
        ROOT / "src",
        ROOT / "src/render/shaders",
        args.air_root / "src",
        args.air_root / "src/render/shaders",
        args.air_root / "test/experiments/render",
    ]
    command = [
        "nice",
        "-n",
        "10",
        *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
        "-V",
    ]
    command += ["-I" + str(path) for path in includes]
    command += [f"-D{name}={value}" for name, value in definitions.items()]
    command += [
        "test/experiments/weather/cloud_air_prefix.comp",
        "-o",
        str(case / "probe.spv"),
    ]
    run(command, case, "compile", commands)


def dense_control(args, commands):
    case = args.output / "dense-clear-repartitioned"
    case.mkdir(exist_ok=True)
    fixtures = args.air_root / "test/experiments/render/data/air-prefix/dense"
    compile_probe(
        args,
        case,
        commands,
        {
            "CLOUD_PREFIX_COVER": 1.0,
            "CLOUD_PREFIX_EXTINCTION_PER_M": 0.0,
            "CLOUD_VIEW_WARP": args.warp,
        },
    )
    command = [
        str(args.probe),
        str(case / "probe.spv"),
        str(fixtures / "rays.bin"),
        str(fixtures / "queries.bin"),
        str(fixtures / "veil.bin"),
        str(fixtures / "multi.bin"),
        str(fixtures / "medium.bin"),
        str(case / "native.bin"),
    ]
    run(command, case, "capture", commands)
    assert not commands[-1]["stderr"], "native validation emitted diagnostics"
    command = [
        "nice",
        "-n",
        "10",
        "python3",
        str(args.air_root / "test/experiments/render/check_air_prefix_gpu.py"),
        "--fixtures",
        str(fixtures),
        "--samples",
        str(case / "native.bin"),
        "--output",
        str(case / "comparison.json"),
    ]
    run(command, case, "reference", commands)
    samples = np.fromfile(case / "native.bin", dtype=SAMPLE)
    assert ((samples["counts"] >> 8) & 255).max() == 0
    assert (samples["counts"] & 255).max() == 0
    return json.loads((case / "comparison.json").read_text())


def invalid_controls(args, commands):
    output = args.output / "invalid"
    output.mkdir(exist_ok=True)
    base = args.output / "clear"
    fixture = args.air_root / "test/experiments/render/data/air-prefix/homogeneous"
    medium = np.fromfile(fixture / "medium.bin", dtype="<f4")
    for name, radius in [
        ("nan", float("nan")),
        ("infinity", float("inf")),
        ("below-planet", float(medium[0]) - 1.0),
    ]:
        rays = np.fromfile(base / "rays.bin", dtype=RAY)
        rays[0]["ray"][0] = radius
        path = output / f"{name}.bin"
        rays.tofile(path)
        result = output / f"{name}-output.bin"
        result.unlink(missing_ok=True)
        command = [
            str(args.probe),
            str(base / "probe.spv"),
            str(path),
            str(base / "queries.bin"),
            str(fixture / "veil.bin"),
            str(fixture / "multi.bin"),
            str(fixture / "medium.bin"),
            str(result),
        ]
        run(command, output, name, commands, expected=2)
        assert "invalid fixture extents or query spans" in commands[-1]["stderr"]
        assert not commands[-1]["stdout"] and not result.exists()
    return 3


def replay(args):
    args.output.mkdir(parents=True, exist_ok=True)
    receipt = args.output / "receipt.json"
    receipt.unlink(missing_ok=True)
    reports, commands = {}, []
    light_fixture = (
        args.air_root / "test/experiments/render/data/air-prefix/homogeneous"
    )
    for name, cover, fresh, clear_control in [
        ("clear", 0.0, 0, 0),
        ("broken", 0.65, 0, 0),
        ("overcast", 1.0, 0, 0),
        ("fresh", 0.65, 1, 0),
        ("clear-control", 0.0, 0, 1),
    ]:
        case = args.output / name
        case.mkdir(exist_ok=True)
        command = [
            "nice",
            "-n",
            "10",
            "python3",
            "-W",
            "error",
            "test/experiments/weather/cloud_air_reference.py",
            "--output",
            str(case),
            "--fixtures",
            str(light_fixture),
            "--cover",
            str(cover),
            "--subset",
            args.subset,
            "--warp",
            str(args.warp),
        ]
        run(command, case, "prepare", commands)
        shader = case / "probe.spv"
        compile_probe(
            args,
            case,
            commands,
            {
                "CLOUD_PREFIX_COVER": cover,
                "CLOUD_PREFIX_FRESH_QUERY": fresh,
                "CLOUD_PREFIX_CLEAR_CONTROL": clear_control,
                "CLOUD_VIEW_WARP": args.warp,
            },
        )
        command = [
            str(args.probe),
            str(shader),
            str(case / "rays.bin"),
            str(case / "queries.bin"),
            str(light_fixture / "veil.bin"),
            str(light_fixture / "multi.bin"),
            str(light_fixture / "medium.bin"),
            str(case / "native.bin"),
        ]
        run(command, case, "capture", commands)
        reports[name] = check(case, case / "native.bin", cover)
        reports[name]["bit_identical_repeated_queries"] = repeated_queries(case)
        reports[name]["shader_sha256"] = sha(shader)
        assert not commands[-1]["stderr"], "native validation emitted diagnostics"
    reports["dense_clear_repartitioned"] = dense_control(args, commands)
    reports["invalid_input_controls"] = invalid_controls(args, commands)
    streamed = np.fromfile(args.output / "broken/native.bin", dtype=SAMPLE)
    fresh = np.fromfile(args.output / "fresh/native.bin", dtype=SAMPLE)
    difference = max(
        float(np.max(np.abs(streamed[name] - fresh[name])))
        for name in ("radiance", "through")
    )
    assert difference < 1e-4
    assert all(
        np.array_equal(streamed[name], fresh[name]) for name in ("sources", "counts")
    )
    clear = np.fromfile(args.output / "clear/native.bin", dtype=SAMPLE)
    control = np.fromfile(args.output / "clear-control/native.bin", dtype=SAMPLE)
    clear_error = float(np.max(np.abs(clear["radiance"] - control["radiance"])))
    clear_t_error = float(np.max(np.abs(clear["through"] - control["through"])))
    assert clear_error < 1e-5 and clear_t_error < 2e-6
    assert all(
        np.array_equal(clear[name], control[name]) for name in ("sources", "counts")
    )
    reports["separate_shader_specialization_max_difference"] = difference
    reports["separate_shader_specialization_counts_identical"] = True
    reports["zero_cloud_control_bit_identical"] = clear.tobytes() == control.tobytes()
    reports["zero_cloud_control_radiance_difference"] = clear_error
    reports["zero_cloud_control_transmittance_difference"] = clear_t_error
    reports["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    reports["working_tree_dirty"] = bool(
        subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)
    )
    reports["consumer_sha256"] = sha(args.probe)
    reports["subset"] = args.subset
    reports["warp"] = args.warp
    reports["probe_shader_sha256"] = sha(
        ROOT / "test/experiments/weather/cloud_air_prefix.comp"
    )
    reports["air_source_sha256"] = {
        str(path.relative_to(args.air_root)): sha(path)
        for path in sorted((args.air_root / "src").rglob("*"))
        if path.is_file()
        and (path.name.startswith(("air", "Air", "ClearSun", "clearSun")))
    }
    reports["commands"] = commands
    reports["source_sha256"] = {
        str(path.relative_to(ROOT)): sha(path)
        for path in [
            ROOT / "src/render/shaders" / name
            for name in [
                "cloudPrefix.glsl",
                "cloudAirCell.glsl",
                "cloudAirPrefix.glsl",
                "cloudLayer.glsl",
                "cloudRadiance.glsl",
            ]
        ]
    }
    reports["scope"] = (
        "Native homogeneous clear air plus procedural clouds; no general source-LUT, scene, GPU-time or frame-budget acceptance."
    )
    receipt.write_text(json.dumps(reports, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": reports["commit"],
                "subset": args.subset,
                "warp": args.warp,
                "cases": {
                    name: {
                        key: value
                        for key, value in row.items()
                        if key
                        in (
                            "samples",
                            "records",
                            "max_radiance_error",
                            "max_transmittance_error",
                            "meets_targets",
                        )
                    }
                    for name, row in reports.items()
                    if isinstance(row, dict)
                    and name not in ("source_sha256", "air_source_sha256")
                },
                "receipt": str(receipt),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--air-root", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--subset", choices=("all", "ground", "orbit"), default="all")
    parser.add_argument("--warp", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    for name in ("output", "air_root", "probe"):
        setattr(args, name, getattr(args, name).resolve())
    replay(args)


if __name__ == "__main__":
    main()

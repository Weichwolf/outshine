#!/usr/bin/env python3
"""Check the product archive's CPU/JSON contracts and reject incorrect real GPU output."""

import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def compiler(root):
    database = json.loads((root / "compile_commands.json").read_text())
    entry = next(
        row for row in database if row["file"].endswith("/calibration/Json.cpp")
    )
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    result = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
        elif argument == "-o":
            skip = True
        elif argument != "-c" and argument != entry["file"]:
            result.append(argument)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root, output = args.engine_root.resolve(), args.output.resolve()
    if not output.is_relative_to(Path(os.environ.get("TMPDIR", "/tmp")).resolve()):
        parser.error("private outputs must be under the system temporary directory")
    output.mkdir(parents=True, exist_ok=True)
    flags = compiler(root)
    archive = str(root / "build/liboutshine.a")
    native = shlex.split(
        subprocess.check_output(
            ["sh", "test/run.sh", "--native-link-flags"], cwd=root, text=True
        )
    )
    sources = [
        ("json-probe", HERE / "checks/JsonProbe.cpp", []),
        ("cpu-threads-probe", HERE / "checks/CpuThreads.cpp", []),
        ("gpu-probe", root / "test/harness/render/CalibrationProbe.cpp", native),
    ]
    commands = []
    with (output / "checks-build.log").open("w") as log:
        for name, source, libraries in sources:
            command = [
                "nice",
                "-n",
                "10",
                *flags,
                str(source),
                archive,
                *libraries,
                "-o",
                str(output / name),
            ]
            commands.append(command)
            subprocess.run(
                command,
                cwd=root,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=120,
            )
        source = output / "wrong-fma.comp"
        source.write_text(
            (root / "src/render/shaders/calibrationFma.comp")
            .read_text()
            .replace("into.values[at] = value;", "into.values[at] = vec4(0.0);")
        )
        glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
        command = [
            "nice",
            "-n",
            "10",
            *glslang,
            "-V",
            "--target-env",
            "vulkan1.0",
            "-I" + str(root / "src"),
            str(source),
            "-o",
            str(output / "wrong-fma.spv"),
        ]
        commands.append(command)
        subprocess.run(
            command,
            cwd=root,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=120,
        )
    (output / "checks-commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    result = json.loads(
        subprocess.check_output([str(output / "json-probe")], text=True, timeout=10)
    )
    assert result["cpu"] == 'quote" newline\n tab\t slash\\'
    missing, cpu = result["workloads"]
    assert missing["summary"] is None
    assert all(sample["gpu_ns"] is None for sample in missing["samples"])
    assert cpu["summary"]["median_ns"] == 20
    assert cpu["summary"]["min_ns"] == 10 and cpu["summary"]["max_ns"] == 30
    assert cpu["summary"]["ns_per_access"] == 10
    cpu_threads = subprocess.run(
        [str(output / "cpu-threads-probe")],
        cwd=root,
        capture_output=True,
        text=True,
        timeout=30,
        check=True,
    )
    (output / "cpu-threads.log").write_text(cpu_threads.stdout + cpu_threads.stderr)
    assert not cpu_threads.stderr
    environment = os.environ.copy()
    if sys.platform == "darwin":
        environment["MTL_DEBUG_LAYER"] = "1"
    shaders = root / "build/shaders"
    command = [
        str(output / "gpu-probe"),
        str(output / "wrong-fma.spv"),
        *[
            str(shaders / (name + ".spv"))
            for name in [
                "calibrationTriad.comp",
                "calibrationRaster.vert",
                "calibrationRaster.frag",
            ]
        ],
        "product-archive",
        "negative-control",
        "functional check",
        "1",
    ]
    child = subprocess.run(
        command,
        cwd=root,
        capture_output=True,
        text=True,
        timeout=120,
        env=environment,
        check=False,
    )
    (output / "negative-control.log").write_text(child.stdout + child.stderr)
    assert child.returncode != 0
    assert "GPU calibration result differs from complete CPU reference" in child.stderr
    assert not child.stdout.strip()
    (output / "checks.json").write_text(
        json.dumps(
            {
                "status": "terminal",
                "exit": 0,
                "missing_gpu_time": "null without wait-time fallback",
                "cpu_threads_equal_work": "1/2/4/6 verified",
                "json_escape": "pass",
                "real_gpu_mismatch": "rejected",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        "CPU thread partitions, JSON time-basis and real incorrect-GPU-output checks passed"
    )


if __name__ == "__main__":
    main()

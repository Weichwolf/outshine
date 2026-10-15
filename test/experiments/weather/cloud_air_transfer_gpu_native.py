"""Compare fused and staged resident GPU normalization with live state updates."""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from cloud_air_transfer_native import compile_kernel, run_capture
from cloud_sun_reuse_benchmark import load_snapshot
from cloud_sun_reuse_native import ROOT, build_consumer, digest


def monitor(command, folder, name, cwd):
    samples = []
    cost = 0.0
    with (
        (folder / f"{name}.stdout").open("w") as stdout,
        (folder / f"{name}.stderr").open("w") as stderr,
    ):
        process = subprocess.Popen(command, cwd=cwd, stdout=stdout, stderr=stderr)
        while process.poll() is None:
            start = time.monotonic()
            row = load_snapshot()
            try:
                status = Path(f"/proc/{process.pid}/status").read_text()
                match = re.search(r"^VmHWM:\s+(\d+) kB$", status, re.MULTILINE)
                if match:
                    row["process_peak_rss_bytes"] = int(match.group(1)) * 1024
            except FileNotFoundError:
                pass
            samples.append(row)
            cost += time.monotonic() - start
            time.sleep(0.02)
    return process.wait(), {
        "samples": samples,
        "sampling_wall_seconds": cost,
        "scope": "Observed global NVML memory/load and child Linux VmHWM; not isolated app/device budget",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--api-root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-receipt", type=Path, required=True)
    parser.add_argument("--step", type=int, default=8)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    if (
        args.output.resolve().is_relative_to(ROOT)
        or not 1 <= args.repeats <= 16
        or args.step < 1
    ):
        parser.error(
            "use an explicit system-temp output, positive step and1..16 repeats"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    record = {"commands": [], "profiles": {}, "step": args.step}
    receipt = args.output / "receipt.json"

    def execute(command, folder, name, cwd=ROOT, measured=False):
        command = list(map(str, command))
        if measured:
            status, telemetry = monitor(command, folder, name, cwd)
            record.setdefault("telemetry", {})[folder.name] = telemetry
            stdout = (folder / f"{name}.stdout").read_text()
        else:
            result = subprocess.run(
                command, cwd=cwd, capture_output=True, text=True, check=False
            )
            (folder / f"{name}.stdout").write_text(result.stdout)
            (folder / f"{name}.stderr").write_text(result.stderr)
            status, stdout = result.returncode, result.stdout
        record["commands"].append({"argv": command, "cwd": str(cwd), "exit": status})
        receipt.write_text(json.dumps(record, indent=2))
        if status:
            raise subprocess.CalledProcessError(status, command)
        return stdout

    record["commit"] = execute(
        ["git", "rev-parse", "HEAD"], args.output, "commit"
    ).strip()
    record["dirty"] = execute(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    binary, record["consumer"] = build_consumer(
        args,
        execute,
        source=ROOT / "test/experiments/weather/cloud_air_transfer_gpu.cpp",
        extra_includes=(
            args.api_root / "src/render/device/native",
            ROOT / "test/harness/render",
            ROOT / "test/harness/shared",
        ),
    )
    baseline, record["baseline_consumer"] = build_consumer(args, execute)
    worker = [
        "nice",
        "-n",
        "10",
        sys.executable,
        ROOT / "test/experiments/weather/cloud_air_transfer_gpu_cases.py",
        "--output",
        args.output,
        "--step",
        args.step,
    ]
    execute([*worker, "--task", "prepare"], args.output, "prepare")
    field = args.output / f"step-{args.step}"
    for name, defines in (
        ("advected", []),
        ("stationary", ["-DSOURCE_WIND_EAST_M=0", "-DSOURCE_WIND_SOUTH_M=0"]),
    ):
        shader = args.output / f"direct-{name}.spv"
        compile_kernel(field, shader, execute, "-DSOURCE_EYE_CLOUD_COVER=0", *defines)
        record["profiles"][f"direct-{name}"] = run_capture(
            args,
            baseline,
            shader,
            args.output,
            args.output / f"direct-{name}.bin",
            execute,
        )
    reconstruction = args.output / "reconstruct.spv"
    compile_kernel(field, reconstruction, execute, reconstruct=True)
    cubic_reconstruction = args.output / "reconstruct-cubic.spv"
    compile_kernel(
        field, cubic_reconstruction, execute, "-DTRANSFER_CUBIC=1", reconstruct=True
    )
    normalizer = args.output / "normalize.spv"
    execute(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
            "-V",
            "-Isrc",
            "-Isrc/render/shaders",
            "-Itest/experiments/weather",
            "-Itest/experiments/render",
            ROOT / "test/experiments/weather/cloud_air_transfer_normalize.comp",
            "-o",
            normalizer,
        ],
        args.output,
        "compile-normalize",
    )
    for name, output_image in (("bilinear", 1), ("fused", 1), ("staged", 0)):
        folder = args.output / name
        folder.mkdir(exist_ok=True)
        shader = folder / f"{name}.spv"
        compile_kernel(
            field,
            shader,
            execute,
            "-DSOURCE_EYE_CLOUD_COVER=0",
            "-DSOURCE_FIELD_STATE=1",
            f"-DSOURCE_VISIBILITY_OUTPUT={output_image}",
        )
        command = [
            binary,
            shader,
            reconstruction if name == "bilinear" else cubic_reconstruction,
            args.output,
            field,
            folder / "gpu",
            args.repeats,
        ]
        if name == "staged":
            command += ["--staged", normalizer]
        before = load_snapshot()
        start = time.monotonic()
        log = execute(command, folder, "capture", measured=True)
        elapsed = time.monotonic() - start
        rows = []
        for line in log.splitlines():
            if not line.startswith("gpu_pair "):
                continue
            row = {k: float(v) for k, v in re.findall(r"(\w+)=([^\s]+)", line)}
            if not all(np.isfinite(list(row.values()))) or min(row.values()) < 0:
                raise RuntimeError("invalid native GPU timing row")
            rows.append(row)
        if len(rows) != 4 * (args.repeats + 1):
            raise RuntimeError("not all dependent GPU timing rows were retained")
        record["profiles"][name] = {
            "rows": rows,
            "load_before": before,
            "load_after": load_snapshot(),
            "process_wall_seconds": elapsed,
        }
    execute([*worker, "--task", "analyze"], args.output, "analyze")
    execute(
        [
            *worker,
            "--task",
            "guards",
            "--binary",
            binary,
            "--producer",
            args.output / "fused/fused.spv",
            "--reconstruction",
            cubic_reconstruction,
        ],
        args.output,
        "guards",
    )
    record["guards"] = json.loads((args.output / "guards/receipt.json").read_text())
    record["analysis"] = json.loads((args.output / "analysis.json").read_text())
    record["hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.rglob("*")
        if p.suffix in (".bin", ".field", ".spv", ".glsl")
    }
    source = [
        ROOT / "test/experiments/render/air_prefix_gpu.cpp",
        ROOT / "test/harness/render/NativeGpuCheck.h",
        ROOT / "test/harness/shared/Check.h",
        *sorted((ROOT / "test/experiments/weather").glob("*.*")),
        *sorted((ROOT / "src/render/shaders").glob("*.glsl")),
        *sorted((ROOT / "src/generators/weather/shaders").glob("*.glsl")),
    ]
    record["source_hashes"] = {
        str(p.relative_to(ROOT)): digest(p) for p in source if p.is_file()
    }
    receipt.write_text(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()

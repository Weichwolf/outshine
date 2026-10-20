"""Replay native visibility selection, compaction and position-only depth costs."""

import argparse
import json
import math
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from statistics import median

from native_visibility_data import digest, extract, prepare

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "test/outshine/data/render/visibility/selected-native.tar.xz"


def execute(command, checkout, output, name, records):
    began = time.monotonic()
    row = {"name": name, "command": list(map(str, command)), "status": None}
    records.append(row)
    try:
        with (output / f"{name}.log").open("w") as stream:
            result = subprocess.run(
                command,
                cwd=checkout,
                stdout=stream,
                stderr=subprocess.STDOUT,
                timeout=180,
                check=False,
            )
        row["status"] = result.returncode
        if result.returncode:
            raise RuntimeError(f"{name} exited {result.returncode}; inspect {name}.log")
    finally:
        row["wall_s"] = time.monotonic() - began


def build(checkout, output, records):
    database = json.loads((checkout / "compile_commands.json").read_text())
    entry = next(row for row in database if Path(row["file"]).name == "GpuCompute.cpp")
    if Path(entry["directory"]).resolve() != checkout:
        raise ValueError("compilation database belongs to another checkout")
    source = Path(__file__).with_suffix(".cpp")
    command = list(entry.get("arguments") or shlex.split(entry["command"]))
    command[command.index(entry["file"])] = str(source)
    command[command.index("-o") + 1] = str(output / "probe.o")
    for flag, name in (("-MF", "probe.d"), ("-MJ", "probe-command.json")):
        if flag in command:
            command[command.index(flag) + 1] = str(output / name)
    (output / "compile_commands.json").write_text(
        json.dumps(
            [{"directory": str(checkout), "file": str(source), "arguments": command}]
        )
    )
    execute(["nice", "-n", "10", *command], checkout, output, "compile", records)
    libraries = shlex.split(
        subprocess.check_output(
            ["sh", "test/run.sh", "--native-link-flags"], cwd=checkout, text=True
        )
    ) + shlex.split(
        subprocess.check_output(["pkg-config", "--libs", "sdl3"], text=True)
    )
    flags = [
        flag
        for flag in command
        if flag.startswith(("-stdlib=", "-fsanitize=", "-fno-sanitize=", "-fuse-ld="))
    ]
    binary = output / "visibility-pipeline"
    execute(
        [
            "nice",
            "-n",
            "10",
            command[0],
            *flags,
            str(output / "probe.o"),
            str(checkout / "build/liboutshine.a"),
            *libraries,
            "-o",
            str(binary),
        ],
        checkout,
        output,
        "link",
        records,
    )
    return binary


def shaders(checkout, output, records):
    compiler = checkout / "build/deps/install/bin/glslangValidator"
    if not compiler.is_file():
        compiler = shutil.which("glslangValidator")
    if not compiler:
        raise ValueError("build/install glslangValidator before the native replay")
    sources = Path(__file__).with_name("native_visibility")
    for name, source in (
        ("select", sources / "select.comp"),
        ("pack", sources / "pack.comp"),
        ("scan", checkout / "src/render/shaders/subjectScanKernel.comp"),
        ("compact32", sources / "compact.comp"),
        ("raster", sources / "raster.vert"),
    ):
        execute(
            [
                "nice",
                "-n",
                "10",
                str(compiler),
                "-V",
                "--target-env",
                "vulkan1.0",
                "-I" + str(checkout / "src/render/shaders"),
                str(source),
                "-o",
                str(output / f"{name}.spv"),
            ],
            checkout,
            output,
            f"shader-{name}",
            records,
        )


def verify(result):
    summary = []
    for row in result["variants"]:
        checks = row["verification"]
        if (
            checks["false_rejections_vs_double"] != 0
            or not checks["original_indices_exact"]
            or not checks["baseline_depth_exact"]
        ):
            raise ValueError(
                "native bounds, compaction or baseline raster control failed"
            )
        timing = {}
        for mode in ("baseline_raster", "select_compact", "select_compact_raster"):
            values = row[mode]
            if len(values) != 12 or any(
                value is None or not math.isfinite(value) or value <= 0
                for value in values
            ):
                raise ValueError("native GPU timing is incomplete")
            timing[mode + "_p50_ms"] = median(values) / 1e6
        summary.append({"label": row["label"], **timing, **checks})
    expected = {
        f"{policy}-{limit}"
        for policy in ("source", "block")
        for limit in (0, 64, 32, 16, 8)
    }
    if len(summary) != 10 or {row["label"] for row in summary} != expected:
        raise ValueError("native replay did not verify every group/batch variant")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--input", default=FIXTURE, type=Path)
    parser.add_argument("--device-name")
    args = parser.parse_args()
    checkout, output = args.checkout.resolve(), args.output.resolve()
    roots = {Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve()}
    if (
        not any(output != root and output.is_relative_to(root) for root in roots)
        or output.is_relative_to(ROOT)
        or output.is_relative_to(checkout)
        or output.exists()
    ):
        parser.error("use a new system-temp output outside both checkouts")
    if sys.platform == "darwin":
        os.environ.setdefault("MTL_DEBUG_LAYER", "1")
    output.mkdir(parents=True)
    records = []
    receipt = {
        "terminal": False,
        "steps": records,
        "scope": "One pose, position-only; no frame admission",
    }
    status = 2
    try:
        receipt["native_commit"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip()
        receipt["native_changes"] = subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=checkout,
            text=True,
        ).splitlines()
        sources = [
            Path(__file__),
            Path(__file__).with_suffix(".cpp"),
            Path(__file__).with_name("native_visibility_data.py"),
            Path(__file__).with_name("native_visibility_cost.py"),
            *sorted(Path(__file__).with_name("native_visibility").glob("*")),
            checkout / "src/render/shaders/subjectScanKernel.comp",
            checkout / "src/render/shaders/cullView.glsl",
        ]
        receipt["source_sha256"] = {str(path): digest(path) for path in sources}
        receipt["input"] = extract(args.input.resolve(), output / "input")
        receipt["variants"] = prepare(output / "input", output / "prepared")
        shaders(checkout, output, records)
        binary = build(checkout, output, records)
        execute(
            [
                str(binary),
                str(output / "prepared"),
                str(output / "input"),
                str(output / "select.spv"),
                str(output),
                str(output / "result.json"),
            ],
            checkout,
            output,
            "run",
            records,
        )
        result = json.loads((output / "result.json").read_text())
        if args.device_name and result["gpu"] != args.device_name:
            raise ValueError("actual native GPU differs from --device-name")
        if "VUID-" in (output / "run.log").read_text():
            raise ValueError("native replay reports Vulkan validation errors")
        receipt.update(
            gpu=result["gpu"],
            summary=verify(result),
            binary_sha256=digest(binary),
            shader_sha256={path.name: digest(path) for path in output.glob("*.spv")},
        )
        status = 0
    except (
        OSError,
        ValueError,
        RuntimeError,
        subprocess.SubprocessError,
        StopIteration,
    ) as error:
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
    finally:
        receipt.update(terminal=True, status=status)
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if status == 0:
        print(json.dumps({"gpu": receipt["gpu"], "variants": receipt["summary"]}))
    return status


if __name__ == "__main__":
    sys.exit(main())

"""Compare Metal indirect loops with GPU-encoded command buffers, without the engine."""

import argparse
import hashlib
import json
import os
import platform
import re
import shlex
import subprocess
import tempfile
from pathlib import Path

CASE = re.compile(
    r"COUNT (\d+) INDEX (\d+) FRAMES (\d+) EXACT (\d+) "
    r"CPU_LOOP_MS ([0-9.]+) CPU_ICB_MS ([0-9.]+) "
    r"GPU_LOOP_MS ([0-9.]+) GPU_ICB_MS ([0-9.]+)"
)


def run(output, validation):
    if platform.system() != "Darwin":
        raise ValueError("this backend experiment requires macOS and Metal")
    output = output.resolve()
    roots = (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    if not any(output.is_relative_to(root) for root in roots):
        raise ValueError("private outputs belong in the system temporary directory")
    output.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).with_suffix(".mm").resolve()
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    build = [
        "nice",
        "-n",
        "10",
        *compiler,
        "-std=c++23",
        "-fobjc-arc",
        "-O3",
        "-Wall",
        "-Wextra",
        "-Werror",
        str(source),
        "-framework",
        "Foundation",
        "-framework",
        "Metal",
        "-framework",
        "ImageIO",
        "-framework",
        "CoreGraphics",
        "-o",
        str(output / "benchmark"),
    ]
    with (output / "build.log").open("w") as log:
        subprocess.run(
            build, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=60
        )
    env = os.environ.copy()
    env.pop("MTL_DEBUG_LAYER", None)
    env.pop("MTL_SHADER_VALIDATION", None)
    if validation:
        env["MTL_DEBUG_LAYER"] = "1"
    command = [str(output / "benchmark"), "--gpu-arguments"]
    with (output / "run.log").open("w") as log:
        subprocess.run(
            command,
            check=True,
            cwd=output,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
    text = (output / "run.log").read_text()
    rows = []
    for match in CASE.finditer(text):
        count, index, frames, exact, cpu_loop, cpu_icb, gpu_loop, gpu_icb = (
            match.groups()
        )
        rows.append(
            {
                "records": int(count),
                "index_bits": int(index),
                "frames": int(frames),
                "exact_pairs": int(exact),
                "cpu_loop_ms": float(cpu_loop),
                "cpu_icb_ms": float(cpu_icb),
                "gpu_loop_ms": float(gpu_loop),
                "gpu_icb_ms": float(gpu_icb),
            }
        )
    expected = {
        (count, index) for count in (0, 1, 7, 128, 1024, 8192) for index in (16, 32)
    }
    if (
        {(row["records"], row["index_bits"]) for row in rows} != expected
        or len(rows) != len(expected)
        or any(row["exact_pairs"] != 20 for row in rows)
    ):
        raise ValueError("missing or incomplete exact render comparison")
    record = {
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "binary_sha256": hashlib.sha256(
            (output / "benchmark").read_bytes()
        ).hexdigest(),
        "build": build,
        "command": command,
        "platform": platform.platform(),
        "api_validation": validation,
        "metal_shader_validation": False,
        "gpu_generated_arguments": True,
        "cpu_argument_reference": True,
        "pairs_alternate_order": True,
        "warmup_frames_per_case": 4,
        "timed_frames_per_case": 16,
        "readback_in_gpu_time": True,
        "icb_allocations": [
            int(value) for value in re.findall(r"ICB_BYTES (\d+)", text)
        ],
        "cases": rows,
        "scope": "synthetic backend encoding; no world or frame-budget admission",
    }
    (output / "report.json").write_text(json.dumps(record, indent=2) + "\n")
    for row in rows:
        if row["records"] == 8192:
            print(json.dumps(row))
    print(f"PASS 12 cases / 240 exact pairs and CPU argument controls; {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--validation", action="store_true")
    args = parser.parse_args()
    run(args.output, args.validation)


if __name__ == "__main__":
    main()

"""Profile scenario benchmark passes and submitted triangle references on Metal."""

import argparse
import hashlib
import json
import math
import os
import platform
import re
import shlex
import subprocess
import tempfile
from collections import defaultdict
from itertools import pairwise
from pathlib import Path
from statistics import mean

from metal_compute_profile import parse_compute_records, summarize_compute

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "test/harness/render/MetalWorkProfile.mm"


def parse_records(text):
    passes = {}
    work = []
    for line in text.splitlines():
        if not line.startswith(("METAL_PASS serial=", "METAL_WORK serial=")):
            continue
        fields = dict(re.findall(r"(\w+)=([^ ]*)", line))
        record = {
            key: int(value) if key != "pipeline" else value
            for key, value in fields.items()
        }
        if line.startswith("METAL_PASS "):
            key = record["serial"], record["pass"]
            if key in passes:
                raise ValueError("duplicate raster interval")
            passes[key] = record
        else:
            work.append(record)
    frames = {
        record["serial"]
        for record in passes.values()
        if "sky.frag.spv" in record["pipeline"]
    }
    return passes, work, frames


def summarize(passes, work, frames):
    groups = defaultdict(list)
    for record in passes.values():
        if record["serial"] in frames:
            groups[record["pipeline"] or "clear"].append(record)
    phases = {}
    for name, rows in groups.items():
        valid = [row for row in rows if row["valid"]]
        phases[name] = {
            "samples": len(rows),
            "valid_timing_samples": len(valid),
            "vertex_ms_mean": mean(row["vertex_ns"] for row in valid) / 1e6
            if valid
            else None,
            "fragment_ms_mean": mean(row["fragment_ns"] for row in valid) / 1e6
            if valid
            else None,
            "calls_mean": mean(row["calls"] for row in rows),
            "triangle_references_mean": mean(
                row["direct_indices"] + row["indirect_indices"] for row in rows
            )
            / 3,
            "unavailable_indirect": sum(row["unavailable_indirect"] for row in rows),
        }
    by_shader = defaultdict(list)
    for record in work:
        if record["serial"] in frames:
            by_shader[record["pipeline"]].append(record["indices"])
    return {
        "frame_submissions": len(frames),
        "raster_passes": phases,
        "shader_triangle_references_mean": {
            name: sum(values) / len(frames) / 3 for name, values in by_shader.items()
        },
    }


def measured_frames(text, available, benchmark):
    commands = {}
    for line in text.splitlines():
        if not line.startswith("METAL_COMMAND "):
            continue
        fields = {key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", line)}
        if (
            not {"serial", "valid", "gpu_ns"} <= fields.keys()
            or fields["serial"] <= 0
            or fields["valid"] not in (0, 1)
            or fields["gpu_ns"] < 0
        ):
            raise ValueError("incomplete or invalid command timing")
        serial = fields["serial"]
        if serial in commands:
            raise ValueError("duplicate command timing serial")
        commands[serial] = fields
    gpu = benchmark["gpu_times"]
    samples = gpu["samples"]
    count = len(samples)
    if (
        benchmark["benchmark_version"] != 1
        or benchmark["mode"] != "scenario"
        or not benchmark["measurement_complete"]
        or not benchmark["world_complete"]
        or benchmark["unrefined_frames"] != 0
        or not gpu["complete"]
        or count == 0
        or any(gpu[key] != count for key in ("rendered_frames", "timed_frames"))
        or gpu["unavailable_frames"] != 0
        or benchmark["motion"]["frames"] != count
    ):
        raise ValueError("incomplete scenario benchmark GPU measurements")
    durations = [sample["elapsed_ms"] * 1e6 for sample in samples]
    identities = [sample["submission"] for sample in samples]
    stats = gpu["frame"]
    if (
        any(not math.isfinite(value) or value < 0 for value in durations)
        or len({sample["device"] for sample in samples}) != 1
        or any(a >= b for a, b in pairwise(identities))
    ):
        raise ValueError("invalid or unordered benchmark GPU measurements")
    if (
        stats["samples"] != count
        or any(not math.isfinite(stats[key]) for key in ("mean_ms", "max_ms"))
        or abs(stats["mean_ms"] * count * 1e6 - sum(durations)) > 0.5 * count + 1
        or abs(stats["max_ms"] * 1e6 - max(durations)) > 1
    ):
        raise ValueError("benchmark GPU summary differs from its actual samples")
    available = sorted(available)
    matches = []
    for first in range(len(available) - count + 1):
        window = available[first : first + count]
        if all(
            serial in commands
            and commands[serial]["valid"] == 1
            and abs(commands[serial]["gpu_ns"] - duration) <= 0.500001
            for serial, duration in zip(window, durations, strict=True)
        ):
            matches.append(window)
    if len(matches) != 1:
        raise ValueError(
            "benchmark GPU sequence has no unique matching Metal frame window"
        )
    return commands, matches[0], samples


def frame_timings(text, passes, frames, benchmark, compute=None):
    commands, ordered, samples = measured_frames(text, frames, benchmark)
    peak = max(ordered, key=lambda serial: commands[serial]["gpu_ns"])
    selected = []
    for record in sorted(passes.values(), key=lambda entry: entry["pass"]):
        if record["serial"] != peak:
            continue
        selected.append(
            {
                "kind": "raster",
                "pass": record["pass"],
                "pipeline": record["pipeline"],
                "vertex_ms": record["vertex_ns"] / 1e6 if record["valid"] else None,
                "fragment_ms": record["fragment_ns"] / 1e6 if record["valid"] else None,
                "interval_span_ms": (
                    max(record["vertex_end_ns"], record["fragment_end_ns"])
                    - min(record["vertex_start_ns"], record["fragment_start_ns"])
                )
                / 1e6
                if record["valid"]
                else None,
            }
        )
    for record in (compute or {}).values():
        if record["serial"] == peak:
            selected.append(
                {
                    "kind": "compute",
                    "pass": record["pass"],
                    "pipeline": record["pipeline"],
                    "vertex_ms": None,
                    "fragment_ms": None,
                    "interval_span_ms": record["duration_ns"] / 1e6
                    if record["valid"]
                    else None,
                }
            )
    selected.sort(key=lambda row: row["pass"])
    return {
        "capture_matches_command_timings": True,
        "frames": [
            {
                "capture_frame": index + 1,
                "serial": serial,
                "native_submission": samples[index]["submission"],
                "gpu_ms": commands[serial]["gpu_ns"] / 1e6,
            }
            for index, serial in enumerate(ordered)
        ],
        "peak_frame": ordered.index(peak) + 1,
        "peak_serial": peak,
        "peak_passes": selected,
        "scope": "Stage intervals overlap; neither their durations nor their spans are additive. Command timings include diagnostic instrumentation.",
    }


def run(command, path, environment=None, cwd=ROOT):
    with path.open("w") as log:
        result = subprocess.run(
            command,
            cwd=cwd,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
    if result.returncode:
        raise SystemExit(
            f"{shlex.join(command)} exited {result.returncode}; see {path}"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--client", type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--runtime-root", type=Path, default=ROOT)
    parser.add_argument("benchmark", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    args.client = args.client or args.build_dir / "outshine"
    if platform.system() != "Darwin":
        parser.error("this probe uses native Metal on macOS")
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if not any(
        output.is_relative_to(path) for path in (temporary, Path("/tmp").resolve())
    ) or output.is_relative_to(ROOT):
        parser.error("private output must be inside the system temporary directory")
    arguments = args.benchmark
    if arguments and arguments[0] == "--":
        arguments = arguments[1:]
    if not arguments or any(
        value.split("=", 1)[0] in ("--output", "--image") for value in arguments
    ):
        parser.error(
            "pass native scenario benchmark arguments after --; output/image are owned here"
        )
    output.mkdir(parents=True, exist_ok=True)
    library = args.build_dir.resolve() / "probes/metal-work-profile.dylib"
    compile_command = [
        "nice",
        "-n",
        "10",
        "cmake",
        "--build",
        str(args.build_dir.resolve()),
        "--target",
        "test_harness_metal_work_profile",
        "--parallel",
    ]
    run(compile_command, output / "compile.log")
    environment = dict(os.environ)
    existing = environment.get("DYLD_INSERT_LIBRARIES")
    environment["DYLD_INSERT_LIBRARIES"] = str(library) + (
        ":" + existing if existing else ""
    )
    command = [
        str(args.client.resolve()),
        "benchmark",
        *arguments,
        "--output",
        str(output / "benchmark.json"),
        "--image",
        str(output / "image.png"),
    ]
    run(command, output / "client.log", environment, args.runtime_root.resolve())
    text = (output / "client.log").read_text()
    benchmark = json.loads((output / "benchmark.json").read_text())
    passes, work, available = parse_records(text)
    compute, compute_work = parse_compute_records(text)
    timing = frame_timings(text, passes, available, benchmark, compute)
    frames = {frame["serial"] for frame in timing["frames"]}
    result = summarize(passes, work, frames)
    result["frame_timings"] = timing
    result["compute_encoders"] = summarize_compute(compute, compute_work, frames)
    result.update(
        {
            "tool_commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
            ).strip(),
            "tool_status": subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            ).strip(),
            "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "client_sha256": hashlib.sha256(
                args.client.resolve().read_bytes()
            ).hexdigest(),
            "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
            "build_command": compile_command,
            "capture_command": command,
            "runtime_root": str(args.runtime_root.resolve()),
            "metal_debug_layer": environment.get("MTL_DEBUG_LAYER"),
            "truncated": "METAL_TRUNCATED" in text,
            "benchmark_report": str(output / "benchmark.json"),
            "all_pass_records": list(passes.values()),
            "all_work_records": work,
            "all_compute_records": list(compute.values()),
            "all_compute_work_records": compute_work,
            "scope": "Metal stage timestamps and submitted triangle references; instrumented diagnostic, not a device gate",
        }
    )
    receipt = output / "receipt.json"
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "receipt": str(receipt),
                "frames": result["frame_submissions"],
                "peak_frame": timing["peak_frame"],
                "peak_gpu_ms": max(frame["gpu_ms"] for frame in timing["frames"]),
                "raster_groups": len(result["raster_passes"]),
                "compute_groups": len(result["compute_encoders"]),
                "truncated": result["truncated"],
            },
        )
    )


if __name__ == "__main__":
    main()

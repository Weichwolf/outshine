"""Probe native Metal encoder timing and summarize a matched single-place trace."""

import argparse
import hashlib
import json
import math
import shlex
import statistics
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def temporary(path):
    path = path.resolve()
    if path.is_relative_to(ROOT) or not path.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        raise ValueError("use an explicit system-temp output outside the checkout")
    path.mkdir(parents=True, exist_ok=True)
    return path


def run(command, output, label):
    result = subprocess.run(
        command, cwd=ROOT, text=True, capture_output=True, check=False, timeout=120
    )
    (output / (label + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"{label}: exit {result.returncode}; see {output}")
    return result


def probe(output):
    database = json.loads((ROOT / "compile_commands.json").read_text())
    row = next(
        item for item in database if item["file"].endswith("/MetalEncoderProfile.mm")
    )
    if not Path(row["directory"]).resolve().is_relative_to(ROOT):
        raise ValueError("build a compilation database for this checkout first")
    flags = row.get("arguments") or shlex.split(row["command"])
    flags = flags.copy()
    flags.remove(row["file"])
    flags.remove("-c")
    offset = flags.index("-o")
    del flags[offset : offset + 2]
    source = ROOT / "test/harness/render/MetalEncoderProfileProbe.mm"
    obj, binary = output / "probe.o", output / "probe"
    compile_command = [
        *flags,
        "-I" + str(Path(row["file"]).parent),
        "-Itest/harness/shared",
        "-c",
        str(source),
        "-o",
        str(obj),
    ]
    native = subprocess.check_output(
        ["sh", "test/run.sh", "--native-link-flags"], cwd=ROOT, text=True
    )
    link = [
        flags[0],
        str(obj),
        str(ROOT / "build/liboutshine.a"),
        *shlex.split(native),
        "-o",
        str(binary),
    ]
    commands = [
        ("compile", compile_command),
        ("link", link),
        ("run", [str(binary), str(output)]),
    ]
    records = []
    for label, command in commands:
        result = run(command, output, label)
        records.append({"command": command, "status": result.returncode})
    (output / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(ROOT),
                    "file": str(source),
                    "arguments": compile_command,
                }
            ]
        )
    )
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "commands": records,
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "archive_sha256": hashlib.sha256(
                    (ROOT / "build/liboutshine.a").read_bytes()
                ).hexdigest(),
                "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            },
            indent=2,
        )
    )
    print(result.stdout.strip())


def benchmark_window(document, benchmark):
    if document.get("allocation_failures", 0):
        raise ValueError("the diagnostic trace has allocation failures")
    if document.get("dropped_submissions", 0) and document.get("retention") != (
        "latest_completed_submissions"
    ):
        raise ValueError("the diagnostic trace has unknown missing submissions")
    gpu = benchmark["gpu_times"]
    frames = benchmark["motion"]["frames"]
    samples = gpu["samples"]
    if (
        not benchmark["measurement_complete"]
        or not gpu["complete"]
        or frames <= 0
        or gpu["timed_frames"] != frames
        or gpu["rendered_frames"] != frames
        or gpu["unavailable_frames"] != 0
        or len(samples) != frames
        or len({sample["device"] for sample in samples}) != 1
    ):
        raise ValueError("the benchmark must contain every declared frame timestamp")
    serials = [sample["submission"] for sample in samples]
    if serials != sorted(set(serials)):
        raise ValueError("benchmark submissions must be unique and ordered")
    submissions = {row["serial"]: row for row in document["submissions"]}
    if len(submissions) != len(document["submissions"]):
        raise ValueError("trace submission serials must be unique")
    window = []
    for sample in samples:
        row = submissions.get(sample["submission"])
        if row is None:
            raise ValueError("a benchmark submission is absent from the trace")
        duration = sample["elapsed_ms"]
        actual = row["submission_gpu_ns"] / 1e6
        if (
            not math.isfinite(duration)
            or duration < 0
            or not math.isclose(actual, duration, rel_tol=1e-9, abs_tol=1e-6)
        ):
            raise ValueError("trace timing differs from the benchmark submission")
        window.append(row)
    return window


def gpurow_window(document, log):
    rows = [
        line.split("\t")
        for line in log.read_text().splitlines()
        if line.startswith("GPUROW\t")
    ]
    if len(rows) != 1:
        raise ValueError(
            "supply one complete measurement window with exactly one GPUROW"
        )
    gpu = rows[0]
    frames = int(gpu[2])
    if frames <= 0 or int(gpu[3]) != frames or float(gpu[6]) != 0:
        raise ValueError("the GPUROW window must have every declared frame timestamp")
    submissions = sorted(document["submissions"], key=lambda row: row["serial"])
    if len(submissions) < frames:
        raise ValueError("the trace contains fewer submissions than measured frames")
    window = submissions[-frames:]
    total = sum(row["submission_gpu_ns"] for row in window) / 1e6
    if not math.isclose(total, float(gpu[4]), rel_tol=1e-9, abs_tol=1e-6):
        raise ValueError("the trace tail does not match the actual measured GPUROW sum")
    return window


def counter_timeline(window):
    spans, scales = [], []
    for submission in window:
        cpu, gpu = submission["calibration_cpu_ns"], submission["calibration_gpu"]
        if (not all(math.isfinite(x) for x in (*cpu, *gpu))
                or cpu[1] <= cpu[0] or gpu[1] <= gpu[0]):
            raise ValueError("invalid common-clock calibration")
        ratio = (cpu[1] - cpu[0]) / (gpu[1] - gpu[0])
        if not math.isfinite(ratio) or ratio <= 0:
            raise ValueError("invalid counter clock scale")
        scales.append(ratio)
        ticks = []
        for encoder in submission["encoders"]:
            if encoder["kind"] not in ("render", "compute") or not encoder["available"]:
                raise ValueError("encoder counter samples unavailable")
            count = 4 if encoder["kind"] == "render" else 2
            used = encoder["ticks"][:count]
            if (len(used) != count or any(not math.isfinite(t) or t <= 0 for t in used)
                    or any(used[at + 1] < used[at] for at in range(0, len(used), 2))):
                raise ValueError("invalid encoder counter interval")
            ticks.extend(used)
        if not ticks:
            raise ValueError("empty encoder counter envelope")
        spans.append((min(ticks), max(ticks)))
    if not spans:
        raise ValueError("empty counter window")
    scale = statistics.median(scales) / 1e6
    merged = []
    for first, last in sorted(spans):
        if merged and first <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], last)
        else:
            merged.append([first, last])
    total = sum(last - first for first, last in spans) * scale
    union = sum(last - first for first, last in merged) * scale
    intervals = [(b[1] - a[1]) * scale for a, b in zip(spans, spans[1:])]
    ordered = all(value > 0 for value in intervals)
    values = sorted(intervals) if ordered else []
    return {
        "scope": "Encoder envelopes exclude unprofiled commands; neither utilization nor a budget verdict.",
        "clock": "same-device raw ticks, median calibration scale",
        "calibration_ns_per_tick_min_max": [min(scales), max(scales)],
        "envelope_sum_ms": total, "envelope_union_ms": union,
        "overlap_ms": total - union,
        "ordered_encoder_endings": ordered,
        "encoder_end_intervals": {
            "samples": len(values),
            **{f"p{percentile}_ms": values[math.ceil(len(values) * percentile / 100) - 1]
               if values else None for percentile in (50, 95, 99)}},
    }


def analyse(trace, log, output, benchmark=None):
    document = json.loads(trace.read_text())
    if document["allocation_failures"] or (
        document["dropped_submissions"] and benchmark is None
    ):
        raise ValueError("the diagnostic trace is incomplete")
    window = (
        benchmark_window(document, json.loads(benchmark.read_text()))
        if benchmark is not None
        else gpurow_window(document, log)
    )
    frames = len(window)
    total = sum(row["submission_gpu_ns"] for row in window) / 1e6
    groups = {}
    for submission in window:
        if submission["dropped_encoders"]:
            raise ValueError(
                "a measured submission exceeds the encoder sample capacity"
            )
        cpu = submission["calibration_cpu_ns"]
        clock = submission["calibration_gpu"]
        if cpu[1] <= cpu[0] or clock[1] <= clock[0]:
            raise ValueError("the raw counter clock cannot be calibrated")
        scale = (cpu[1] - cpu[0]) / (clock[1] - clock[0]) / 1e6
        for ordinal, encoder in enumerate(submission["encoders"]):
            if not encoder["available"]:
                raise ValueError("a measured encoder has unavailable counter samples")
            key = (
                ordinal,
                encoder["kind"],
                encoder["label"],
                encoder["width"],
                encoder["height"],
                encoder["colours"],
                encoder["depth"],
            )
            if key not in groups:
                groups[key] = {
                    "ordinal": ordinal,
                    "kind": encoder["kind"],
                    "label": encoder["label"],
                    "width": encoder["width"],
                    "height": encoder["height"],
                    "colours": encoder["colours"],
                    "depth": encoder["depth"],
                    "calls": 0,
                    "vertex_or_compute_ms": 0.0,
                    "fragment_ms": 0.0,
                }
            group = groups[key]
            times = encoder["ticks"]
            group["calls"] += 1
            group["vertex_or_compute_ms"] += (times[1] - times[0]) * scale
            if encoder["kind"] == "render":
                group["fragment_ms"] += (times[3] - times[2]) * scale
    result = {
        "frames": frames,
        "submission_gpu_ms": total,
        "first_serial": window[0]["serial"],
        "last_serial": window[-1]["serial"],
        "collect_cpu_ms_all_submissions": document["collect_cpu_ns"] / 1e6,
        "scope": "Counter stages overlap; do not sum them or substitute CPU waits.",
        "overhead": "Collection only; allocation and GPU sampling cost not isolated.",
        "counter_timeline": counter_timeline(window),
        "encoders": list(groups.values()),
    }
    for group in result["encoders"]:
        group["mean_vertex_or_compute_ms"] = group["vertex_or_compute_ms"] / frames
        group["mean_fragment_ms"] = group["fragment_ms"] / frames
    (output / "summary.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    case = commands.add_parser("probe")
    case.add_argument("--output", type=Path, required=True)
    report = commands.add_parser("analyse")
    report.add_argument("--trace", type=Path, required=True)
    source = report.add_mutually_exclusive_group(required=True)
    source.add_argument("--log", type=Path)
    source.add_argument("--benchmark", type=Path)
    report.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = temporary(args.output)
    if args.command == "probe":
        probe(output)
    else:
        analyse(
            args.trace.resolve(),
            args.log.resolve() if args.log else None,
            output,
            args.benchmark.resolve() if args.benchmark else None,
        )


if __name__ == "__main__":
    main()

"""Probe native Metal encoder timing and summarize a matched single-place trace."""

import argparse
import hashlib
import json
import math
import shlex
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
    if Path(row["directory"]).resolve() != ROOT:
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


def analyse(trace, log, output):
    document = json.loads(trace.read_text())
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
    if document["dropped_submissions"] or document["allocation_failures"]:
        raise ValueError("the diagnostic trace is incomplete")
    submissions = sorted(document["submissions"], key=lambda row: row["serial"])
    if len(submissions) < frames:
        raise ValueError("the trace contains fewer submissions than measured frames")
    window = submissions[-frames:]
    total = sum(row["submission_gpu_ns"] for row in window) / 1e6
    if not math.isclose(total, float(gpu[4]), rel_tol=1e-9, abs_tol=1e-6):
        raise ValueError("the trace tail does not match the actual measured GPUROW sum")
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
    report.add_argument("--log", type=Path, required=True)
    report.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = temporary(args.output)
    if args.command == "probe":
        probe(output)
    else:
        analyse(args.trace.resolve(), args.log.resolve(), output)


if __name__ == "__main__":
    main()

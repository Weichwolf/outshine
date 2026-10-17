"""Profile native Metal Place passes and submitted triangle references outside the product."""

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
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "test/harness/render/MetalWorkProfile.mm"


def compiler_command():
    records = json.loads((ROOT / "compile_commands.json").read_text())
    record = next(row for row in records
                  if Path(row["file"]).name == "MetalCommands.mm")
    flags = record.get("arguments") or shlex.split(record["command"])
    command = []
    skip = False
    for flag in flags:
        if skip:
            skip = False
        elif flag == "-o":
            skip = True
        elif flag != "-c" and Path(flag).resolve() != Path(record["file"]).resolve():
            command.append(flag)
    return command


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
            passes[record["serial"], record["pass"]] = record
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


def frame_timings(text, passes, frames):
    commands = {}
    for line in text.splitlines():
        if not line.startswith("METAL_COMMAND "):
            continue
        fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        serial = fields["serial"]
        if serial in commands:
            raise ValueError("duplicate command timing serial")
        commands[serial] = fields
    if not frames or any(serial not in commands or not commands[serial]["valid"] for serial in frames):
        raise ValueError("incomplete measured frame command timings")
    gpu_rows = [line.split("\t") for line in text.splitlines() if line.startswith("GPUROW\t")]
    if len(gpu_rows) != 1:
        raise ValueError("expected one capture GPU summary")
    row = gpu_rows[0]
    values = list(map(float, row[2:]))
    if len(values) != 6 or not all(math.isfinite(value) for value in values):
        raise ValueError("invalid capture GPU summary")
    rendered, timed, total, maximum, unavailable, _ = values
    ordered = sorted(frames)
    samples = [commands[serial]["gpu_ns"] for serial in ordered]
    if rendered != len(frames) or timed != len(frames) or unavailable != 0:
        raise ValueError("capture and command frame counts differ")
    if abs(sum(samples) - total * 1e6) > 0.5 * len(frames) + 1 or abs(max(samples) - maximum * 1e6) > 1:
        raise ValueError("capture totals do not match measured frame submissions")
    peak = max(ordered, key=lambda serial: commands[serial]["gpu_ns"])
    selected = []
    for record in sorted(passes.values(), key=lambda entry: entry["pass"]):
        if record["serial"] != peak:
            continue
        selected.append({
            "pass": record["pass"], "pipeline": record["pipeline"],
            "vertex_ms": record["vertex_ns"] / 1e6,
            "fragment_ms": record["fragment_ns"] / 1e6,
            "interval_span_ms": (max(record["vertex_end_ns"], record["fragment_end_ns"])
                                 - min(record["vertex_start_ns"], record["fragment_start_ns"])) / 1e6
            if record["valid"] else None,
        })
    return {
        "capture_matches_command_timings": True,
        "frames": [{"capture_frame": index + 1, "serial": serial, "gpu_ms": commands[serial]["gpu_ns"] / 1e6}
                   for index, serial in enumerate(ordered)],
        "peak_frame": ordered.index(peak) + 1, "peak_serial": peak, "peak_passes": selected,
        "scope": "Stage intervals overlap; neither their durations nor their spans are additive. Command timings include diagnostic instrumentation.",
    }


def run(command, path, environment=None):
    with path.open("w") as log:
        result = subprocess.run(
            command,
            cwd=ROOT,
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
    parser.add_argument("--client", type=Path, default=ROOT / "build/outshine-client")
    parser.add_argument("--place", default="Tokyo")
    parser.add_argument("--overrides", default='{"render":{"fps":30}}')
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("this probe uses native Metal on macOS")
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if not output.is_relative_to(temporary) or output.is_relative_to(ROOT):
        parser.error("private output must be inside the system temporary directory")
    overrides = json.loads(args.overrides)
    output.mkdir(parents=True, exist_ok=True)
    library = output / "metal-work-profile.dylib"
    compile_command = [
        "nice",
        "-n",
        "10",
        *compiler_command(),
        "-dynamiclib",
        str(SOURCE),
        "-framework",
        "Metal",
        "-framework",
        "Foundation",
        "-o",
        str(library),
    ]
    run(compile_command, output / "compile.log")
    environment = dict(os.environ)
    existing = environment.get("DYLD_INSERT_LIBRARIES")
    environment["DYLD_INSERT_LIBRARIES"] = str(library) + (
        ":" + existing if existing else ""
    )
    command = [
        str(args.client.resolve()),
        "shots",
        "--offline",
        "--rows",
        "--stats",
        "--scenario-overrides",
        json.dumps(overrides),
        args.place,
    ]
    run(command, output / "client.log", environment)
    text = (output / "client.log").read_text()
    rows = [line.split("\t") for line in text.splitlines() if line.startswith("ROW\t")]
    passes, work, frames = parse_records(text)
    if len(rows) != 1 or rows[0][3] != "1" or len(frames) != int(rows[0][7]):
        raise SystemExit(
            "the successful Place capture and measured sky-pass submissions differ"
        )
    result = summarize(passes, work, frames)
    result["frame_timings"] = frame_timings(text, passes, frames)
    result.update(
        {
            "commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
            ).strip(),
            "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "client_sha256": hashlib.sha256(
                args.client.resolve().read_bytes()
            ).hexdigest(),
            "compile_command": compile_command,
            "capture_command": command,
            "metal_debug_layer": environment.get("MTL_DEBUG_LAYER"),
            "truncated": "METAL_TRUNCATED" in text,
            "row": rows[0],
            "all_pass_records": list(passes.values()),
            "all_work_records": work,
            "scope": "Metal stage timestamps and submitted triangle references; instrumented diagnostic, not a device gate",
        }
    )
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "frame_submissions",
                    "raster_passes",
                    "shader_triangle_references_mean",
                )
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

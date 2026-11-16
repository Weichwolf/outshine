#!/usr/bin/env python3
"""Render the test catalogue through Outshine's generic scenario benchmark."""

import argparse
import hashlib
import json
import math
import os
import re
import stat
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from place_turn import turn_scenario

ROOT = Path(__file__).resolve().parents[2]


def keep_latest(image, directory, name):
    data = image.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    directory.mkdir(parents=True, exist_ok=True)
    latest = directory / f"{name}-{digest[:8]}.png"
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=directory, prefix=".place-", delete=False
        ) as output:
            temporary = Path(output.name)
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        temporary.replace(latest)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    pattern = re.compile(re.escape(name) + r"-[0-9a-f]{8}\.png")
    for old in directory.iterdir():
        if (
            old != latest
            and pattern.fullmatch(old.name)
            and stat.S_ISREG(old.lstat().st_mode)
        ):
            old.unlink()
    return latest, digest


def benchmark_report(prepared, view, options, work, frames, angular="0,0,0",
                     linear="0,0,0", timeout=240):
    work.mkdir(parents=True, exist_ok=True)
    image = work / "image.png"
    report_path = work / "benchmark.json"
    image.unlink(missing_ok=True)
    report_path.unlink(missing_ok=True)
    command = [
        str(options.client),
        "benchmark",
        "--scenario",
        str(prepared),
        "--view",
        view,
        "--frames",
        str(frames),
        "--angular-velocity",
        angular,
        "--linear-velocity",
        linear,
        "--preload-seconds",
        str(options.preload_seconds),
        "--image",
        str(image),
        "--output",
        str(report_path),
    ]
    if not options.online:
        command.append("--offline")
    if options.cache_dir:
        command.extend(("--cache-dir", str(options.cache_dir)))
    (work / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (work / "run.log").open("w") as log:
        environment = dict(os.environ)
        if getattr(options, "bench", False) and options.bench_profile != "a18":
            environment["OUTSHINE_VULKAN_DEVICE"] = options.device or {
                "rtx3060": "RTX 3060", "iris": "Iris"
            }[options.bench_profile]
        result = subprocess.run(
            command,
            check=False,
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            env=environment,
            timeout=min(options.preload_seconds + 120, timeout),
        )
    if result.returncode:
        raise RuntimeError(
            f"{work.name}: benchmark exited {result.returncode}; {work / 'run.log'}"
        )
    report = json.loads(report_path.read_text())
    if report.get("benchmark_version") != 2 or report.get("mode") != "scenario":
        raise RuntimeError(f"{work.name}: unsupported benchmark report; {report_path}")
    return report


def capture(source, options):
    name = source.stem
    if re.fullmatch(r"[A-Za-z0-9_-]+", name) is None:
        raise ValueError(f"{source}: invalid capture name")
    work = options.reports / name
    work.mkdir(parents=True, exist_ok=True)
    declaration, view, velocity = turn_scenario(source, options.fps)
    prepared = work / "scene.scenario"
    prepared.write_text(declaration)
    report = benchmark_report(prepared, view, options, work, options.fps, velocity)
    report_path = work / "benchmark.json"
    image = work / "image.png"
    if (
        not report["measurement_complete"]
        or not report["world_complete"]
        or report["unrefined_frames"]
    ):
        raise RuntimeError(f"{name}: incomplete world or measurement; {report_path}")
    if (
        report["motion"]["frames"] != options.fps
        or report["profile"]["fps"] != options.fps
    ):
        raise RuntimeError(f"{name}: benchmark did not measure one target-rate second")
    start = report["motion"]["start"]["world_from_camera"]
    finish = report["motion"]["finish"]["world_from_camera"]
    same_pose = (
        len(start) == 16
        and len(finish) == 16
        and all(
            math.isfinite(a)
            and math.isfinite(b)
            and abs(a - b) <= 64 * math.ulp(max(1.0, abs(a), abs(b)))
            for a, b in zip(start, finish)
        )
    )
    if not same_pose:
        raise RuntimeError(f"{name}: complete turn did not return to the starting pose")
    saved, digest = keep_latest(image, options.shots, name)
    gpu = report["gpu_times"]["frame"]
    return {
        "place": name,
        "image": str(saved),
        "sha256": digest,
        "frames": options.fps,
        "turn_degrees": 360,
        "cpu_p99_ms": report["cpu_times"]["frame"]["p99_ms"],
        "gpu_p99_ms": None if gpu is None else gpu["p99_ms"],
        "preload_ms": report["phases_ms"]["preload"],
        "budget": report["budget"],
        "report": str(report_path),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*")
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument(
        "--client",
        type=Path,
        default=Path(os.environ.get("OUTSHINE_CLIENT", ROOT / "build/outshine")),
    )
    parser.add_argument(
        "--cache-dir", type=Path, default=os.environ.get("OUTSHINE_CACHE_DIR")
    )
    parser.add_argument("--shots", type=Path, default=ROOT / "build/shots/places")
    parser.add_argument("--reports", type=Path)
    parser.add_argument("--fps", type=int, choices=(25, 30, 60), default=60)
    parser.add_argument("--preload-seconds", type=float, default=120)
    parser.add_argument("--online", action="store_true")
    parser.add_argument("--check-budget", action="store_true")
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--bench-profile", choices=("rtx3060", "iris", "a18"), default="rtx3060")
    parser.add_argument("--driver", help="selected GPU driver version, if not available from Vulkan")
    parser.add_argument("--device", help="Vulkan device name substring (default: GPU of bench profile)")
    parser.add_argument("--max-seconds", type=float, default=900)
    options = parser.parse_args()
    if not math.isfinite(options.max_seconds) or options.max_seconds <= 0:
        parser.error("--max-seconds must be finite and positive")
    if not math.isfinite(options.preload_seconds) or options.preload_seconds <= 0:
        parser.error("--preload-seconds must be finite and positive")
    options.client = options.client.resolve()
    options.catalog = options.catalog.resolve()
    options.shots = options.shots.resolve()
    if options.reports is None:
        commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                                         text=True).strip()
        role = "mac" if options.bench and options.bench_profile == "a18" else "linux"
        options.reports = Path(tempfile.mkdtemp(prefix=f"outshine-{role}-places-{commit}-"))
    options.reports = options.reports.resolve()
    available = {path.stem: path for path in sorted(options.catalog.glob("*.scenario"))}
    selected = options.names or list(available)
    if not selected or any(name not in available for name in selected):
        parser.error("catalogue is empty or a requested place is missing")
    if options.bench:
        from place_bench import benchmark_places

        return benchmark_places(available, selected, options, benchmark_report)
    results = []
    for name in selected:
        try:
            result = capture(available[name], options)
        except (
            OSError,
            ValueError,
            KeyError,
            TypeError,
            RuntimeError,
            subprocess.TimeoutExpired,
            ET.ParseError,
        ) as error:
            parser.exit(1, f"places: {error}\n")
        results.append(result)
        print(json.dumps(result), flush=True)
    (options.reports / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return int(
        options.check_budget and any(not row["budget"]["met"] for row in results)
    )


if __name__ == "__main__":
    raise SystemExit(main())

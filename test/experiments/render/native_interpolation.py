"""Replay fixed native interpolation inputs through unchanged product shaders.

Build a matching native archive and compile_commands.json first. Exit 6 means that
the GPU disagreed with the independent CPU motion reference; it is never a pass.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "test/outshine/data/render/interpolation"


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def git(root, *args):
    return subprocess.check_output(["git", *args], cwd=root, text=True).strip()


def compile_command(checkout, output, source):
    database = json.loads((checkout / "compile_commands.json").read_text())
    entry = next((row for row in database if Path(row["file"]).name == "StagePipelines.cpp"), None)
    if entry is None:
        raise ValueError("compilation database has no native StagePipelines.cpp record")
    if Path(entry["directory"]).resolve() != checkout:
        raise ValueError("compilation database belongs to a different checkout")
    args = entry.get("arguments") or shlex.split(entry["command"])
    args = list(args)
    original = Path(entry["file"])
    if not original.is_absolute():
        original = checkout / original
    at = next((i for i, arg in enumerate(args)
               if (checkout / arg).resolve() == original.resolve()), None)
    if at is None:
        raise ValueError("compilation record does not contain its source path")
    args[at] = str(source)
    if "-c" not in args or "-o" not in args:
        raise ValueError("expected a complete object compilation command")
    args[args.index("-o") + 1] = str(output / "probe.o")
    for flag, name in (("-MF", "probe.d"), ("-MJ", "probe-command.json")):
        if flag in args:
            args[args.index(flag) + 1] = str(output / name)
    return args, entry


def execute(args, checkout, output, name):
    marker = output / (name + "-timeout")
    Path(str(marker) + ".start").touch()
    command = [sys.executable, str(ROOT / "test/scripts/run_with_timeout.py"),
               str(os.getpid()), "120", str(output / (name + ".log")), str(marker), *args]
    began = time.monotonic()
    with (output / (name + "-supervisor.log")).open("w") as log:
        result = subprocess.run(command, cwd=checkout, stdout=log, stderr=subprocess.STDOUT,
                                check=False)
    return {"command": args, "exit_code": result.returncode,
            "wall_s": time.monotonic() - began,
            "timeout": marker.exists(),
            "cleanup_failed": Path(str(marker) + ".cleanup-failed").exists()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--iterations", type=int, default=1024)
    parser.add_argument("--device-name", help="reject a different native GPU")
    args = parser.parse_args()
    checkout, output = args.checkout.resolve(), args.output.resolve()
    temporary_roots = {Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve()}
    if (not any(output != root and output.is_relative_to(root) for root in temporary_roots)
            or output.is_relative_to(checkout) or output.is_relative_to(ROOT)):
        parser.error("private outputs must be in a system temporary directory outside checkouts")
    if not 1 <= args.iterations <= 4096:
        parser.error("iterations must be between 1 and 4096")
    archive = checkout / "build/liboutshine.a"
    shaders = [checkout / "build/shaders" / name
               for name in ("lit-200-1.vert.spv", "lit-010-100.frag.spv")]
    if not archive.is_file() or not all(path.is_file() for path in shaders):
        parser.error("build the native product archive and shader package first")
    source = Path(__file__).with_suffix(".cpp")
    manifest = json.loads((FIXTURE / "manifest.json").read_text())
    for name, expected in manifest["files"].items():
        path = FIXTURE / name
        if path.stat().st_size != expected["bytes"] or digest(path) != expected["sha256"]:
            parser.error("fixed input differs from its provenance manifest: " + name)
    output.mkdir(parents=True, exist_ok=False)
    receipt = output / "report.json"
    record = {"status": "running", "scope": "native correctness diagnostic; no FPS or image admission",
              "source_checkout": str(ROOT), "source_commit": git(ROOT, "rev-parse", "HEAD"),
              "source_dirty": git(ROOT, "status", "--porcelain"),
              "native_checkout": str(checkout), "native_commit": git(checkout, "rev-parse", "HEAD"),
              "native_dirty": git(checkout, "status", "--porcelain"),
              "archive_sha256": digest(archive), "source_sha256": digest(source),
              "input_manifest_sha256": digest(FIXTURE / "manifest.json"),
              "platform": sys.platform,
              "environment": {key: os.environ[key] for key in
                              ("VK_DRIVER_FILES", "VK_ICD_FILENAMES", "VK_INSTANCE_LAYERS",
                               "INTEL_DEBUG", "MESA_SHADER_CACHE_DISABLE", "MTL_DEBUG_LAYER")
                              if key in os.environ},
              "shader_sha256": {path.name: digest(path) for path in shaders}, "phases": []}
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    code = 2
    try:
        command, entry = compile_command(checkout, output, source)
        record["original_compilation_record"] = entry
        phase = execute(["nice", "-n", "10", *command], checkout, output, "compile")
        record["phases"].append(phase)
        if phase["exit_code"]:
            raise RuntimeError("native probe compilation failed")
        libraries = shlex.split(subprocess.check_output(
            ["sh", "test/run.sh", "--native-link-flags"], cwd=checkout, text=True))
        libraries += shlex.split(subprocess.check_output(
            ["pkg-config", "--libs", "sdl3"], cwd=checkout, text=True))
        link_flags = [flag for flag in command
                      if flag.startswith(("-stdlib=", "-fsanitize=", "-fno-sanitize=", "-fuse-ld="))]
        binary = output / "native-interpolation"
        link = [command[0], *link_flags, str(output / "probe.o"), str(archive), *libraries,
                "-o", str(binary)]
        phase = execute(["nice", "-n", "10", *link], checkout, output, "link")
        record["phases"].append(phase)
        if phase["exit_code"]:
            raise RuntimeError("native probe link failed")
        run = [str(binary), str(FIXTURE), *(str(path) for path in shaders), str(args.iterations)]
        phase = execute(run, checkout, output, "run")
        record["phases"].append(phase)
        log = (output / "run.log").read_text(errors="replace")
        results = re.findall(r"^RESULT frames=(\d+) bad_frames=(\d+) checks=(\d+) "
                             r"bad_components=(\d+) max_ndc_error=(\S+)$", log, re.MULTILINE)
        devices = re.findall(r"^DEVICE (.+)$", log, re.MULTILINE)
        if (phase["exit_code"] not in (0, 6) or phase["timeout"] or phase["cleanup_failed"]
                or len(results) != 1 or len(devices) != 1 or "VUID-" in log):
            raise RuntimeError("native run has an invalid or incomplete verdict; inspect run.log")
        if args.device_name and devices[0] != args.device_name:
            raise RuntimeError("the actual native device differs from --device-name")
        frames, bad_frames, checks, bad_components, maximum = results[0]
        if (int(frames) != args.iterations or int(checks) != args.iterations * 2
                or (phase["exit_code"] == 0) != (int(bad_components) == 0)):
            raise RuntimeError("native counters disagree with the terminal result")
        code = phase["exit_code"]
        finite_error = math.isfinite(float(maximum))
        record.update(verdict="failed" if code else "passed", device=devices[0],
                      frames=int(frames), checks=int(checks), bad_frames=int(bad_frames),
                      bad_components=int(bad_components), nonfinite_error=not finite_error,
                      max_ndc_error=float(maximum) if finite_error else None)
        print(f"{devices[0]}: {bad_frames}/{frames} bad frames; report: {receipt}")
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        record.update(verdict="invalid", error=str(error))
        print(f"{error}; report: {receipt}", file=sys.stderr)
    finally:
        record.update(status="terminal", exit_code=code)
        receipt.write_text(json.dumps(record, indent=2) + "\n")
    return code


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Run the product calibration with external timeout and provenance; no duplicate native build."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def manifest(root):
    names = [
        "calibrationFma.comp",
        "calibrationTriad.comp",
        "calibrationRaster.vert",
        "calibrationRaster.frag",
    ]
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "source_dirty": subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=root, text=True
        ),
        "binary_sha256": digest(root / "build/outshine"),
        "archive_sha256": digest(root / "build/liboutshine.a"),
        "shader_sha256": {
            n + ".spv": digest(root / "build/shaders" / (n + ".spv")) for n in names
        },
    }


def run(root, output, baseline, host_load):
    current = manifest(root)
    if current != baseline:
        raise ValueError("calibration inputs or build artifacts changed; build again")
    args = [
        str(root / "build/outshine"),
        "benchmark",
        "--output",
        str(output / "run.json"),
    ]
    environment = os.environ.copy()
    validation = environment.get("OUTSHINE_CALIBRATION_VALIDATION") == "1"
    if validation:
        args.append("--gpu-validation")
        if sys.platform == "darwin":
            environment["MTL_DEBUG_LAYER"] = "1"
    marker = output / "timeout"
    for suffix in ["", ".start", ".cleanup-failed"]:
        Path(str(marker) + suffix).unlink(missing_ok=True)
    command = [
        sys.executable,
        str(root / "test/scripts/run_with_timeout.py"),
        str(os.getpid()),
        "120",
        str(output / "run.log"),
        str(marker),
        "--stderr-file",
        str(output / "stderr.log"),
        *args,
    ]
    began = time.monotonic()
    with (output / "supervisor.log").open("w") as log:
        child = subprocess.Popen(
            command, cwd=root, stdout=log, stderr=subprocess.STDOUT, env=environment
        )
        Path(str(marker) + ".start").touch()
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
    receipt = dict(
        baseline,
        command=args,
        exit=child.returncode,
        seconds=time.monotonic() - began,
        host_load=host_load,
        timed_out=marker.exists(),
        cleanup_failed=Path(str(marker) + ".cleanup-failed").exists(),
        supervisor_peak_rss_bytes=usage.ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
    )
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if child.returncode or receipt["timed_out"] or receipt["cleanup_failed"]:
        raise RuntimeError(f"calibration failed: {output}")
    messages = (output / "stderr.log").read_text().splitlines()
    notice = (
        sys.platform == "darwin"
        and validation
        and len(messages) == 1
        and messages[0].endswith(" Metal API Validation Enabled")
    )
    if messages and not notice:
        raise RuntimeError(f"calibration reported diagnostics: {output / 'stderr.log'}")
    report = json.loads((output / "run.json").read_text())
    report["provenance"] = receipt
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(output),
                "workloads": len(report["workloads"]),
                "cpu": report["cpu"],
                "gpu": report["gpu"],
                "seconds": receipt["seconds"],
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--host-load", required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--build-only", action="store_true")
    modes.add_argument("--run-only", action="store_true")
    args = parser.parse_args()
    root, output = args.engine_root.resolve(), args.output.resolve()
    if not output.is_relative_to(Path(os.environ.get("TMPDIR", "/tmp")).resolve()):
        parser.error("private outputs must be under the system temporary directory")
    output.mkdir(parents=True, exist_ok=True)
    if not args.run_only:
        with (output / "build.log").open("w") as log:
            subprocess.run(
                ["nice", "-n", "10", "make", "all", "db"],
                cwd=root,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
            )
        (output / "manifest.json").write_text(
            json.dumps(manifest(root), indent=2) + "\n"
        )
    if not args.build_only:
        run(
            root,
            output,
            json.loads((output / "manifest.json").read_text()),
            args.host_load,
        )


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

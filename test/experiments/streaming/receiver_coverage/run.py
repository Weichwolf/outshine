"""Build the standalone CPU union comparison with complete native CDB flags."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
import time
from pathlib import Path

SOURCE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, directory, output, name, steps):
    row = {"name": name, "command": list(map(str, command)), "returncode": None}
    steps.append(row)
    start = time.monotonic()
    try:
        with (output / f"{name}.log").open("w") as log:
            result = subprocess.run(
                row["command"],
                cwd=directory,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=False,
            )
        row["returncode"] = result.returncode
        if result.returncode:
            raise RuntimeError(f"{name} failed: {result.returncode}")
    finally:
        row["seconds"] = time.monotonic() - start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-database", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--side", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    database = args.compile_database.resolve()
    entry = next(
        r
        for r in json.loads(database.read_text())
        if Path(r["file"]).name == "PreparedRoadManifests.cpp"
    )
    directory = Path(entry["directory"])
    command = list(entry.get("arguments") or shlex.split(entry["command"]))
    command[command.index(entry["file"])] = str(SOURCE / "Probe.cpp")
    command[command.index("-o") + 1] = str(output / "Probe.o")
    for flag, suffix in (("-MF", ".d"), ("-MJ", "-command.json")):
        if flag in command:
            command[command.index(flag) + 1] = str(output / f"Probe{suffix}")
    (output / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(directory),
                    "file": str(SOURCE / "Probe.cpp"),
                    "arguments": command,
                }
            ],
            indent=2,
        )
        + "\n"
    )
    steps = []
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=SOURCE, text=True
            )
        ),
        "sources_sha256": {
            str(p): digest(p) for p in sorted(SOURCE.iterdir()) if p.is_file()
        },
        "compile_database_sha256": digest(database),
        "trace_sha256": digest(args.trace),
        "side": args.side,
        "load_before": list(os.getloadavg()),
        "steps": steps,
        "success": False,
        "scope": "Standalone CPU rectangle union; no native engine archive, GPU work or asset-page suppression",
    }
    try:
        execute(["nice", "-n", "10", *command], directory, output, "compile", steps)
        flags = [
            f
            for f in command
            if f.startswith(
                ("-stdlib=", "-std=", "-fsanitize=", "-fno-sanitize=", "-fuse-ld=")
            )
        ]
        execute(
            [
                "nice",
                "-n",
                "10",
                command[0],
                *flags,
                output / "Probe.o",
                "-o",
                output / "probe",
            ],
            directory,
            output,
            "link",
            steps,
        )
        receipt["binary_sha256"] = digest(output / "probe")
        execute(
            [output / "probe", args.trace.resolve(), str(args.side), output],
            directory,
            output,
            "run",
            steps,
        )
        receipt["load_after"] = list(os.getloadavg())
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()

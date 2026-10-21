"""Build and run the isolated native coverage experiment against a prepared archive."""

import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

SOURCE = Path(__file__).resolve().parent


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def execute(command, checkout, output, name, records):
    row = {"name": name, "command": list(map(str, command)), "returncode": None}
    records.append(row)
    began = time.monotonic()
    try:
        with (output / f"{name}.log").open("w") as log:
            result = subprocess.run(
                row["command"],
                cwd=checkout,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=180,
                check=False,
            )
        row["returncode"] = result.returncode
        if result.returncode:
            raise RuntimeError(
                f"{name} exited {result.returncode}; see {output}/{name}.log"
            )
    finally:
        row["seconds"] = time.monotonic() - began


def build(checkout, database, output, records):
    entry = next(
        row
        for row in json.loads(database.read_text())
        if Path(row["file"]).name == "Hardware.cpp"
        and "diagnostics/calibration" in row["file"]
    )
    if Path(entry["directory"]).resolve() != checkout:
        raise ValueError("compilation database belongs to another checkout")
    command = list(entry.get("arguments") or shlex.split(entry["command"]))
    source = SOURCE / "Probe.cpp"
    command[command.index(entry["file"])] = str(source)
    command[command.index("-o") + 1] = str(output / "Probe.o")
    for flag, name in (("-MF", "Probe.d"), ("-MJ", "Probe-command.json")):
        if flag in command:
            command[command.index(flag) + 1] = str(output / name)
    (output / "compile_commands.json").write_text(
        json.dumps(
            [{"directory": str(checkout), "file": str(source), "arguments": command}],
            indent=2,
        )
        + "\n"
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
    binary = output / "coverage-probe"
    execute(
        [
            "nice",
            "-n",
            "10",
            command[0],
            "-std=c++23",
            *flags,
            str(output / "Probe.o"),
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
        raise ValueError("glslangValidator is required")
    for name in ("coverage.vert", "coverage.frag"):
        execute(
            [
                "nice",
                "-n",
                "10",
                str(compiler),
                "-V",
                "--target-env",
                "vulkan1.3",
                str(SOURCE / name),
                "-o",
                str(output / f"{name}.spv"),
            ],
            checkout,
            output,
            name,
            records,
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-checkout", required=True, type=Path)
    parser.add_argument("--compile-database", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seed", default=990, type=int)
    parser.add_argument("--tessellated", action="store_true")
    args = parser.parse_args()
    if not 0 <= args.seed < 2**32:
        parser.error("seed must be a uint32")
    checkout = args.native_checkout.resolve()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty; existing evidence is preserved")
    database = (args.compile_database or checkout / "compile_commands.json").resolve()
    records = []
    receipt = {
        "native_checkout": str(checkout),
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True
        ).strip(),
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=SOURCE, text=True
            )
        ),
        "native_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=checkout, text=True
            )
        ),
        "source_sha256": {
            path.name: digest(path)
            for path in (
                SOURCE / "Probe.cpp",
                SOURCE / "coverage.vert",
                SOURCE / "coverage.frag",
                Path(__file__),
            )
        },
        "archive_sha256": digest(checkout / "build/liboutshine.a"),
        "compile_database_sha256": digest(database),
        "steps": records,
        "success": False,
    }
    try:
        shaders(checkout, output, records)
        binary = build(checkout, database, output, records)
        receipt["binary_sha256"] = digest(binary)
        command = [binary, output, output / "result.json", str(args.seed)]
        if args.tessellated:
            command.append("--tessellated")
        execute(
            command,
            checkout,
            output,
            "run",
            records,
        )
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()

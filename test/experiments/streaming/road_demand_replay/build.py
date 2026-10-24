#!/usr/bin/env python3
"""Link a Linux road-demand replay probe against a successfully built frozen client."""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path


def run_stage(command, name, output, directory, receipts):
    with (output / f"{name}.log").open("w") as log:
        result = subprocess.run(
            ["nice", "-n", "10", *command],
            cwd=directory,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=False,
        )
    receipts.append(
        {
            "stage": name,
            "command": list(command),
            "directory": directory,
            "priority": 10,
            "returncode": result.returncode,
        }
    )
    (output / "build.json").write_text(json.dumps(receipts, indent=2) + "\n")
    print(name, result.returncode, flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", type=Path, required=True)
    parser.add_argument("--client-link", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("this experiment uses Linux linker wrapping")
    temporary = Path(os.environ.get("TMPDIR", "/tmp")).resolve()
    output = args.out.resolve()
    if not output.is_relative_to(temporary) or output == temporary:
        parser.error("--out must be a dedicated system-temp directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("--out must be empty; existing evidence is preserved")
    link = json.loads(args.client_link.read_text())
    if link.get("returncode") != 0:
        parser.error("--client-link requires a successful frozen native link receipt")
    rows = json.loads(args.compile_commands.read_text())
    row = next(row for row in rows if Path(row["file"]).name == "Engine.cpp")
    folder = Path(__file__).resolve().parent
    obj = output / "ConsumerReplay.o"
    cflags = shlex.split(
        subprocess.check_output(
            ["pkg-config", "--cflags", "libxxhash", "libzstd"], text=True
        )
    )
    cdb = []
    for name in ("ConsumerReplay", "GuardCheck"):
        source, target = folder / f"{name}.cpp", output / f"{name}.o"
        command = list(row.get("arguments") or shlex.split(row["command"]))
        command[command.index(row["file"])] = str(source)
        command[command.index("-o") + 1] = str(target)
        for flag, value in (
            ("-MF", output / f"{name}.d"),
            ("-MT", target),
            ("-MQ", target),
        ):
            if flag in command:
                command[command.index(flag) + 1] = str(value)
        command.extend(cflags)
        cdb.append(
            {"directory": row["directory"], "file": str(source), "arguments": command}
        )
    (output / "compile_commands.json").write_text(json.dumps(cdb, indent=2) + "\n")
    receipts = []
    for entry in cdb:
        run_stage(
            entry["arguments"],
            "compile-" + Path(entry["file"]).stem,
            output,
            row["directory"],
            receipts,
        )
    declarations = re.findall(
        r'asm\(((?:\s*"[^"]*")+\s*)\)',
        (folder / "ConsumerHooks.h").read_text(),
    )
    names = ["".join(re.findall(r'"([^"]*)"', value)) for value in declarations]
    symbols = sorted(
        {
            value.removeprefix("__wrap_")
            for value in names
            if value.startswith("__wrap_")
        }
    )
    if len(symbols) != 5:
        parser.error("the replay requires all five demand/read/render hooks")
    command = list(link["command"])
    archive = next(
        i for i, value in enumerate(command) if value.endswith("/liboutshine.a")
    )
    command.insert(archive, str(obj))
    compiler = next(i for i, value in enumerate(command) if "clang++" in value)
    command[compiler + 1 : compiler + 1] = ["-Wl,--wrap=" + value for value in symbols]
    command[command.index("-o") + 1] = str(output / "ConsumerReplay")
    command.extend(
        shlex.split(
            subprocess.check_output(
                ["pkg-config", "--libs", "libxxhash", "libzstd"], text=True
            )
        )
    )
    run_stage(command, "link", output, row["directory"], receipts)
    main = next(
        entry
        for entry in rows
        if Path(entry["file"]).parts[-2:] == ("client", "Main.cpp")
    )
    arguments = main.get("arguments") or shlex.split(main["command"])
    main_object = arguments[arguments.index("-o") + 1]
    if main_object not in command:
        parser.error("client link does not match the entry object in the native CDB")
    command[command.index(main_object)] = str(output / "GuardCheck.o")
    command[command.index("-o") + 1] = str(output / "GuardCheck")
    run_stage(command, "link-guards", output, row["directory"], receipts)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Exercise bounded GPU chunk expansion using current native-target compile/link flags."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def run(command, cwd, log, env=None):
    with log.open("w") as stream:
        result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT, env=env)
    record = {"command": command, "status": result.returncode, "log": str(log)}
    log.with_suffix(".terminal.json").write_text(json.dumps(record, indent=2) + "\n")
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    experiment = Path(__file__).resolve().parent
    root = experiment.parents[3]
    if output.is_relative_to(root):
        raise ValueError("experiment output must be outside the checkout")
    (output / "result.json").unlink(missing_ok=True)
    build = args.build.resolve()
    target = "test_outshine_src_render_device_native_Device_TransfersCompleteWithoutSceneOrWindow"
    steps = {}
    steps["build"] = run(
        ["nice", "-n", "10", "cmake", "--build", str(build), "--target", target, "--parallel", "3"],
        root, output / "build.log")
    if steps["build"]["status"]:
        raise RuntimeError(steps["build"])
    for name in ("reset", "expand"):
        steps[name] = run(
            ["nice", "-n", "10", "glslang", "-V", "--target-env", "vulkan1.0", str(experiment / f"{name}.comp"),
             "-o", str(output / f"{name}.spv")], root, output / f"{name}.log")
        if steps[name]["status"]:
            raise RuntimeError(steps[name])
    cdb = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in cdb if row["file"].endswith("/Device/TransfersCompleteWithoutSceneOrWindow.cpp"))
    command = shlex.split(entry["command"])
    command[command.index("-c") + 1] = str(experiment / "probe.cpp")
    for flag, value in (("-o", output / "probe.o"), ("-MF", output / "probe.d"), ("-MT", output / "probe.o")):
        if flag in command:
            command[command.index(flag) + 1] = str(value)
    steps["compile"] = run(["nice", "-n", "10", *command], entry["directory"], output / "compile.log")
    if steps["compile"]["status"]:
        raise RuntimeError(steps["compile"])
    commands = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", target], text=True)
    link = shlex.split(commands.splitlines()[-1])
    if link[:2] != [":", "&&"] or link[-2:] != ["&&", ":"]:
        raise RuntimeError("unexpected Ninja link framing")
    link = link[2:-2]
    link[link.index("-o") + 1] = str(output / "probe")
    unit = next(item for item in link if item.endswith("/TransfersCompleteWithoutSceneOrWindow.cpp.o"))
    link[link.index(unit)] = str(output / "probe.o")
    steps["link"] = run(["nice", "-n", "10", *link], build, output / "link.log")
    if steps["link"]["status"]:
        raise RuntimeError(steps["link"])
    invocation = [str(output / "probe"), str(output / "reset.spv"), str(output / "expand.spv")]
    steps["native"] = run(invocation, root, output / "native.log", os.environ.copy())
    steps["negative_without_reset"] = run([*invocation, "--omit-reset"], root, output / "negative.log", os.environ.copy())
    report = {"commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
              "steps": steps, "cdb": entry,
              "sources": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in experiment.iterdir() if p.is_file()}}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({name: result["status"] for name, result in steps.items()}))
    return int(steps["native"]["status"] != 0 or steps["negative_without_reset"]["status"] == 0)


if __name__ == "__main__":
    raise SystemExit(main())

"""Profile an existing native original-input exporter without building it."""

import argparse
import hashlib
import json
import os
import signal
import subprocess
import tempfile
from collections import Counter
from pathlib import Path


def without_timing(value):
    if value is None:
        return None
    result = dict(value)
    result["results"] = [
        {k: v for k, v in row.items() if k != "elapsed_s"} for row in value["results"]
    ]
    return result


def phase_counts(profile, actual):
    entries = profile.get("bake_entries", [])
    if actual is None or len(entries) != len(actual["results"]):
        return None
    if any("finish_cpu_ticks" not in x or x.get("unexpected_unwind") for x in entries):
        return None
    groups = {"whole": Counter(), "cells": Counter(), "outside_bake": Counter()}
    for sample in profile.get("samples", []):
        index = sample["phase"]
        key = (
            "outside_bake"
            if index == -1
            else "whole"
            if actual["results"][index]["cell"] is None
            else "cells"
        )
        groups[key].update({"samples": 1})
        groups[key].update(set(sample["stack"]))
    return {key: dict(counter) for key, counter in groups.items()}


def run(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        output.exists()
        or output == temporary
        or not output.is_relative_to(temporary)
        or output.is_relative_to(checkout)
    ):
        raise ValueError("fresh explicit system-temp output outside checkout required")
    paths = [args.binary.resolve(), args.input.resolve(), args.reference.resolve()]
    if not all(p.is_file() for p in paths):
        raise ValueError("existing native binary, original input and output required")
    helper = Path(__file__).with_name("gdb_capture_samples.py").resolve()
    output.mkdir(parents=True)
    result = output / "native.json"
    command = [
        "gdb",
        "-nx",
        "-q",
        "-batch",
        "--return-child-result",
        "-ex",
        "set pagination off",
        "-ex",
        "set confirm off",
        "-ex",
        "set debuginfod enabled off",
        "-ex",
        "set disable-randomization off",
        "-ex",
        "start",
        "-ex",
        f"source {helper}",
        "--args",
        str(paths[0]),
        str(paths[1]),
        str(result),
    ]
    env = dict(os.environ, OUTSHINE_CAPTURE_PROFILE_JSON=str(output / "stacks.json"))
    with (
        (output / "gdb.log").open("w") as log,
        subprocess.Popen(
            command,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        ) as process,
    ):
        try:
            exit_code = process.wait(timeout=240)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            exit_code = process.wait()
    profile_path = output / "stacks.json"
    profile = json.loads(profile_path.read_text()) if profile_path.exists() else {}
    reference = json.loads(args.reference.read_bytes())
    actual = json.loads(result.read_bytes()) if result.exists() else None
    equal = without_timing(actual) == without_timing(reference)
    tops, inclusive = Counter(), Counter()
    for sample in profile.get("samples", []):
        stack = sample["stack"]
        tops.update(stack[:1])
        inclusive.update(set(stack))
    receipt = {
        "command": command,
        "exit": exit_code,
        "all_fields_equal_except_result_elapsed_s": equal,
        "profile_native_exit": profile.get("native_exit"),
        "worker_error": profile.get("worker_error"),
        "library_search_path": env.get("LD_LIBRARY_PATH", ""),
        "mapped_library_sha256_at_main": {
            name: hashlib.sha256(Path(name).read_bytes()).hexdigest()
            for name in profile.get("libraries_at_main", [])
            if Path(name).is_file()
        },
        "samples": len(profile.get("samples", [])),
        "bake_phase_counts": phase_counts(profile, actual),
        "top_frames": tops.most_common(),
        "inclusive_frames": inclusive.most_common(),
        "sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*paths, helper, Path(__file__)]
        },
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if (
        exit_code != 0
        or profile.get("native_exit") != 0
        or profile.get("worker_error") is not None
        or not receipt["samples"]
        or receipt["bake_phase_counts"] is None
        or not equal
    ):
        raise RuntimeError(f"incomplete or output-changing profile; see {output}")
    print(json.dumps({"samples": receipt["samples"], "output_equal": equal}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("binary", "input", "reference", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    run(parser.parse_args())

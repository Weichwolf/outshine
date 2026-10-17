"""Consume actual native producer updates in the existing full-sum GPU control."""

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

import numpy as np
from check_local_light_native import compare
from local_light_inputs import ROOT


def prepare(base, producer, output, step):
    output.mkdir(parents=True)
    for path in base.glob("*.bin"):
        shutil.copyfile(path, output / path.name)
    suffix = "" if step < 0 else f".update-{step}"
    lights = None
    for name, method in [("tiles", "hash"), ("log", "sorted")]:
        prefix = producer / f"dense-32-{method}{suffix}"
        data = Path(str(prefix) + ".lights.bin").read_bytes()
        if lights is not None and lights != data:
            raise AssertionError("both lookup variants must retain identical Light64")
        lights = data
        words = np.fromfile(Path(str(prefix) + ".words.bin"), "<u4")
        words.tofile(output / f"{name}-lists.bin")
        frame = np.fromfile(output / f"{name}-frame.bin", "<u4")
        pose_at = int(words[4]) + 3 * (len(data) // 64)
        frame[3] = (pose_at << 1) | int(method == "sorted")
        frame.tofile(output / f"{name}-frame.bin")
        if name == "tiles":
            frame.tofile(output / "brute-frame.bin")
    (output / "lights.bin").write_bytes(lights)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "consumer",
        "vertex",
        "brute",
        "listed",
        "compute",
        "base-input",
        "producer",
        "output",
    ):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for step in range(-1, 7):
        folder = args.output / f"step-{step}"
        inputs = folder / "inputs"
        captures = folder / "actual"
        prepare(args.base_input, args.producer, inputs, step)
        captures.mkdir()
        argv = [
            str(args.consumer),
            str(args.vertex),
            str(args.brute),
            str(args.listed),
            str(inputs),
            str(captures),
            "1",
            "1",
            str(args.compute),
            "0",
            "0",
        ]
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=180, check=False
        )
        (folder / "native.log").write_text(result.stdout + result.stderr)
        checks = compare(captures)
        (folder / "comparison.json").write_text(json.dumps(checks, indent=2) + "\n")
        if result.returncode or any(
            row[key]
            for row in checks
            for key in (
                "nonfinite",
                "negative_components",
                "rgb_failures",
                "support_mismatches",
                "support_nonintegers",
            )
        ):
            raise AssertionError((step, result.returncode, checks))
        row = {
            "step": step,
            "argv": argv,
            "exit": result.returncode,
            "rgb_support_checks_passed": True,
            "hashes": {
                x.name: hashlib.sha256(x.read_bytes()).hexdigest()
                for x in captures.glob("*.rgba32")
            },
            "input_hashes": {
                x.name: hashlib.sha256(x.read_bytes()).hexdigest()
                for x in inputs.glob("*.bin")
            },
        }
        rows.append(row)
        if step >= 0:
            row["brute_planes_changed_from_base"] = [
                plane
                for plane in range(4)
                if row["hashes"][f"brute-{plane}.rgba32"]
                != rows[0]["hashes"][f"brute-{plane}.rgba32"]
            ]
        (args.output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(
            step,
            "actual GPU support/RGB match",
            row.get("brute_planes_changed_from_base"),
            flush=True,
        )


if __name__ == "__main__":
    main()

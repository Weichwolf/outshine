"""Exercise segmented GPU scans across block and recursive-level boundaries."""

import argparse
import json
from pathlib import Path
import subprocess

import numpy as np


def write_fixture(directory, count):
    root_counts = []
    remaining, step = count, 0
    while remaining:
        length = min(remaining, [0, 1, 33, 225, 1025, 4095][step % 6])
        root_counts.append(length)
        remaining -= length
        step += 1
    roots, visible, arguments = [], [], []
    logical, batch, total = 0, 0, 0
    kept = np.where(np.arange(count) % 7, 3 * (1 + np.arange(count) % 2), 0).astype("<u4")
    for row, length in enumerate(root_counts):
        if row and row % 4 == 0:
            arguments.append([total, 1, 0, 0, 0])
            batch += 1
            total = 0
        live = int(row % 3 != 1)
        visible.append(live)
        roots.append([0, 0, 0, 0, 0, length, logical, batch, row, 0, length * 6, 0])
        if live:
            total += int(kept[logical:logical + length].sum())
        logical += length
    arguments.append([total, 1, 0, 0, 0])
    for name, values in [("roots", roots), ("visible", visible), ("kept", kept),
                         ("arguments", arguments)]:
        np.asarray(values, dtype="<u4").tofile(directory / (name + ".bin"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--shaders", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False)
    results = []
    for count in [1, 255, 256, 257, 65535, 65536, 65537, 70001]:
        directory = (args.output / str(count)).resolve()
        directory.mkdir()
        write_fixture(directory, count)
        command = [str(args.probe.resolve()), str(directory), str(directory),
                   str(args.shaders.resolve())]
        with (directory / "run.log").open("w") as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        results.append({"clusters": count, "command": command, "exit": result.returncode})
        (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        print(count, "exit", result.returncode, flush=True)
        if result.returncode:
            raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()

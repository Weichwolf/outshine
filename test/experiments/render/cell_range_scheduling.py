"""Compare planning slices over the source-cell order from a native index."""

import argparse
import hashlib
import json
from pathlib import Path


def planning(cells, target, quota, selected_only):
    at = calls = scans = 0
    selected = []
    while at < len(cells):
        used = 0
        while at < len(cells) and used < quota:
            match = cells[at] == target
            scans += 1
            if match:
                selected.append(at)
            used += match if selected_only else 1
            at += 1
        calls += 1
    return calls, scans, selected


def compare(cells, quota, ranges_per_task):
    report = {"sources": len(cells), "cells": len(set(cells)), "quota": quota,
              "ranges_per_task": ranges_per_task,
              "scope": "planning only; excludes selection, emission, IO, GPU and frames"}
    for mode in ("old", "new"):
        for key in ("ranges", "scans", "tasks"):
            report[f"{mode}_{key}"] = 0
    for cell in sorted(set(cells)):
        old = planning(cells, cell, quota, False)
        new = planning(cells, cell, quota, True)
        if old[2] != new[2]:
            raise RuntimeError("source selection or order changed")
        for mode, result in (("old", old), ("new", new)):
            report[f"{mode}_ranges"] += result[0]
            report[f"{mode}_scans"] += result[1]
            report[f"{mode}_tasks"] += (result[0] + ranges_per_task - 1) // ranges_per_task
    report["selected_equal"] = True
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--quota", type=int, default=64)
    parser.add_argument("--ranges-per-task", type=int, default=4)
    args = parser.parse_args()
    cells = json.loads(args.input.read_text())
    if (not isinstance(cells, list) or len(cells) > 1_000_000 or
            any(type(cell) is not int or not 1 <= cell <= 64 for cell in cells) or
            args.quota <= 0 or args.ranges_per_task <= 0):
        parser.error("expected bounded source-cell IDs and positive scheduling limits")
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs in the system temporary directory")
    report = compare(cells, args.quota, args.ranges_per_task)
    report["input_sha256"] = hashlib.sha256(args.input.read_bytes()).hexdigest()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

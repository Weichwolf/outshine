"""Compare unchanged native producer bytes and interleaved full/incremental pack costs."""

import argparse
import hashlib
import json
import statistics
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT
from local_light_world_build import edits


def digest_outputs(prefix):
    return {
        path.name.removeprefix(prefix.name): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(prefix.parent.glob(prefix.name + "*.bin"))
    }


def execute(binary, root, label, source, updates):
    prefix = root / label
    argv = [
        str(binary),
        str(source),
        str(prefix),
        "32",
        "hash",
        str(16 * 1024**2),
        "0",
        "0",
        "0",
        "1",
        str(updates),
    ]
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=180, check=False
    )
    (root / (label + ".stdout")).write_text(result.stdout)
    (root / (label + ".stderr")).write_text(result.stderr)
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    if result.returncode or result.stderr or len(rows) != 8:
        raise AssertionError((argv, result.returncode, result.stderr))
    if any(row["work_counters"] for row in rows):
        raise AssertionError("build with LOCAL_LIGHT_BUILD_COUNTER=0 for timing")
    return {
        "argv": argv,
        "exit": result.returncode,
        "rows": rows,
        "sha256": digest_outputs(prefix),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, choices=range(2, 17), default=6)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    _, _, packed, _, _ = inputs("dense", True, "street")
    source, updates = args.output / "source.bin", args.output / "updates.bin"
    packed.tofile(source)
    states = edits(packed)
    np.array(
        [
            np.r_[np.uint32(id_), light.view("<u4").ravel()]
            for _, id_, light, _ in states
        ],
        dtype="<u4",
    ).tofile(updates)
    runs = []
    for repeat in range(args.repeats):
        order = ("before", "after") if repeat % 2 == 0 else ("after", "before")
        pair = {}
        for side in order:
            label = f"{side}-{repeat}"
            pair[side] = execute(
                getattr(args, side), args.output, label, source, updates
            )
            if repeat:
                for path in args.output.glob(label + "*.bin"):
                    path.unlink()
        if pair["before"]["sha256"] != pair["after"]["sha256"]:
            raise AssertionError("full and incremental output bytes differ")
        runs.append(pair)
        (args.output / "runs.json").write_text(json.dumps(runs, indent=2) + "\n")
    summary = []
    for index, name in enumerate(["cold", *[item[0] for item in states]]):
        item = {"phase": name}
        for side in ("before", "after"):
            rows = [pair[side]["rows"][index] for pair in runs]
            item[side] = {
                field: statistics.median(row[field] for row in rows)
                for field in (
                    "build_ms",
                    "pack_ms",
                    "own_allocations",
                    "own_peak_bytes",
                )
            }
            item[side]["packed_word_writes"] = rows[0].get("packed_word_writes")
            item[side]["pack_rebuilds"] = rows[0].get("pack_rebuilds")
        summary.append(item)
    result = {
        "repeats": args.repeats,
        "summary": summary,
        "input_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "identical_outputs": True,
        "scope": "CPU producer only; no GPU upload, runtime or frame proof",
    }
    (args.output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

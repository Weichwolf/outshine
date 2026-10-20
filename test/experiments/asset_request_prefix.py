"""Compare exact demand hashes for a captured request with an immutable recipe prefix."""

import argparse
import hashlib
import json
from pathlib import Path
import statistics
import struct
import tempfile
import time


def request_parts(path):
    raw = path.read_bytes()
    if not 8 <= len(raw) <= 4 * 1024 * 1024:
        raise ValueError("request must fit its existing four-MiB encoding limit")
    tag_size = struct.unpack_from("<I", raw)[0]
    if raw[4:4 + tag_size] != b"native-ground-region-demand-2":
        raise ValueError("input is not a serialized regional demand")
    recipe_size = struct.unpack_from("<I", raw, 4 + tag_size)[0]
    split = 4 + tag_size + 4 + recipe_size
    if split > len(raw) - 8:
        raise ValueError("request has no complete prefix and variable suffix")
    return raw, raw[:split], raw[split:]


def measure(prefix, inputs, method, expected):
    started = time.perf_counter_ns()
    seed = hashlib.sha256(prefix) if method == "prefix" else None
    previous = previous_digest = None
    hashed = len(prefix) if seed is not None else 0
    complete_hashes = 0
    actual = []
    for suffix in inputs:
        if method == "last-input" and previous == suffix:
            result = previous_digest
        elif seed is not None:
            state = seed.copy()
            state.update(suffix)
            result = state.digest()
            hashed += len(suffix)
        else:
            result = hashlib.sha256(prefix + suffix).digest()
            hashed += len(prefix) + len(suffix)
            complete_hashes += 1
            if method == "last-input":
                previous, previous_digest = suffix, result
        actual.append(result)
    milliseconds = (time.perf_counter_ns() - started) / 1e6
    if actual != expected:
        raise AssertionError("candidate changed a request digest")
    return milliseconds, hashed, complete_hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=1754)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("iterations must be positive")
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be an explicit system-temp path")
    raw, prefix, suffix = request_parts(args.input)
    rows = []
    for workload in ("unchanged", "changing"):
        inputs = [
            bytes(bytearray(suffix)) if workload == "unchanged"
            else suffix[:-8] + count.to_bytes(8, "little")
            for count in range(args.iterations)
        ]
        expected = [hashlib.sha256(prefix + value).digest() for value in inputs]
        for method in ("full", "prefix", "last-input"):
            trials = [measure(prefix, inputs, method, expected) for _ in range(3)]
            rows.append({
                "workload": workload,
                "method": method,
                "median_ms": statistics.median(row[0] for row in trials),
                "rounds_ms": [row[0] for row in trials],
                "hashed_bytes": trials[0][1],
                "full_hashes": trials[0][2],
                "digests_equal": True,
            })
    result = {
        "input_sha256": hashlib.sha256(raw).hexdigest(),
        "input_bytes": len(raw),
        "prefix_bytes": len(prefix),
        "suffix_bytes": len(suffix),
        "requests": args.iterations,
        "results": rows,
        "scope": (
            "Python/hashlib reference only, no native time or allocation claim. "
            "Input lists precede timing; prefix preparation is timed. "
            "Last-input retains only its previous suffix and digest."
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2))
    for row in rows:
        print(row["workload"], row["method"], round(row["median_ms"], 3), "ms")


if __name__ == "__main__":
    main()

import argparse
import bisect
import json
import random
import statistics
import time
from pathlib import Path


def scan(names, heard):
    result = [None] * len(names)
    comparisons = 0
    for index, name in enumerate(names):
        for row, (identity, active) in enumerate(heard):
            comparisons += 1
            if identity == name and active:
                result[index] = row
    return result, comparisons


def hashed(index, heard):
    result = [None] * len(index)
    lookups = 0
    for row, (identity, active) in enumerate(heard):
        if active:
            lookups += 1
            at = index.get(identity)
            if at is not None:
                result[at] = row
    return result, lookups


def ordered(keys, indices, heard):
    result = [None] * len(keys)
    lookups = 0
    for row, (identity, active) in enumerate(heard):
        if active:
            lookups += 1
            at = bisect.bisect_left(keys, identity)
            if at < len(keys) and keys[at] == identity:
                result[indices[at]] = row
    return result, lookups


def measure(function, arguments, expected):
    times = []
    work = 0
    for _ in range(7):
        start = time.perf_counter_ns()
        result, work = function(*arguments)
        times.append(time.perf_counter_ns() - start)
        assert result == expected
    return {"median_ns": statistics.median(times), "queries": work}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    cases = []
    for count in (16, 128, 1024):
        names = [f"sound-{index}" for index in range(count)]
        index = {name: at for at, name in enumerate(names)}
        sorted_names = sorted(index)
        indices = [index[name] for name in sorted_names]
        normal = [(name, True) for name in names]
        shuffled = normal.copy()
        random.Random(42).shuffle(shuffled)
        for kind, heard in (
            ("ordered", normal),
            ("shuffled", shuffled),
            ("sparse", normal[::8]),
            (
                "duplicate-and-unknown",
                shuffled + normal + [(names[0], False), ("unknown", True)],
            ),
        ):
            expected, _ = scan(names, heard)
            cases.append(
                {
                    "declared": count,
                    "heard": len(heard),
                    "case": kind,
                    "scan": measure(scan, (names, heard), expected),
                    "sorted": measure(
                        ordered, (sorted_names, indices, heard), expected
                    ),
                    "hash": measure(hashed, (index, heard), expected),
                }
            )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"cases": cases}, indent=2) + "\n")
    print(
        f"{len(cases)} exact binding comparisons passed; Python timings are not device admission"
    )


if __name__ == "__main__":
    main()

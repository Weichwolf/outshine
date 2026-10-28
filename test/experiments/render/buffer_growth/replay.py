import argparse
import csv
import gzip
import json
from dataclasses import dataclass
from pathlib import Path

MAX_BYTES = 2**32 - 1
COPY_USE = 3


@dataclass
class Buffer:
    capacity: int = 0
    usage: int = 0
    present: bool = False


class Arena:
    def __init__(self):
        self.buffers = [Buffer() for _ in range(17)]
        self.copied = 0
        self.allocations = 0
        self.allocated_bytes = 0
        self.growths = []

    def request(self, row, policy, verify):
        (
            kind,
            _owner,
            slot,
            required,
            usage,
            preserve,
            _deferred,
            present,
            held,
            held_usage,
            copied,
        ) = row
        buffer = self.buffers[slot]
        if verify:
            actual = (buffer.capacity, buffer.usage, self.copied)
            expected = (held, held_usage, copied)
            if actual != expected:
                raise ValueError(f"state mismatch: {row}; predicted={actual}")
            if kind == "G" and buffer.present != bool(present):
                raise ValueError(f"presence mismatch: {row}")
        if kind == "X" and not present:
            self.buffers[slot] = Buffer()
            return
        fits = (
            buffer.present
            and buffer.capacity >= required
            and buffer.usage & usage == usage
        )
        if fits:
            return
        capacity = required
        if kind == "G":
            capacity = buffer.capacity or required
            if policy == "doubling_max":
                if capacity < required:
                    capacity = max(required, min(MAX_BYTES, capacity * 2))
            elif policy == "exact":
                capacity = max(capacity, required)
            else:
                numerator, denominator = {
                    "doubling": (2, 1),
                    "three_halves": (3, 2),
                    "five_quarters": (5, 4),
                }[policy]
                while capacity < required:
                    step = max(
                        capacity + 1,
                        (capacity * numerator + denominator - 1) // denominator,
                    )
                    if policy != "doubling":
                        step = (step + 3) // 4 * 4
                    capacity = min(MAX_BYTES, step)
            if buffer.present and preserve:
                self.copied += buffer.capacity
            self.growths.append((slot, buffer.capacity, required, capacity))
        self.allocations += 1
        self.allocated_bytes += capacity
        self.buffers[slot] = Buffer(capacity, usage | COPY_USE, True)


def replay(path, policy, verify):
    arenas = {}
    generations = {}
    current = {}
    counts = {"G": 0, "X": 0, "R": 0}
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", newline="") as file:
        for line, raw in enumerate(csv.reader(file), 1):
            if len(raw) != 11:
                raise ValueError(f"invalid trace row {line}: {raw}")
            kind, owner = raw[:2]
            row = (kind, owner, *(int(x) for x in raw[2:]))
            if (
                kind not in counts
                or not 0 <= row[2] < 17
                or not 0 <= row[3] <= MAX_BYTES
            ):
                raise ValueError(f"invalid request fields at row {line}")
            if any(value < 0 for value in row[2:]) or row[8] > MAX_BYTES:
                raise ValueError(f"invalid unsigned fields at row {line}")
            counts[kind] += 1
            if kind == "R":
                generations[owner] = generations.get(owner, 0) + 1
                current[owner] = (owner, generations[owner])
                arenas[current[owner]] = Arena()
                arenas[current[owner]].copied = row[-1]
            else:
                if owner not in current:
                    raise ValueError(f"missing owner reset at row {line}")
                try:
                    arenas[current[owner]].request(row, policy, verify)
                except ValueError as error:
                    raise ValueError(f"row {line}: {error}") from error
    output = []
    for (owner, generation), arena in arenas.items():
        output.append(
            {
                "owner": owner,
                "generation": generation,
                "capacity": sum(b.capacity for b in arena.buffers),
                "streams": [b.capacity for b in arena.buffers],
                "copy_bytes": arena.copied,
                "allocations": arena.allocations,
                "cumulative_requested_allocation_bytes": arena.allocated_bytes,
                "growths": arena.growths,
            }
        )
    return {"policy": policy, "counts": counts, "arenas": output}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    results = [
        replay(args.trace, policy, policy == "doubling")
        for policy in (
            "doubling",
            "doubling_max",
            "three_halves",
            "five_quarters",
            "exact",
        )
    ]
    args.output.write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        arena = max(result["arenas"], key=lambda a: a["capacity"])
        print(
            result["policy"],
            result["counts"],
            {k: v for k, v in arena.items() if k not in ("growths", "streams")},
        )


if __name__ == "__main__":
    main()

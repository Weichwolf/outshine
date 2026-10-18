"""Compare allocation strategies using native PART/ARRAY metadata, without geometry changes."""

import argparse
import json
from pathlib import Path


COMPONENTS = {
    "positions": 3, "normals": 3, "tangents": 4, "uv": 2, "uv1": 2, "colours": 4
}


class Storage:
    def __init__(self):
        self.size = {name: 0 for name in (*COMPONENTS, "indices")}
        self.capacity = self.size.copy()
        self.allocations = 0
        self.copied_bytes = 0
        self.peak_bytes = 0

    def allocate(self, name, capacity):
        if capacity == self.capacity[name]:
            return
        self.peak_bytes = max(self.peak_bytes, 4 * (sum(self.capacity.values()) + capacity))
        self.allocations += 1
        self.copied_bytes += 4 * self.size[name]
        self.capacity[name] = capacity

    def resize(self, name, size):
        if size > self.capacity[name]:
            self.allocate(name, max(size, 2 * self.capacity[name]))
        self.size[name] = size

    def append_indices(self, count):
        final = self.size["indices"] + count
        while self.capacity["indices"] < final:
            self.size["indices"] = self.capacity["indices"]
            self.allocate("indices", max(1, 2 * self.capacity["indices"]))
        self.size["indices"] = final

    def report(self):
        return {
            "capacity_bytes": {key: 4 * value for key, value in self.capacity.items()},
            "used_bytes": {key: 4 * value for key, value in self.size.items()},
            "allocations": self.allocations,
            "growth_copy_bytes": self.copied_bytes,
            "peak_requested_bytes": self.peak_bytes,
        }


def append(storage, parts):
    vertices = storage.size["positions"] // 3
    for part in parts:
        flat = part["normals"] == 0 and part["indices"] != 0
        vertices += part["indices"] if flat else part["positions"] // 3
        for name, components in COMPONENTS.items():
            present = part[name] != 0 or (name == "normals" and flat)
            if present or storage.size[name] != 0:
                storage.resize(name, vertices * components)
        storage.append_indices(part["indices"])


def compare(parts):
    current = Storage()
    append(current, parts)
    planned = Storage()
    for name, size in current.size.items():
        if size:
            planned.allocate(name, size)
    append(planned, parts)
    assert current.size == planned.size
    current_report = current.report()
    for name, size in current.size.items():
        if size:
            current.allocate(name, size)
    return {"growth": current_report, "reserve_before": planned.report(), "shrink_after": current.report()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args()
    parts = []
    measured = {}
    for line in options.trace.read_text().splitlines():
        fields = line.split("\t")
        if fields[0] == "PART":
            names = ("positions", "indices", "normals", "tangents", "uv", "uv1", "colours")
            parts.append(dict(zip(names, map(int, fields[3:]), strict=True)))
        if (
            fields[0] == "ARRAY"
            and fields[1] == "geometry-cooked"
            and fields[3] in (*COMPONENTS, "indices")
        ):
            measured[fields[3]] = {"used": int(fields[4]), "capacity": int(fields[5])}
    if not parts or not measured:
        parser.error("trace must contain native PART and cooked ARRAY rows")
    result = compare(parts)
    assert result["growth"]["capacity_bytes"] == {
        name: row["capacity"] for name, row in measured.items()
    }
    assert result["growth"]["used_bytes"] == {
        name: row["used"] for name, row in measured.items()
    }
    report = {
        "trace": str(options.trace),
        "parts": parts,
        "scope": (
            "Logical vector allocation/copy model matching observed libc++ capacities; "
            "excludes clusters, allocator overhead, GPU memory and physical DRAM traffic."
        ),
        "alternatives": result,
    }
    options.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()

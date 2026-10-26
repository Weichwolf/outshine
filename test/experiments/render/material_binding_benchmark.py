"""Compare stable material binding work before native implementation."""

import argparse
import json
import time


def scanned(parts, surfaces):
    if not surfaces:
        return ([0], [0] * len(parts)), 0
    materials = []
    slots = []
    searches = 0
    for material in parts:
        try:
            slot = materials.index(material)
            searches += slot + 1
        except ValueError:
            searches += len(materials)
            slot = len(materials)
            materials.append(material)
        slots.append(slot)
    for material in range(surfaces):
        try:
            searches += materials.index(material) + 1
        except ValueError:
            searches += len(materials)
            materials.append(material)
    return (materials, slots), searches


def indexed(parts, surfaces):
    if not surfaces:
        return ([0], [0] * len(parts)), 0
    native = [-1] * surfaces
    fallback = {}
    materials = []
    slots = []
    for material in parts:
        if 0 <= material < surfaces:
            slot = native[material]
            if slot == -1:
                slot = len(materials)
                native[material] = slot
                materials.append(material)
        else:
            slot = fallback.get(material)
            if slot is None:
                slot = len(materials)
                fallback[material] = slot
                materials.append(material)
        slots.append(slot)
    for material, slot in enumerate(native):
        if slot == -1:
            materials.append(material)
    return (materials, slots), len(parts) + surfaces


def fixtures(size):
    return {
        "dense": ([(i * 17) % size for i in range(size)], size),
        "repeated": ([i % min(8, size) for i in range(4 * size)], size),
        "unused": ([], size),
        "foreign": ([-1, 2**31 - 1, -7, 0, size - 1] * size, size),
        "no-surfaces": (list(range(size)), 0),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sizes", type=int, nargs="+", default=[256, 1024, 4096])
    options = parser.parse_args()
    for size in options.sizes:
        if size <= 0 or size & (size - 1):
            parser.error("sizes must be positive powers of two")
        for name, (parts, surfaces) in fixtures(size).items():
            values = {}
            times = {"scanned": [], "indexed": []}
            for methods in ((scanned, indexed), (indexed, scanned)):
                for method in methods:
                    began = time.process_time_ns()
                    result, work = method(parts, surfaces)
                    times[method.__name__].append(time.process_time_ns() - began)
                    values[method.__name__] = (result, work)
            assert values["scanned"][0] == values["indexed"][0], (name, size)
            print(json.dumps({"case": name, "size": size, "parts": len(parts),
                              "surfaces": surfaces, "same_output": True,
                              "search_comparisons": values["scanned"][1],
                              "index_probes": values["indexed"][1],
                              "cpu_ns": times}), flush=True)


if __name__ == "__main__":
    main()

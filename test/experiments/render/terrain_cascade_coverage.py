"""Compare region-union coverage with bounded dyadic tile coverage."""

import argparse
import json
import math
import random
import time
import xml.etree.ElementTree as ET
from pathlib import Path


SIDE = 4


def origin(value):
    return 2 * math.floor((math.floor(value) - 1) / 2)


def contains(parent, child):
    x, y, span = parent
    a, b, width = child
    return x <= a and y <= b and a + width <= x + span and b + width <= y + span


def traverse(point, zoom, levels, seed, mode, playable, linear):
    regions = []
    cells = [0] * 16
    has_coverage = False
    previous_x = previous_y = 0
    calls = []
    sheets = []
    counts = dict(tiles=0, skipped=0, overlapped=0, pending=0, absent=0, refused=0,
                  contact_pending=0, bare=0)
    wanted = [0] * 24
    pending = [0] * 24
    work = 0
    for level in range(min(levels, zoom)):
        span = 1 << level
        x_at, y_at = point[0] / span, point[1] / span
        first_x, first_y = origin(x_at), origin(y_at)
        if linear and level and has_coverage:
            parent_cells = [0] * 16
            for row in range(SIDE):
                for column in range(SIDE):
                    target_x = (previous_x + column) // 2 - first_x
                    target_y = (previous_y + row) // 2 - first_y
                    assert 0 <= target_x < SIDE and 0 <= target_y < SIDE
                    parent_cells[target_y * SIDE + target_x] += cells[row * SIDE + column]
                    work += 1
            cells = parent_cells
        completed = []
        tile_zoom = zoom - level
        world = 1 << tile_zoom
        for row in range(SIDE):
            for column in range(SIDE):
                x, y = first_x + column, first_y + row
                contact = level == 0 and x == math.floor(x_at) and y == math.floor(y_at)
                if mode != "mesh" and playable and level != min(levels, zoom) - 1 and not contact:
                    continue
                region = (x * span, y * span, span)
                if linear:
                    covered = cells[row * SIDE + column]
                else:
                    covered = 0
                    for held in regions:
                        work += 1
                        if contains(region, held):
                            covered += held[2] ** 2
                if covered == span * span:
                    counts["skipped"] += 1
                    continue
                counts["overlapped"] += covered != 0
                if not 0 <= y < world:
                    continue
                tile = (tile_zoom, x % world, y)
                code = 0 if seed == -1 else 1 if seed == -2 else (
                    tile[1] * 2654435761 + y * 2246822519 + tile_zoom * 3266489917
                    + seed + len(calls) * 374761393) & 0xFFFFFFFF
                if seed >= 0:
                    code %= 7
                calls.append((tile, code))
                counts["tiles"] += 1
                wanted[tile_zoom] += 1
                waiting = code in (1, 5)
                counts["pending"] += waiting
                pending[tile_zoom] += waiting
                counts["absent"] += code in (2, 4)
                counts["refused"] += code == 3
                ready = code == 0 or code == 6 and mode != "mesh"
                counts["contact_pending"] += contact and not ready
                counts["bare"] += mode == "mesh" and not ready
                if ready:
                    if mode == "mesh":
                        sheets.append(tile)
                    if linear:
                        cells[row * SIDE + column] = span * span
                        has_coverage = True
                    else:
                        completed.append(region)
        if not linear:
            for region in completed:
                retained = []
                for held in regions:
                    work += 1
                    if not contains(region, held):
                        retained.append(held)
                regions = retained + [region]
        previous_x, previous_y = first_x, first_y
    return (calls, counts, wanted, pending, sheets), work


def tile_point(latitude, longitude, zoom):
    n = 1 << zoom
    latitude = min(85.0511287798066, max(-85.0511287798066, latitude))
    x = (longitude + 180) / 360 * n
    y = (1 - math.asinh(math.tan(math.radians(latitude))) / math.pi) / 2 * n
    return min(math.nextafter(n, 0), max(0, x)), min(math.nextafter(n, 0), max(0, y))


def cases(catalog):
    for x in range(16):
        for y in range(16):
            yield (16 + x + 0.5, 16 + y + 0.5), 6, 4
    for zoom in (1, 2, 6, 14, 23):
        n = 1 << zoom
        edges = (0, math.nextafter(1, 0), n / 2, math.nextafter(n, 0))
        for x in edges:
            for y in edges:
                yield (x, y), zoom, zoom
    rng = random.Random(1670)
    for _ in range(128):
        zoom = rng.randrange(1, 24)
        yield (rng.random() * (1 << zoom), rng.random() * (1 << zoom)), zoom, rng.randrange(1, zoom + 1)
    for path in sorted(catalog.glob("*.scenario")):
        world = ET.parse(path).getroot().find("world")
        latitude, longitude = float(world.attrib["lat"]), float(world.attrib["lon"])
        for zoom in (6, 14, 23):
            yield tile_point(latitude, longitude, zoom), zoom, min(8, zoom)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=Path("test/outshine/data/places"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = list(cases(args.catalog))
    checked = 0
    worst = {"regions": 0, "linear": 0}
    for point, zoom, levels in inputs:
        for seed in (-2, -1, 0, 1, 19):
            for mode in ("inspect", "request", "mesh"):
                for playable in (False, True):
                    old, before = traverse(point, zoom, levels, seed, mode, playable, False)
                    new, after = traverse(point, zoom, levels, seed, mode, playable, True)
                    assert old == new, (point, zoom, levels, seed, mode, playable)
                    worst["regions"] = max(worst["regions"], before)
                    worst["linear"] = max(worst["linear"], after)
                    checked += 1
    benchmarks = []
    for seed in (-2, -1, 19):
        output = {"seed": seed}
        for name, linear in (("regions", False), ("linear", True)):
            started = time.perf_counter()
            work = queries = 0
            for _ in range(256):
                result, count = traverse((424315.5, 319137.5), 20, 12, seed, "inspect", False, linear)
                work += count
                queries += len(result[0])
            output[name] = {"seconds": time.perf_counter() - started,
                            "coverage_steps": work, "queries": queries}
        benchmarks.append(output)
    report = {"checked": checked, "input_cases": len(inputs), "worst_coverage_steps": worst,
              "benchmarks": benchmarks, "bounds": {"tiles_per_level": 16,
              "linear_parent_additions_most": "16*(levels-1)",
              "old_region_capacity": "16*(24-1)", "linear_cells": 16}}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

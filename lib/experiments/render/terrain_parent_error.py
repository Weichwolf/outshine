"""Evaluate a stitched parent's 3D error against four native height pages."""

import argparse
from collections import defaultdict
from math import atan, pi, sinh
from pathlib import Path
from time import perf_counter
import json

import numpy as np

from terrain_cached_page import checked_heights, eligible, package, page_prefix, page_records


CELLS = 32
SIDE = CELLS + 1
POSTINGS = np.arange(SIDE)
FRACTION = POSTINGS / CELLS
ECCENTRICITY2 = 1 - (1 - 1 / 298.257223563) ** 2


def geographic(zoom, x, y):
    span = 1 << zoom
    return 2 * pi * (x / span - 0.5), atan(sinh(pi * (1 - 2 * y / span)))


def frame(longitude, latitude):
    sl, cl = np.sin(longitude), np.cos(longitude)
    sp, cp = np.sin(latitude), np.cos(latitude)
    up = np.array((cl * cp, sl * cp, sp))
    radius = 6378137 / np.sqrt(1 - ECCENTRICITY2 * sp * sp)
    return radius * up * (1, 1, 1 - ECCENTRICITY2), np.array(
        ((-sl, cl, 0), (-cl * sp, -sl * sp, cp), up)
    )


def vertices(address, heights):
    zoom, x, y = address
    west, north = geographic(zoom, x, y)
    east, south = geographic(zoom, x + 1, y + 1)
    centre, axes = frame((west + east) * 0.5, (north + south) * 0.5)
    corners = [((frame(lon, lat)[0] - centre) @ axes[:2].T).astype(np.float32)
               for lat in (north, south) for lon in (west, east)]
    u, v = FRACTION[None, :, None], FRACTION[:, None, None]
    top = corners[0] * (1 - u) + corners[1] * u
    bottom = corners[2] * (1 - u) + corners[3] * u
    horizontal = top * (1 - v) + bottom * v
    sag = np.square(horizontal).sum(axis=2) * (0.5 * np.float32(1 / np.linalg.norm(centre)))
    local = np.dstack((horizontal, heights - sag))
    return local @ axes + centre


def parent_heights(children):
    parent = np.empty((SIDE, SIDE), dtype=np.float32)
    for index, heights in enumerate(children):
        x, y = index % 2 * (CELLS // 2), index // 2 * (CELLS // 2)
        parent[y:y + CELLS // 2 + 1, x:x + CELLS // 2 + 1] = heights[::2, ::2]
    return parent


def align_parent(parent, index):
    x = POSTINGS + (index % 2 * CELLS)
    y = POSTINGS + (index // 2 * CELLS)
    x0, y0 = np.minimum(x // 2, CELLS - 1), np.minimum(y // 2, CELLS - 1)
    u, v = ((x * 0.5 - x0)[None, :, None], (y * 0.5 - y0)[:, None, None])
    nw = parent[np.ix_(y0, x0)]
    ne = parent[np.ix_(y0, x0 + 1)]
    sw = parent[np.ix_(y0 + 1, x0)]
    se = parent[np.ix_(y0 + 1, x0 + 1)]
    return np.where(u >= v, nw + u * (ne - nw) + v * (se - ne),
                    nw + v * (sw - nw) + u * (se - sw))


def bound_one(parent_address, children, anchor):
    coarse = vertices(parent_address, parent_heights(children))
    bound = 0.0
    minimum, maximum = coarse.min(axis=(0, 1)), coarse.max(axis=(0, 1))
    z, x, y = parent_address
    for index, heights in enumerate(children):
        fine = vertices((z + 1, 2 * x + index % 2, 2 * y + index // 2), heights)
        bound = max(bound, float(np.linalg.norm(fine - align_parent(coarse, index), axis=2).max()))
        minimum = np.minimum(minimum, fine.min(axis=(0, 1)))
        maximum = np.maximum(maximum, fine.max(axis=(0, 1)))
    distance_scale = np.linalg.norm(np.maximum(abs(minimum - anchor), abs(maximum - anchor)))
    rounding = 64 * np.finfo(np.float32).eps * (1 + distance_scale + np.linalg.norm(maximum - minimum))
    return bound + rounding, minimum - rounding, maximum + rounding


def read_pages(data):
    reader, anchor, count = page_prefix(data)
    pages = {}
    for row, nodes in page_records(reader, count):
        if eligible(row) and (row["virtual"] or row["postings"] == SIDE):
            address = row["zoom"], row["x"], row["y"]
            values = np.frombuffer(checked_heights(nodes), dtype="<f4").reshape((SIDE + 2, SIDE + 2))
            pages[address] = values[1:-1, 1:-1]
    return pages, anchor, count


def candidates(data):
    pages, anchor, count = read_pages(data)
    origin, axes = frame(*np.radians(anchor))
    quartets = defaultdict(dict)
    for (zoom, x, y), heights in pages.items():
        if zoom > 0:
            quartets[zoom - 1, x // 2, y // 2][2 * (y % 2) + x % 2] = heights
    packs = defaultdict(list)
    for (zoom, x, y), children in quartets.items():
        if len(children) != 4 or (zoom, x, y) in pages:
            continue
        packs[zoom + 1, x // 4, y // 4].append(bound_one((zoom, x, y), [children[i] for i in range(4)], origin))
    return packs, count, origin, axes


def summarize(data, report):
    packs, pages, origin, axes = candidates(data)
    rotation = axes[[0, 2, 1]] * np.array([[1], [1], [-1]])
    motion = report["motion"]
    eyes = np.asarray([motion[phase]["position_m"] for phase in ("start", "finish")]) @ rotation + origin
    low, high = eyes.min(axis=0), eyes.max(axis=0)
    viewport = report["profile"]
    focal = viewport["height_px"] * 0.5 / np.tan(np.radians(motion["start"]["perspective_fov_deg"]) * 0.5)
    tx, ty = np.asarray((viewport["width_px"], viewport["height_px"])) / (2 * focal)
    depth_scale = np.hypot(1, np.hypot(tx, ty))
    displacement = focal * np.hypot(np.sqrt(2), tx + ty)

    def fits(bound, pixels):
        error, lower, upper = bound
        separation = np.linalg.norm(np.maximum(0, np.maximum(lower - high, low - upper)))
        depth = max(0, separation - error) / depth_scale
        return bool(depth > error and displacement * error / (depth - error) <= pixels)

    flat = [row for rows in packs.values() for row in rows]
    thresholds = (0.25, 0.5, 1, 2)
    return {"pages": pages, "complete_quartets": len(flat),
            "parent_error_m_p50": float(np.median([row[0] for row in flat])),
            "saved_pages_individual": {px: 3 * sum(fits(row, px) for row in flat)
                                       for px in thresholds},
            "saved_pages_whole_pack": {px: 3 * sum(len(rows) for rows in packs.values()
                                                  if all(fits(row, px) for row in rows))
                                       for px in thresholds}}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cache", type=Path, required=True)
    cli.add_argument("--key", required=True)
    cli.add_argument("--report", type=Path, required=True)
    args = cli.parse_args()
    start = perf_counter()
    data, _ = package(args.cache, args.key)
    result = summarize(data, json.loads(args.report.read_text()))
    print(json.dumps({**result, "seconds": perf_counter() - start}))


if __name__ == "__main__":
    main()

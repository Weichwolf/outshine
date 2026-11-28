"""Count zero-copy prism bands after excluding native building tile rectangles."""

import argparse
import gzip
import json
import math
import sqlite3
import struct
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import EARTH_RADIUS_M
from impostor_depth import content
from tile_selection import local_frame


def visible_bands(width, height, exclusions):
    edges = sorted({0, height, *(edge for cut in exclusions for edge in cut[1::2])})
    for index in range(1, len(edges)):
        top, bottom = edges[index - 1], edges[index]
        spans = sorted(
            (left, right) for left, a, right, b in exclusions if a <= top < b
        )
        till = 0
        for left, right in spans:
            if left > till:
                yield till, top, left, bottom
            till = max(right, till)
        if till < width:
            yield till, top, width, bottom


def patch_views(tiers, rectangles):
    height, width = tiers.shape
    expected = tiers > 0
    for left, top, right, bottom in rectangles:
        expected[top:bottom, left:right] = False
    restored = np.zeros(tiers.shape, bool)
    stride_views = line_views = 0
    addresses = np.arange(tiers.size).reshape(tiers.shape)
    for left, top, right, bottom in visible_bands(width, height, rectangles):
        domain = np.s_[top:bottom, left:right]
        occupied = tiers[domain] > 0
        assert not np.any(restored[domain])
        restored[domain] = occupied
        recorded = top * width + left + np.arange(bottom - top)[:, None] * width
        assert np.array_equal(recorded + np.arange(right - left), addresses[domain])
        if occupied.any():
            stride_views += 1
            line_views += (
                1 if right - left == width else np.count_nonzero(occupied.any(axis=1))
            )
    assert np.array_equal(restored, expected)
    return line_views, stride_views


def child_rectangles(source, first, pitch):
    half = math.pi * EARTH_RADIUS_M
    step = 2 * half / (1 << source["z"])
    x, y = first
    for tile in source["tiles"]:
        west, east = (at * step - half for at in (tile["x"], tile["x"] + 1))
        north, south = (half - at * step for at in (tile["y"], tile["y"] + 1))
        yield (
            math.ceil(west / pitch - x - 0.5),
            math.floor(y + 0.5 - north / pitch) + 1,
            math.ceil(east / pitch - x - 0.5),
            math.floor(y + 0.5 - south / pitch) + 1,
        )


def replay(database, source):
    anchor = source["anchor"]
    centre, _ = local_frame(float(anchor["lat"]), float(anchor["lon"]))
    demand = tuple(float(x + d) for x in centre for d in (12000, -12000))
    totals = np.zeros(2, dtype=np.int64)
    identities = set()
    affected = all_patches = 0
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        keys = db.execute(
            """SELECT a.key FROM bounds b JOIN assets a ON a.id=b.id
               WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=?
                 AND b.minz<=? AND b.maxz>=? AND a.kind='urban-area'
               ORDER BY a.id DESC""",
            demand,
        ).fetchall()
        for (key,) in keys:
            payload = content(db, key)
            fmt, pitch, _, patch, x, y = struct.unpack_from("<QddIqq", payload)
            assert fmt == 0x31414552414E4255
            if (x, y, pitch, patch) in identities:
                continue
            identities.add((x, y, pitch, patch))
            columns, rows = struct.unpack_from("<II", payload, 68)
            count = columns * rows
            tiers = np.frombuffer(payload, "u1", count, len(payload) - count).reshape(
                rows, columns
            )
            cuts = tuple(child_rectangles(source, (x, y), pitch))
            for i in range(0, rows, patch):
                for j in range(0, columns, patch):
                    window = tiers[i : i + patch, j : j + patch]
                    if not window.any():
                        continue
                    height, width = window.shape
                    all_patches += 1
                    clipped = [
                        (
                            max(0, a - j),
                            max(0, b - i),
                            min(width, c - j),
                            min(height, d - i),
                        )
                        for a, b, c, d in cuts
                    ]
                    clipped = [
                        rect
                        for rect in clipped
                        if rect[0] < rect[2] and rect[1] < rect[3]
                    ]
                    if clipped:
                        totals += patch_views(window, clipped)
                        affected += 1
                    else:
                        totals += 1
    return {
        "source_tiles": len(identities),
        "patches": all_patches,
        "clipped": affected,
        "line_views": int(totals[0]),
        "strided_views": int(totals[1]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("footprints", type=Path)
    args = parser.parse_args()
    with gzip.open(args.footprints, "rt") as stream:
        source = json.load(stream)
    began = perf_counter()
    report = replay(args.database, source)
    print(json.dumps(report | {"replay_ms": 1000 * (perf_counter() - began)}))


if __name__ == "__main__":
    main()

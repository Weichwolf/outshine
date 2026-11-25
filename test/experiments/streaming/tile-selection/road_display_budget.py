"""Measure prospective native road LOD demand on a recorded straight camera path.

This reads existing full-detail road manifests. A candidate error budget bounds
the projection if a replacement is independently certified for that error.
The trial does not generate road variants or estimate bytes saved.
"""

import argparse
import json
import sqlite3
import struct
import subprocess
import xml.etree.ElementTree as ET
import zlib
from contextlib import closing
from hashlib import sha256
from pathlib import Path

import numpy as np
from tile_selection import (
    camera_path,
    distance_from_box,
    distance_from_segment,
    local_frame,
    panoramic_projection,
)


class Cursor:
    def __init__(self, data):
        self.content = data
        self.offset = 0

    def get(self, pattern):
        schema = struct.Struct("<" + pattern)
        out = schema.unpack_from(self.content, self.offset)
        self.offset += schema.size
        return out

    def text(self, width=1):
        (size,) = self.get("I")
        start = self.offset
        self.offset += size * width
        assert self.offset <= len(self.content)
        return self.content[start : self.offset]


def read_manifest(content, anchor, count):
    c = Cursor(content)
    (signature,) = c.get("Q")
    version = signature >> 32
    if version not in range(1, 8) or signature & 0xFFFFFFFF != 0x314D4452:
        return
    c.text()
    c.text()
    appearance = c.get("I")[0] if version >= 3 else version == 2
    if appearance:
        c.text()
    lon, lat, pages = c.get("ddI")
    if (lat, lon) != anchor or pages != count:
        return
    result = []
    for _ in range(pages):
        key = c.text()
        if appearance:
            c.text()
        *bounds, n_bytes, level, parts = c.get("ddddddQII")
        error = c.get("d")[0] if version >= 3 and c.get("B")[0] else 0.0
        if version >= 4:
            c.get("I")
        if version >= 5:
            c.get("B")
        if version == 7:
            c.text()
            c.text(8)
        c.offset += parts * 8
        result.append((key, np.asarray(bounds).reshape(2, 3), n_bytes, error, level))
    c.text()
    assert c.offset == len(content)
    return result


def manifests(database, location):
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as conn:
        rows = conn.execute(
            """
          SELECT a.key,p.codec,p.crc,a.offset,a.bytes,coalesce(b.bytes,p.bytes)
          FROM assets a JOIN packages p ON p.key=a.package
          LEFT JOIN package_bodies b ON p.key=b.key
          WHERE a.kind='road-page-manifest'
          AND a.minx<=? AND a.maxx>=? AND a.miny<=? AND a.maxy>=?
          AND a.minz<=? AND a.maxz>=? AND a.bytes<=?
          ORDER BY a.id DESC
        """,
            (*[float(a + b) for a in location for b in (3000, -3000)], 8 * 1024**2),
        )
        for key, codec, checksum, offset, extent, body in rows:
            assert zlib.crc32(body) == checksum
            data = (
                subprocess.run(
                    ["zstd", "-q", "-dc"], input=body, capture_output=True, check=True
                ).stdout
                if codec == 1
                else body
            )
            content = data[offset : offset + extent]
            assert sha256(content).hexdigest() == key
            yield key, content


def measure(baseline, route, location, axes, flight_cut=None, exact_path=False):
    start, finish = camera_path(route, location, axes)
    boxes = np.stack([b for _, b, _, _, _ in baseline])
    distance = (
        distance_from_segment(boxes, start, finish)
        if exact_path
        else distance_from_box(
            boxes, np.minimum(start, finish), np.maximum(start, finish)
        )
    )
    _, depth, displacement = panoramic_projection(route)
    allowed = distance / (displacement * depth + depth + 1)
    errors = (0.125, 0.25, 0.5, 1.0)
    selected = np.searchsorted(errors, allowed, side="right")
    for rank, error in enumerate(errors, 1):
        valid = selected == rank
        layout = distance[valid] / depth - error / depth - error
        assert np.all(layout > 0) and np.all(
            displacement * error / layout <= 1.0000000001
        )
    if flight_cut is not None:
        with flight_cut.open("w") as output:
            for (key, *_), rank in zip(baseline, selected, strict=True):
                if rank:
                    print(key.decode("ascii"), errors[rank - 1], file=output)
    return {
        "pages_by_error_m": {
            str(level): int((selected == i).sum())
            for i, level in enumerate((0, *errors))
        },
        "original_bytes_in_eligible_pages": int(
            sum(
                row[2]
                for row, chosen in zip(baseline, selected, strict=True)
                if chosen > 0
            )
        ),
        "original_bytes_total": int(sum(row[2] for row in baseline)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("still", type=Path)
    parser.add_argument("flight", type=Path)
    parser.add_argument("--manifest", help="select a particular matching manifest")
    parser.add_argument(
        "--flight-cut",
        type=Path,
        help="write selected keys and errors for the exact flight path",
    )
    args = parser.parse_args()
    if args.flight_cut and not args.manifest:
        parser.error("--flight-cut requires --manifest")
    db, scenario, still, flight = args.database, args.scenario, args.still, args.flight
    world = ET.parse(scenario).getroot().find("world")
    anchor = tuple(float(world.attrib[n]) for n in ("lat", "lon"))
    location, axes = local_frame(*anchor)
    still, flight = (json.loads(p.read_text()) for p in (still, flight))
    count = next(
        int(m["value"])
        for m in still["metrics"]
        if m["name"] == "ground candidate: restored road pages"
    )
    native_bytes = next(
        int(m["value"])
        for m in still["metrics"]
        if m["name"] == "ground candidate: restored road package bytes"
    )
    matches = [
        (key, pages)
        for key, content in manifests(db, location)
        if (not args.manifest or key == args.manifest)
        and (pages := read_manifest(content, anchor, count)) is not None
        and sum(row[2] for row in pages) == native_bytes
    ]
    assert matches, "no compatible road manifest at recorded world anchor"
    for key, baseline in matches:
        assert all(
            level == 0 and err == 0 and nbytes > 0
            for _, _, nbytes, err, level in baseline
        )
        assert len({t[0] for t in baseline}) == count
        print(
            json.dumps(
                {
                    "manifest": key,
                    "pages": count,
                    "stationary": measure(baseline, still, location, axes),
                    "flight": measure(baseline, flight, location, axes),
                    "flight_segment": measure(
                        baseline,
                        flight,
                        location,
                        axes,
                        args.flight_cut,
                        exact_path=True,
                    ),
                }
            )
        )


if __name__ == "__main__":
    main()

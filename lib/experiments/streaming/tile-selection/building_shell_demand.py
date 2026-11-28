"""Replay before-read building shell selection on a real cached region."""

import argparse
import json
import math
import sqlite3
import subprocess
import xml.etree.ElementTree as ET
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from tile_selection import (
    camera_path,
    distance_from_box,
    distance_from_segment,
    local_frame,
    panoramic_projection,
)


def cached_shells(database, anchor, radius):
    sql = """
        SELECT a.key,a.bytes,a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz
        FROM bounds b JOIN assets a ON a.id=b.id
        WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=?
          AND b.minz<=? AND b.maxz>=? AND a.kind='building-static-lod' AND a.level=1
    """
    search = tuple(part for at in anchor for part in (at + radius, at - radius))
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        return db.execute(sql, tuple(map(float, search))).fetchall()


def read_certificates(probe, database, keys):
    result = subprocess.run(
        [probe, database],
        input="\n".join(keys) + "\n",
        text=True,
        capture_output=True,
        check=True,
    )
    certificates = {}
    for line in result.stdout.splitlines():
        key, status, *value = line.split()
        error = float(value[0]) if status == "certified" else math.inf
        if status == "certified" and (not math.isfinite(error) or error < 0):
            raise ValueError("invalid certified shell error")
        certificates[key] = (status, error)
    if len(certificates) != len(keys) or certificates.keys() != set(keys):
        raise ValueError("native reader failed to report all candidate shells")
    return certificates


def measure(rows, certificates, route, anchor, axes, guard_m, pixels):
    start, finish = camera_path(route, anchor, axes)
    size = np.asarray([row[1] for row in rows], dtype=np.int64)
    boxes = np.asarray([row[2:] for row in rows]).reshape(-1, 2, 3)
    status, error = zip(*(certificates[key] for key, *_ in rows), strict=True)
    status, error = np.asarray(status), np.asarray(error)
    if not np.all(np.isfinite(boxes)) or np.any(boxes[:, 0] > boxes[:, 1]):
        raise ValueError("invalid stored shell bounds")
    focal, depth, displacement = panoramic_projection(route)
    finite = status == "certified"
    began = perf_counter()
    here = distance_from_box(boxes, start, start) - guard_m
    along = (
        distance_from_box(boxes, np.minimum(start, finish), np.maximum(start, finish))
        - guard_m
    )
    segment = distance_from_segment(boxes, start, finish) - guard_m
    current_shell = (here > error) & (error * focal <= pixels * (here - error))
    fraction = pixels / (displacement * depth + pixels * (depth + 1))
    swept_shell = (along > 0) & (error <= along * fraction)
    segment_shell = (segment > 0) & (error <= segment * fraction)
    if np.any(swept_shell & ~segment_shell) or np.any(segment_shell & ~current_shell):
        raise AssertionError(
            "box sweep <= segment sweep <= current stationary selection"
        )
    selection_ms = (perf_counter() - began) * 1000

    def aggregate(mask):
        return {
            "assets": int(np.count_nonzero(mask)),
            "native_encoded_bytes": int(size[mask].sum()),
        }

    return {
        "sample": aggregate(np.ones(len(rows), dtype=bool)),
        "current_shell": aggregate(current_shell),
        "certified_but_current_reads_then_rejects": aggregate(finite & ~current_shell),
        "decoded_unqualified": aggregate(status == "unqualified"),
        "unknown": aggregate(~(finite | (status == "unqualified"))),
        "safe_over_box_sweep": aggregate(swept_shell),
        "safe_over_segment_sweep": aggregate(segment_shell),
        "current_shell_needs_review_on_movement": aggregate(
            current_shell & ~segment_shell
        ),
        "translation_m": float(np.linalg.norm(finish - start)),
        "eye_guard_m": guard_m,
        "error_px": pixels,
        "selection_ms": selection_ms,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("probe", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--radius-m", type=float, default=12000)
    parser.add_argument("--eye-guard-m", type=float, default=64)
    parser.add_argument("--error-px", type=float, default=1)
    args = parser.parse_args()
    if not all(
        math.isfinite(v) and v > 0
        for v in (args.radius_m, args.eye_guard_m, args.error_px)
    ):
        parser.error(
            "radius, reuse guard, and screen error must be positive and finite"
        )
    world = ET.parse(args.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    rows = cached_shells(args.database, anchor, args.radius_m)
    if not rows:
        raise ValueError("no cached building shell assets near the requested anchor")
    began = perf_counter()
    proof = read_certificates(args.probe, args.database, [row[0] for row in rows])
    archive_probe_ms = (perf_counter() - began) * 1000
    route = json.loads(args.benchmark.read_text())
    report = measure(rows, proof, route, anchor, axes, args.eye_guard_m, args.error_px)
    print(
        json.dumps(
            {"archive_radius_m": args.radius_m, "archive_probe_ms": archive_probe_ms}
            | report
        )
    )


if __name__ == "__main__":
    main()

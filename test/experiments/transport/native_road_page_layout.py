"""Offline packing of unchanged #926 atoms, not a runtime LOD/page producer."""

import hashlib
import json
import sqlite3
import time
import zlib
from collections import defaultdict
from contextlib import closing
from pathlib import Path

import native_road_parts as corpus
import numpy as np
import zstandard


def bounds(parts, payload):
    result = []
    for part in parts:
        start = part["payload_offset"]
        xyz = np.frombuffer(payload, "<f4", part["counts"][0], start).reshape(-1, 3)
        if not len(xyz):
            # An empty native emission still has identity and plan metadata, not coverage.
            enu = np.asarray([row[:3] for row in part["planned_stations"]])
            result.append([*enu.min(axis=0), *enu.max(axis=0)])
            continue
        lo, hi = xyz.min(axis=0).astype(float), xyz.max(axis=0).astype(float)
        result.append([lo[0], -hi[2], lo[1], hi[0], -lo[2], hi[1]])
    return np.asarray(result)


def partition(boxes, sizes, cell_metres, limit_bytes=None):
    """One owner per whole atom; split dense cells without splitting geometry."""
    centres = (boxes[:, :2] + boxes[:, 3:5]) * 0.5
    cells = defaultdict(list)
    for index, cell in enumerate(np.floor(centres / cell_metres).astype(np.int64)):
        cells[tuple(cell)].append(index)
    result = []
    pending = list(cells.values())
    while pending:
        group = pending.pop()
        if limit_bytes is None or sum(sizes[i] for i in group) <= limit_bytes:
            result.append(sorted(group))
            continue
        if len(group) == 1:
            result.append(group)
            continue
        axis = int(np.argmax(np.ptp(centres[group], axis=0)))
        ordered = sorted(group, key=lambda i: (centres[i, axis], i))
        middle = len(ordered) // 2
        pending.extend((ordered[:middle], ordered[middle:]))
    return sorted(result, key=min)


def route(parts, metadata, boxes, samples, mode):
    core = set(metadata["core_edges"])
    candidates = [p for p in parts if p["kind"] == "edge" and p["edge"] in core]

    def length(part):
        xy = np.asarray([s[:2] for s in part["planned_stations"]])
        return np.linalg.norm(np.diff(xy, axis=0), axis=1).sum()

    edge = max(candidates, key=length)
    if mode == "dense":
        centres = (boxes[:, :2] + boxes[:, 3:5]) * 0.5
        cells = defaultdict(list)
        for i, part in enumerate(parts):
            if part["kind"] == "edge" and part["edge"] in core:
                cells[tuple(np.floor(centres[i] / 256.0))].append(i)
        dense = max(cells.values(), key=len)
        centre = centres[dense].mean(axis=0)
        centre = centres[min(dense, key=lambda i: np.linalg.norm(centres[i] - centre))]
        xy = np.asarray([centre + [-150.0, 0.0], centre + [150.0, 0.0]])
    elif mode == "corridor":
        xy = np.asarray([s[:2] for s in edge["planned_stations"]])
    else:
        raise ValueError("unknown diagnostic route")
    distances = np.r_[0.0, np.cumsum(np.linalg.norm(np.diff(xy, axis=0), axis=1))]
    targets = np.linspace(0, distances[-1], samples)
    points = np.column_stack(
        [np.interp(targets, distances, xy[:, axis]) for axis in (0, 1)]
    )
    # This is a deterministic synthetic demand path on a captured plan, not a camera recording.
    trace = [("start", points[0]), ("hold-start", points[0])]
    trace += [(f"move-{i}", p) for i, p in enumerate(points[1:], 1)]
    trace += [("hold-end", points[-1])]
    trace += [(f"return-{i}", p) for i, p in enumerate(points[-2::-1])]
    trace += [("hold-return", points[0])]
    return edge["edge"] if mode == "corridor" else None, float(distances[-1]), trace


def demands(parts, metadata, boxes, samples=17, radius=150.0, mode="corridor"):
    """Fine-bound ROI oracle plus actual incident junction closure, not occlusion."""
    edges = {p["edge"]: i for i, p in enumerate(parts) if p["kind"] == "edge"}
    junctions = {p["node"]: i for i, p in enumerate(parts) if p["kind"] == "junction"}
    core = np.asarray([edges[e] for e in metadata["core_edges"]], dtype=int)
    edge_id, metres, positions = route(parts, metadata, boxes, samples, mode)
    frames = []
    for name, point in positions:
        separation = np.maximum(
            np.maximum(boxes[core, :2] - point, point - boxes[core, 3:5]), 0
        )
        seeds = core[np.linalg.norm(separation, axis=1) <= radius]
        wanted = set(map(int, seeds))
        nodes = {n for i in wanted for n in parts[i]["nodes"]}
        selected_junctions = [junctions[n] for n in nodes if n in junctions]
        wanted.update(selected_junctions)
        for i in selected_junctions:
            wanted.update(edges[leg[0]] for leg in parts[i]["legs"])
        selected = [parts[i] for i in sorted(wanted)]
        corpus.checked_contacts(selected, metadata)
        frames.append(
            {"name": name, "position_en": point.tolist(), "parts": sorted(wanted)}
        )
    return {
        "route_mode": mode,
        "route_edge": edge_id,
        "route_metres": metres,
        "radius_metres": radius,
        "frames": frames,
    }


SCHEMA = """
CREATE TABLE assets(id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE,kind TEXT NOT NULL,
 minx REAL NOT NULL,miny REAL NOT NULL,minz REAL NOT NULL,maxx REAL NOT NULL,
 maxy REAL NOT NULL,maxz REAL NOT NULL,package TEXT NOT NULL,offset INTEGER NOT NULL,
 bytes INTEGER NOT NULL,level INTEGER NOT NULL,parent TEXT NOT NULL,
 request_key TEXT NOT NULL DEFAULT '');
CREATE VIRTUAL TABLE bounds USING rtree(id,minx,maxx,miny,maxy,minz,maxz);
CREATE TABLE packages(key TEXT PRIMARY KEY,bytes BLOB NOT NULL,crc INTEGER NOT NULL,
 codec INTEGER NOT NULL DEFAULT 0,native_bytes INTEGER NOT NULL DEFAULT 0);
CREATE UNIQUE INDEX asset_requests ON assets(request_key) WHERE request_key<>'';
PRAGMA user_version=3;
"""


def prepare(directory, groups, parts, boxes, payload, materials, scope):
    """Private cost fixture in existing AssetCache schema; no native asset serializer."""
    all_ids = [i for group in groups for i in group]
    if sorted(all_ids) != list(range(len(parts))):
        raise ValueError("atoms must have exactly one package owner")
    directory = Path(directory)
    directory.mkdir()
    started = time.perf_counter()
    with closing(sqlite3.connect(directory / "assets.sqlite")) as connection:
        connection.executescript(SCHEMA)
        connection.execute("PRAGMA cache_size=-4096")
        compressor = zstandard.ZstdCompressor(level=1)
        hashes, layout, raw_bytes, stored_bytes, joined_bytes = {}, [], 0, 0, 0
        published = set()
        source = memoryview(payload)
        for group in [*groups, [len(parts)]]:
            slices = []
            for i in group:
                if i == len(parts):
                    slices.append(memoryview(materials))
                else:
                    p = parts[i]
                    slices.append(
                        source[
                            p["payload_offset"] : p["payload_offset"]
                            + p["payload_bytes"]
                        ]
                    )
            raw = b"".join(slices)
            joined_bytes += len(raw)
            packed = compressor.compress(raw)
            codec = int(len(raw) >= 64 and len(packed) < len(raw))
            stored = packed if codec else raw
            package = hashlib.sha256(raw).hexdigest()
            if package not in published:
                connection.execute(
                    "INSERT INTO packages VALUES(?,?,?,?,?)",
                    (package, stored, zlib.crc32(stored), codec, len(raw)),
                )
                published.add(package)
                raw_bytes += len(raw)
                stored_bytes += len(stored)
                layout.append(
                    {
                        "atoms": len(group),
                        "native_bytes": len(raw),
                        "stored_bytes": len(stored),
                    }
                )
            cursor = 0
            for i, view in zip(group, slices):
                digest = hashlib.sha256(view).hexdigest()
                # Capture-local semantic atom identity is qualified by the frozen source digest.
                key = hashlib.sha256(
                    f"native-corpus-atom:{scope}:{i}:{digest}".encode()
                ).hexdigest()
                box = (
                    boxes[i]
                    if i < len(parts)
                    else np.r_[boxes[:, :3].min(0), boxes[:, 3:].max(0)]
                )
                values = (
                    i + 1,
                    key,
                    "native-road-oracle",
                    *box,
                    package,
                    cursor,
                    len(view),
                    0,
                    "",
                    "",
                )
                connection.execute(
                    "INSERT INTO assets VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", values
                )
                connection.execute(
                    "INSERT INTO bounds VALUES(?,?,?,?,?,?,?)",
                    (i + 1, box[0], box[3], box[1], box[4], box[2], box[5]),
                )
                hashes[str(i + 1)] = digest
                cursor += len(view)
        connection.commit()
    (directory / "hashes.json").write_text(json.dumps(hashes, sort_keys=True))
    return {
        "packages": layout,
        "native_bytes": raw_bytes,
        "stored_bytes": stored_bytes,
        "explicit_join_copy_bytes": joined_bytes,
        "prepare_ms": (time.perf_counter() - started) * 1000,
        "database_bytes": (directory / "assets.sqlite").stat().st_size,
        "verification_metadata_bytes": (directory / "hashes.json").stat().st_size,
    }

"""Compare native geometry cuts using existing cached source metadata, without mesh IO."""

import argparse
import ctypes
import ctypes.util
import json
import hashlib
import math
from pathlib import Path
import sqlite3
import struct
import subprocess
import time

import numpy as np

from panoramic_geometry_cut import PanoramicView
from source_geometry_cut import GeometrySources
from source_product_cut import SourceProducts
from source_product_admission import admit, ordinals


MEMBER = np.dtype([("cell", "<u4"), ("field", "u1"), ("faces", "<u4"), ("shell", "<f8")])


class Cache:
    def __init__(self, database):
        self.db = sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
        self.db.row_factory = sqlite3.Row
        library = ctypes.util.find_library("zstd")
        if library is None:
            directory = Path(subprocess.check_output(["pkg-config", "--variable=libdir", "libzstd"], text=True).strip())
            library = next((str(p) for p in directory.glob("libzstd.*") if p.suffix in (".so", ".dylib")), None)
        if library is None:
            raise ValueError("installed libzstd not found")
        self.zstd = ctypes.CDLL(library)
        self.zstd.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                             ctypes.c_void_p, ctypes.c_size_t]
        self.zstd.ZSTD_decompress.restype = ctypes.c_size_t
        self.package_bytes = 0
        self.native_bytes = 0

    def load(self, record):
        row = self.db.execute(
            "SELECT p.codec,p.native_bytes,CASE WHEN length(p.bytes)>0 THEN p.bytes ELSE b.bytes END AS data "
            "FROM packages p LEFT JOIN package_bodies b ON b.key=p.key WHERE p.key=?",
            (record["package"],)).fetchone()
        if row is None or row["data"] is None:
            raise ValueError("missing metadata package")
        data = row["data"]
        self.package_bytes += len(data)
        if row["codec"] == 1:
            decoded = ctypes.create_string_buffer(row["native_bytes"])
            size = self.zstd.ZSTD_decompress(decoded, len(decoded), data, len(data))
            if size != len(decoded):
                raise ValueError("invalid compressed metadata")
            data = decoded.raw
        elif row["codec"] != 0:
            raise ValueError("unknown package codec")
        if len(data) != row["native_bytes"]:
            raise ValueError("metadata package length differs")
        first, count = record["offset"], record["bytes"]
        if first < 0 or count < 0 or first + count > len(data):
            raise ValueError("metadata record exceeds package")
        self.native_bytes += len(data)
        return data[first:first + count]


def geometry_inputs(data, index, revision):
    if len(data) < 12:
        raise ValueError("source member header missing")
    magic, count = struct.unpack_from("<QI", data)
    if magic not in (0x3250524752435342, 0x3350524752435342) or len(data) != 12 + count * MEMBER.itemsize:
        raise ValueError("source member schema differs")
    members = np.frombuffer(data, MEMBER, count, 12)
    maximum_state = 2 if magic == 0x3350524752435342 else 1
    if (np.any(members["field"] > maximum_state) or np.any(members["cell"] == 0)
            or np.any(members["cell"] > 64)
            or np.any((members["faces"] == 0) & ((members["field"] == 1) | (members["shell"] != -1)))
            or np.any((members["field"] == 2) & (members["faces"] != 0))):
        raise ValueError("invalid source traits")
    tree = SourceProducts(index, list(map(int, members["cell"])),
                          bytes((members["field"] == 1).astype(np.uint8)), revision=revision)
    revision_bytes = revision.encode()
    encoded = struct.pack("<QII", 0x3152544D4F454753, count, len(revision_bytes)) + revision_bytes
    encoded += b"".join(struct.pack("<dI", r["shell"], r["faces"]) for r in members)
    return members, GeometrySources(tree, encoded)


def inputs(cache, record):
    data = cache.load(record)
    index = cache.db.execute("SELECT * FROM assets WHERE parent=? AND kind='spatial-page-index'",
                             (record["parent"],)).fetchone()
    if index is None:
        raise ValueError("source hierarchy missing")
    return geometry_inputs(data, cache.load(index), record["parent"])


def source_records(cache):
    records = {}
    for record in cache.db.execute("SELECT * FROM assets WHERE kind='building-source-groups'"):
        for version in (2, 3):
            expected = hashlib.sha256(
                (f"building-source-groups-{version}/" + record["parent"]).encode()
            ).hexdigest()
            if record["key"] == expected:
                previous = records.get(record["parent"])
                if previous is None or previous[0] < version:
                    records[record["parent"]] = version, record
    return [record for _, record in records.values()]


class ShellView(PanoramicView):
    def hull_error(self, bounds):
        return math.inf


def ranges(ordinals):
    result = []
    for ordinal in sorted(ordinals):
        if result and sum(result[-1]) == ordinal:
            result[-1][1] += 1
        else:
            result.append([ordinal, 1])
    return result


def coalesce(sources, products):
    groups = {}
    covered = set()
    for product in products:
        node = sources.tree.nodes[product.node]
        members = sources.tree.order[node[6]:node[6] + node[7]]
        if covered.intersection(members):
            raise ValueError("product source coverage overlaps")
        covered.update(members)
        cell = sources.tree.cohorts[product.node]
        if cell is None:
            raise ValueError("coalesced geometry crosses source cohorts")
        kind = {"original": "fine", "unresolved": "unknown"}.get(product.kind, product.kind)
        group = groups.setdefault((cell, kind), {"cell": cell, "kind": kind,
            "sources": [], "source_faces": 0, "supports": []})
        group["sources"].extend(members)
        group["source_faces"] += sum(sources.faces[at] for at in members)
        group["supports"].append({"node": product.node,
                                   "error_m": product.error_m})
    if covered != set(range(len(sources.tree.order))):
        raise ValueError("coalesced products leave source coverage missing")
    for group in groups.values():
        group["source_ranges"] = ranges(group.pop("sources"))
    return list(groups.values())


def cut(sources, eye, viewport, focal, allowed, hulls):
    view = (PanoramicView if hulls else ShellView)(tuple(eye), focal, *viewport)
    products, visited = sources.cut(view, allowed, cell_cohorts=True)
    kinds = {kind: {"products": 0, "sources": 0, "source_faces": 0}
             for kind in ("fine", "shell", "hull", "unknown")}
    for product in products:
        kind = {"original": "fine", "unresolved": "unknown"}.get(product.kind, product.kind)
        node = sources.tree.nodes[product.node]
        members = sources.tree.order[node[6]:node[6] + node[7]]
        row = kinds[kind]
        row["products"] += 1
        row["sources"] += len(members)
        row["source_faces"] += sum(sources.faces[at] for at in members)
    groups = coalesce(sources, products)
    unknown_cells = {group["cell"] for group in groups if group["kind"] == "unknown"}
    return {"visited_nodes": visited, "representations": kinds,
            "groups": groups,
            "cell_parent_admission": cell_parent_admission(groups),
            "known_fine_sources_sharing_cells_with_unknown": sum(
                sum(count for _, count in group["source_ranges"])
                for group in groups if group["kind"] == "fine" and group["cell"] in unknown_cells)}


def cell_parent_admission(groups):
    cells = {}
    for group in groups:
        cells.setdefault(group["cell"], []).extend(ordinals(group["source_ranges"]))
    parents = [ranges(members) for members in cells.values()]
    decision = admit(parents, groups)
    accepted = set(decision["generate"])
    deferred = [group for at, group in enumerate(groups)
                if at not in accepted and group["kind"] != "unknown"]
    return {"scope": "model: one indivisible original parent per source cell; not resident World coverage",
            "generate_groups": len(accepted),
            "keep_parent_cells": [cell for at, cell in enumerate(cells)
                                  if at in decision["keep_parents"]],
            "deferred_known_groups": len(deferred),
            "deferred_known_sources": sum(count for group in deferred
                                           for _, count in group["source_ranges"]),
            "deferred_source_faces": sum(group["source_faces"] for group in deferred)}


def ecef(latitude, longitude, height):
    phi, lam = map(math.radians, (latitude, longitude))
    eccentricity = (1 / 298.257223563) * (2 - 1 / 298.257223563)
    normal = 6378137 / math.sqrt(1 - eccentricity * math.sin(phi) ** 2)
    return np.array(((normal + height) * math.cos(phi) * math.cos(lam),
                     (normal + height) * math.cos(phi) * math.sin(lam),
                     (normal * (1 - eccentricity) + height) * math.sin(phi)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--eye", type=float, nargs=3, required=True, metavar=("LAT", "LON", "ASL_M"))
    parser.add_argument("--fov", type=float, required=True)
    parser.add_argument("--viewport", type=int, nargs=2, default=(1280, 720))
    parser.add_argument("--error-px", type=float, default=1)
    parser.add_argument("--tiles", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if (not all(math.isfinite(x) for x in (*args.eye, args.error_px, args.fov))
            or not 0 < args.fov < 180 or min(args.viewport) <= 0
            or args.error_px < 0 or args.tiles <= 0):
        parser.error("finite positive viewport/FOV/tile count and nonnegative error required")
    eye = ecef(*args.eye)
    cache = Cache(args.database)
    records = source_records(cache)
    records.sort(key=lambda r: sum(max(r["min" + a] - eye[i], 0, eye[i] - r["max" + a]) ** 2
                                   for i, a in enumerate("xyz")))
    reports = []
    began = time.perf_counter()
    for record in records[:args.tiles]:
        members, sources = inputs(cache, record)
        focal = args.viewport[1] / (2 * math.tan(math.radians(args.fov) / 2))
        reports.append({"base_key": record["parent"], "sources": len(members),
                        "known_empty_sources": int(np.count_nonzero(members["field"] == 2)),
                        "source_faces": int(members["faces"].astype(np.uint64).sum()),
                        "fine_shell": cut(sources, eye, args.viewport, focal, args.error_px, False),
                        "fine_shell_hull": cut(sources, eye, args.viewport, focal, args.error_px, True)})
    result = {"scope": "metadata demand model; face counts are neither triangles nor measured GPU cost; hull products are hypothetical",
              "inputs": {"eye": args.eye, "viewport": args.viewport, "fov": args.fov, "error_px": args.error_px},
              "package_bytes_read": cache.package_bytes, "native_metadata_bytes": cache.native_bytes,
              "seconds": time.perf_counter() - began, "tiles": reports}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

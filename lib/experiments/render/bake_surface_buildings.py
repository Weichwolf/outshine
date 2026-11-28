"""Compose native building groups with packed terrain grids for the Metal benchmark."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np

from building_plan_inputs import read_plans
from pack_surface_tile import HEADER, INDEX, VERTEX, pack
from terrain_cached_page import package


MAX_VERTICES = 65536
MAX_INDICES = 6 * MAX_VERTICES


def grid(path):
    content = path.read_bytes()
    header = HEADER.unpack_from(content)
    magic, z, x, y, lon, lat = header[:6]
    error, vertices, indices = header[-3:]
    if magic != b"OUTSHST2" or not 0 <= z <= 29 or max(x, y) >= 1 << z:
        raise ValueError(f"{path}: expected a regular terrain grid")
    side = INDEX.unpack_from(content, HEADER.size)[0]
    vertex_start = HEADER.size + INDEX.size
    index_start = vertex_start + VERTEX.size * vertices
    if (side < 2 or vertices != side**2 or vertices > MAX_VERTICES or indices > MAX_INDICES
            or len(content) != index_start + INDEX.size * indices):
        raise ValueError(f"{path}: terrain counts do not match the payload")
    return (z, x, y), (lon, lat), error, side, index_start, content


def in_extent(points, extent):
    lat, lon = points.T
    if extent is None:
        return np.ones(len(points), dtype=bool)
    west, south, east, north = extent
    match = (lon >= west) & (lon <= east) if west <= east else (lon >= west) | (lon <= east)
    return match & (lat >= south) & (lat <= north)


def boundary_edges(points):
    vertices = [tuple(point) for point in points]
    if len(vertices) > 1 and vertices[0] == vertices[-1]:
        vertices.pop()
    edges = zip(vertices, vertices[1:] + vertices[:1])
    return tuple(sorted(tuple(sorted(edge)) for edge in edges))


def footprint_key(source, ordinal):
    record = source["records"][ordinal]
    points = source["points_latlon"]
    first, count = record["local_first"], record["point_count"]
    first_hole, hole_count = record["first_hole"], record["hole_count"]
    rings = source["holes"][first_hole:first_hole + hole_count]
    holes = tuple(sorted(
        (int(h["exterior"]), boundary_edges(points[h["first"]:h["first"] + h["count"]]))
        for h in rings
    ))
    return (record["source_id"], record["minimum_height_m"],
            boundary_edges(points[first:first + count]), holes)


def unseen_footprints(source, ids, seen):
    keep = []
    for ordinal in ids:
        identity = footprint_key(source, ordinal)
        if identity in seen:
            continue
        seen.add(identity)
        keep.append(ordinal)
    return np.asarray(keep, dtype=np.intp)


def building_groups(store, keys, addresses, extent, deduplicate=False):
    groups = {tile: {} for tile in addresses}
    levels = sorted({tile[0] for tile in addresses})
    seen = set()
    for key in sorted(set(keys)):
        body, _ = package(store, key, kind="buildings")
        source = read_plans(body)
        positions = source["points_latlon"][[record["local_first"] for record in source["records"]]]
        supported = np.asarray([row["supported"] for row in source["summaries"]], dtype=bool)
        ids = np.flatnonzero(in_extent(positions, extent) & supported)
        if deduplicate:
            ids = unseen_footprints(source, ids, seen)
        lat, lon = positions[ids].T
        x = (lon + 180) / 360
        y = (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2
        mercator = np.column_stack((x, y))
        for level in levels:
            side = 1 << level
            tiles = np.clip((mercator * side).astype(np.int64), 0, side - 1)
            for ordinal, (column, row) in zip(ids, tiles):
                group = groups.get((level, int(column), int(row)))
                if group is not None:
                    group.setdefault(key, [len(positions), []])[1].append(int(ordinal))
    verify_partitions(groups)
    return groups


def verify_partitions(groups):
    for (z, x, y), parent in groups.items():
        children = [groups.get((z + 1, 2 * x + dx, 2 * y + dy))
                    for dy in range(2) for dx in range(2)]
        if any(child is None for child in children):
            continue
        expected = sorted((key, ordinal) for key, (_, ids) in parent.items() for ordinal in ids)
        actual = sorted((key, ordinal) for child in children for key, (_, ids) in child.items()
                        for ordinal in ids)
        if actual != expected:
            raise ValueError(f"incomplete building coverage at {(z, x, y)}")


def compose(address, origin, error, side, index_start, terrain, overlay):
    geometry = json.loads(overlay)
    ground = np.frombuffer(terrain, dtype="<f4", offset=HEADER.size + INDEX.size,
                           count=6 * side**2).reshape(-1, 6)
    indices = np.frombuffer(terrain, dtype="<u4", offset=index_start)
    positions = ground[:, :3].tolist() + geometry["vertices"]
    normals = ground[:, 3:].tolist() + geometry["normals"]
    indices_out = indices.tolist() + [side**2 + corner for corner in geometry["indices"]]
    if len(positions) > MAX_VERTICES or len(indices_out) > MAX_INDICES:
        raise ValueError(f"{address}: building group exceeds packed surface capacity")
    points = np.asarray(positions, dtype=np.float32)
    return pack({
        "version": 1,
        "grid_side": side,
        "ground_indices": len(indices),
        "coverage": address,
        "origin_lon_lat": origin,
        "geometric_error_m": error,
        "bounds_eus_m": [points.min(axis=0).tolist(), points.max(axis=0).tolist()],
        "revision": hashlib.sha256(terrain + overlay).hexdigest(),
        "vertices_eus_m": positions,
        "normals_eus": normals,
        "indices": indices_out,
        "colours_rgba8": geometry["colours"],
    })


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_store", type=Path)
    parser.add_argument("geometry_store", type=Path)
    parser.add_argument("exporter", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("terrain", type=Path, nargs="+")
    parser.add_argument("--building-key", action="append", required=True)
    parser.add_argument("--fine-level", type=int, required=True)
    parser.add_argument("--extent", type=float, nargs=4, metavar=("WEST", "SOUTH", "EAST", "NORTH"))
    parser.add_argument("--deduplicate", action="store_true",
                        help="merge exact footprints and inner rings with matching source and base height")
    args = parser.parse_args()
    if not 0 <= args.fine_level <= 29 or args.output.exists():
        parser.error("use a valid fine level and a new output directory")
    if args.extent is not None:
        west, south, east, north = args.extent
        if not (-180 <= west <= 180 and -180 <= east <= 180 and -90 <= south <= north <= 90):
            parser.error("invalid geographic extent")
    grids = sorted((grid(path) for path in args.terrain), key=lambda item: item[0])
    addresses = [item[0] for item in grids]
    if len(set(addresses)) != len(grids):
        parser.error("duplicate terrain address")
    groups = building_groups(args.source_store, args.building_key, addresses, args.extent, args.deduplicate)
    requests = [{"origin": origin, "shell": address[0] < args.fine_level,
                 "sources": groups[address], "vertices_left": MAX_VERTICES - side**2,
                 "indices_left": MAX_INDICES - (len(content) - index_start) // INDEX.size}
                for address, origin, _, side, index_start, content in grids]
    with tempfile.TemporaryDirectory(prefix="outshine-surface-buildings-") as work:
        folder = Path(work)
        manifest = folder / "requests.json"
        manifest.write_text(json.dumps(requests), encoding="utf-8")
        subprocess.run([args.exporter, args.source_store, args.geometry_store, manifest, folder],
                       check=True)
        with tempfile.TemporaryDirectory(prefix=".surface-buildings-", dir=args.output.parent) as staged:
            tiles = Path(staged) / "tiles"
            tiles.mkdir()
            for number, item in enumerate(grids):
                name = "-".join(map(str, item[0])) + ".surface"
                (tiles / name).write_bytes(compose(*item, (folder / f"{number}.json").read_bytes()))
            tiles.rename(args.output)


if __name__ == "__main__":
    main()

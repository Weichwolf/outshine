"""Compare source-complete mixed terrace and footprint geometry on Metal."""

import argparse
import gzip
import json
import math
import struct
from collections import defaultdict
from pathlib import Path
from time import perf_counter

import numpy as np

from footprint_cards import catalogue
from footprint_flight import Terrain, seat
from footprint_mesh import mesh, write_prisms, write_products
from footprint_terraces import (
    GROUP_METRES,
    NO_ROOF,
    footprint_support,
    prism_grid_tiles,
    raster,
)
from group_footprints import read_source


def cells(parts, terrain):
    groups = defaultdict(list)
    for index, part in enumerate(parts):
        west, north, east, south = part.footprint.bounds
        centre = np.asarray((west + east, north + south)) * 0.5
        key = np.floor((terrain.origin + centre / terrain.span_m) * 8)
        groups[tuple(key.astype(int))].append(index)
    return groups.values()


def partition(indices, raw, seated, spacing, support, rounds):
    selected = [index for index in indices if raw[index].bottom_m == 0]
    for _ in range(rounds + 1):
        if len(selected) < 2:
            break
        grid = raster([seated[index] for index in selected], spacing)
        if (grid[2] == NO_ROOF).all():
            break
        covered = [
            index
            for index in selected
            if footprint_support(seated[index], spacing, grid) <= support
        ]
        if len(covered) == len(selected):
            return covered, grid
        selected = covered
    return [], None


def face_count(part):
    polygon = part.footprint
    rings = (polygon.exterior, *polygon.interiors)
    vertices = sum(len(ring.coords) - 1 for ring in rings)
    return 3 * vertices + 2 * len(polygon.interiors) - 2


def replace_sources(parts, terrain, spacing, support, rounds):
    seated = seat(parts, terrain)
    raster_parts = set()
    grids = []
    for indices in cells(parts, terrain):
        selected, grid = partition(indices, parts, seated, spacing, support, rounds)
        if grid is None:
            continue
        patches = list(prism_grid_tiles(grid, spacing))
        # Admit only if the grid is smaller than the original index buffer alone.
        saved_indices = sum(face_count(parts[index]) for index in selected) * 3 * 4
        if sum(spans.nbytes + 164 for _, spans in patches) >= saved_indices:
            continue
        raster_parts.update(selected)
        grids.extend(patches)
    remain = [part for index, part in enumerate(seated) if index not in raster_parts]
    return seated, remain, grids


def view_header(radius, yaw):
    yaw = math.radians(yaw)
    forward = np.asarray((-math.sin(yaw) * radius, -1100, -math.cos(yaw) * radius))
    eye = -forward
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, (0, 1, 0))
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    return struct.pack("<13d", *eye, *eye, *forward, *up, math.radians(50))


def mesh_groups(parts):
    for group in catalogue(parts, GROUP_METRES):
        yield mesh(group, False), mesh(group, True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "dem", "output"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--extent-m", type=float, default=1600)
    parser.add_argument("--distance-m", type=float, default=16000)
    parser.add_argument("--heading", type=float, default=0)
    parser.add_argument("--rounds", type=int, default=2, choices=range(4))
    args = parser.parse_args()
    if min(args.distance_m, args.extent_m) <= 0:
        parser.error("distance and extent must be positive")
    started = perf_counter()
    with gzip.open(args.source, "rt") as source:
        terrain = Terrain(args.dem, json.load(source))
    parts = [
        part
        for part in read_source(args.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y))
        <= args.extent_m
    ]
    if not parts:
        parser.error("the selected window has no source footprints")
    spacing, support = 8, 16
    seated, remain, grids = replace_sources(parts, terrain, spacing, support, args.rounds)
    cooked = perf_counter() - started
    header = view_header(args.distance_m, args.heading)
    args.output.mkdir(parents=True, exist_ok=True)
    _, full_faces = write_products(
        args.output / "reference.mesh", header, mesh_groups(seated)
    )
    counts = {"parts": len(parts), "retained": len(remain), "source_faces": full_faces}
    if grids:
        raw_mesh, raw_grid = (args.output / "remainder.mesh", args.output / "grid.prisms")
        _, faces = write_products(raw_mesh, header, mesh_groups(remain))
        patch_count = write_prisms(raw_grid, header, grids, spacing, terrain)
        with (args.output / "hybrid.product").open("wb") as output:
            output.write(struct.pack("<Q", raw_mesh.stat().st_size))
            output.write(raw_mesh.read_bytes())
            output.write(raw_grid.read_bytes())
        counts.update(grid_patches=patch_count, triangles=faces + 2 * patch_count)
    print(json.dumps(dict(counts, preparation_s=cooked)))


if __name__ == "__main__":
    main()

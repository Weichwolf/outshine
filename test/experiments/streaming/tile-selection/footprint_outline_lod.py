"""Build and check a camera-neutral shell LOD before native mesh upload."""

import argparse
import gzip
import json
import math
from pathlib import Path
from time import perf_counter

import numpy as np
from shapely import (
    boundary,
    get_num_coordinates,
    get_num_interior_rings,
    hausdorff_distance,
    is_valid,
    segmentize,
    simplify,
)

from footprint_cards import catalogue
from footprint_flight import Terrain, seat
from footprint_mesh import mesh, write_products
from footprint_terraces import GROUP_METRES
from group_footprints import Extrusion, read_source
from terrace_remainders import view_header


def shell_lod(seated, tolerance_m, error_m):
    source = np.array([part.footprint for part in seated], dtype=object)
    simplified = simplify(source, tolerance_m, preserve_topology=True)
    candidates = (
        (get_num_coordinates(simplified) < get_num_coordinates(source))
        & (get_num_interior_rings(source) == get_num_interior_rings(simplified))
        & is_valid(simplified)
    )
    selected = np.flatnonzero(candidates)
    # Distance to a boundary is 1-Lipschitz, so add half the sampling gap in both directions.
    gap_m = min(1.0, max(0.25, error_m / 4))
    source_edges = segmentize(boundary(source[selected]), gap_m)
    lod_edges = segmentize(boundary(simplified[selected]), gap_m)
    upper_m = hausdorff_distance(source_edges, lod_edges) + gap_m / 2
    accepted = upper_m < error_m
    used = selected[accepted]
    footprint = source.copy()
    footprint[used] = simplified[used]
    lod = [
        Extrusion(shape, part.bottom_m, part.top_m)
        for shape, part in zip(footprint, seated)
    ]
    return lod, len(used), float(upper_m[accepted].max(initial=0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("dem", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--extent-m", type=float, default=1600)
    parser.add_argument("--tolerance-m", type=float, default=2)
    parser.add_argument("--error-m", type=float, default=3)
    args = parser.parse_args()
    sizes_m = (args.extent_m, args.tolerance_m, args.error_m)
    if not all(math.isfinite(metres) and metres > 0 for metres in sizes_m):
        parser.error("extent, tolerance and error must be finite and positive")
    began = perf_counter()
    with gzip.open(args.source, "rt") as source:
        terrain = Terrain(args.dem, json.load(source))
    parts = [
        part for part in read_source(args.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y)) <= args.extent_m
    ]
    if not parts:
        parser.error("the selected window has no source footprints")
    seated = seat(parts, terrain)
    planned = perf_counter()
    lod, simplified, measured_error = shell_lod(seated, args.tolerance_m, args.error_m)
    elapsed = perf_counter() - planned
    args.output.mkdir(parents=True, exist_ok=True)
    header = view_header(5000, 0, 250, 120)
    member_lod = {id(source): detail for source, detail in zip(seated, lod)}
    group_members = catalogue(seated, GROUP_METRES)
    coarse_groups = ([member_lod[id(part)] for part in group] for group in group_members)
    counts = [
        write_products(
            args.output / f"{key}.mesh",
            header,
            ((mesh(group, False), mesh(group, True)) for group in products),
        )
        for key, products in (("source", group_members), ("lod", coarse_groups))
    ]
    print(
        json.dumps(
            {
                "parts": len(parts),
                "simplified": simplified,
                "preparation_s": elapsed,
                "largest_outline_bound_m": measured_error,
                "source_triangles": counts[0][1],
                "lod_triangles": counts[1][1],
                "fixture_export_s": perf_counter() - began,
            }
        )
    )


if __name__ == "__main__":
    main()

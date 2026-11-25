"""Estimate which camera-neutral building proxies can replace source footprint pages."""

import argparse
import json
import math
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import read_source
from shapely import distance, get_coordinates, oriented_envelope, points
from tile_selection import camera_path, distance_from_segment, panoramic_projection


def rectangle_candidates(parts, faces):
    error = np.full(len(parts), np.inf)
    # A containing rectangle's corner distances bound both convex wall boundaries.
    # A courtyard, concave wall, or raised part has no such guarantee.
    eligible = np.array(
        [
            i
            for i, part in enumerate(parts)
            if faces[i] > 10
            and part.bottom_m == 0
            and not part.footprint.interiors
            and part.footprint.equals(part.footprint.convex_hull)
        ],
        dtype=np.intp,
    )
    if eligible.size:
        outlines = np.array([parts[i].footprint for i in eligible])
        rectangles = oriented_envelope(outlines)
        corners = get_coordinates(rectangles).reshape(-1, 5, 2)[:, :4]
        error[eligible] = distance(points(corners), outlines[:, None]).max(axis=1)
    return error


def footprint_faces(part):
    count = len(part.footprint.exterior.coords) - 1
    count += sum(len(ring.coords) - 1 for ring in part.footprint.interiors)
    return 3 * count + 2 * len(part.footprint.interiors) - 2


def proxy_profile(parts):
    boxes = np.array([part.footprint.bounds for part in parts]).reshape(-1, 2, 2)
    faces = np.fromiter(map(footprint_faces, parts), dtype=np.int64, count=len(parts))
    heights = np.array([p.top_m - p.bottom_m for p in parts])
    diameter = np.hypot(np.linalg.norm(boxes[:, 1] - boxes[:, 0], axis=1), heights)
    # Preserve visible gaps and underpasses rather than treating them as solid proxies.
    structural = np.array(
        [p.bottom_m > 0 or bool(p.footprint.interiors) for p in parts]
    )
    diameter[structural] = np.inf
    return boxes, faces, diameter, rectangle_candidates(parts, faces)


def page_bounds(cells, boxes):
    identifiers, order = np.unique(cells, axis=0, return_inverse=True)
    low = np.full((len(identifiers), 2), np.inf)
    high = np.full_like(low, -np.inf)
    np.minimum.at(low, order, boxes[:, 0])
    np.maximum.at(high, order, boxes[:, 1])
    return np.stack((low, high), axis=1), order


def build_catalogue(profile, page):
    boxes, faces, diameter, _ = profile
    cells = np.floor(boxes.mean(axis=1) / page).astype(np.int64)
    _, group = np.unique(cells, axis=0, return_inverse=True)
    finite = np.isfinite(diameter)
    bands = np.full(len(boxes), np.iinfo(np.int64).max)
    bands[finite] = np.floor(np.log2(diameter[finite]))
    split_pages, split_group = page_bounds(np.column_stack((cells, bands)), boxes)
    split_diameter = np.zeros(len(split_pages))
    np.maximum.at(split_diameter, split_group, diameter)
    face_savings = np.zeros(len(split_pages), dtype=np.int64)
    np.add.at(face_savings, split_group, faces - 2)
    return group, split_pages, split_group, split_diameter, face_savings


def replay(profile, catalogue, route, guard, errors, widths):
    boxes, faces, diameter, rectangles = profile
    group, split_pages, split_group, split_diameter, face_savings = catalogue
    group_count = int(group.max()) + 1
    path = camera_path(route, np.zeros(3), np.eye(3))[:, (0, 2)]
    focal, depth, displacement = panoramic_projection(route)
    began = perf_counter()
    split_distance = np.maximum(0, distance_from_segment(split_pages, *path) - guard)
    split_take = {
        pixels: (split_distance > 0)
        & (split_diameter <= split_distance * pixels / (focal * depth**2))
        for pixels in widths
    }
    split_faces = {
        pixels: int(face_savings[take].sum()) for pixels, take in split_take.items()
    }
    catalogue_query_ms = (perf_counter() - began) * 1000
    # Ignoring elevation only reduces the lower distance bound. The source itself
    # models flat extrusions and cannot certify the native roof or terrain seat.
    distance = np.maximum(0, distance_from_segment(boxes, *path) - guard)

    def summarize(take, savings):
        unresolved = np.bincount(group[~take], minlength=group_count) > 0
        return {
            "eligible_sources": int(take.sum()),
            "mixed_page_input_faces_saved": int(savings[take].sum()),
            "whole_pages_replaced": int((~unresolved).sum()),
            "whole_page_original_faces_replaced": int(faces[~unresolved[group]].sum()),
            "whole_page_input_faces_saved": int(savings[~unresolved[group]].sum()),
        }

    variants = {}
    for pixels in errors:
        limit = distance * (pixels / (displacement * depth + pixels * (depth + 1)))
        take = (distance > 0) & (rectangles <= limit)
        variants[f"rectangle_error_{pixels:g}px"] = summarize(take, faces - 10)
    for pixels in widths:
        # This says how small the source is, not how well a quad reproduces it.
        # Both source points are clipped to the frustum, so the perspective
        # transform magnifies their difference by at most focal * depth² / distance.
        factor = pixels / (focal * depth * depth)
        limit = distance * factor
        take = (distance > 0) & (diameter <= limit)
        take_from_catalogue = split_take[pixels][split_group]
        assert not np.any(take_from_catalogue & ~take)
        variants[f"quad_support_{pixels:g}px"] = summarize(take, faces - 2) | {
            "split_pages_replaced": int(split_take[pixels].sum()),
            "split_page_input_faces_saved": split_faces[pixels],
        }
    return {
        "pages": group_count,
        "size_split_pages": len(split_pages),
        "catalogue_query_ms": catalogue_query_ms,
        "focal_px": focal,
        "variants": variants,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("benchmarks", nargs="+", type=Path)
    parser.add_argument("--page-m", type=float, default=240)
    parser.add_argument("--guard-m", type=float, default=64)
    parser.add_argument("--rectangle-px", nargs="+", type=float, default=(1, 2, 4))
    parser.add_argument("--quad-px", nargs="+", type=float, default=(2, 4, 8, 16))
    args = parser.parse_args()
    limits = (args.page_m, args.guard_m, *args.rectangle_px, *args.quad_px)
    if any(not math.isfinite(limit) or limit <= 0 for limit in limits):
        parser.error("page, guard and pixel limits must be finite and positive")
    parts = tuple(read_source(args.source))
    if not parts:
        parser.error("source needs at least one footprint")
    started = perf_counter()
    profile = proxy_profile(parts)
    catalogue = build_catalogue(profile, args.page_m)
    profile_ms = (perf_counter() - started) * 1000
    for benchmark in args.benchmarks:
        route = json.loads(benchmark.read_text())
        started = perf_counter()
        result = replay(
            profile, catalogue, route, args.guard_m, args.rectangle_px, args.quad_px
        )
        print(
            json.dumps(
                {
                    "benchmark": str(benchmark),
                    "sources": len(parts),
                    "original_faces": int(profile[1].sum()),
                    "rectangle_candidates": int(np.isfinite(profile[3]).sum()),
                    "page_m": args.page_m,
                    "guard_m": args.guard_m,
                    "profile_ms": profile_ms,
                    "replay_ms": (perf_counter() - started) * 1000,
                }
                | result
            )
        )


if __name__ == "__main__":
    main()

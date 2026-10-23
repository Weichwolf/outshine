"""Experimental coarse source statistics, compiled without generating child cells."""

import math
from dataclasses import dataclass

import numpy as np
from urban_recipe import (
    DEFAULT_FOOTPRINT,
    DEFAULT_PROJECTED_PITCH_M,
    HEIGHT_CUMULATIVE,
    HEIGHTS_M,
    VACANCY_PERCENT,
)


@dataclass
class UrbanParent:
    centre_fractions: np.ndarray
    frame: np.ndarray
    heights: np.ndarray
    probabilities: np.ndarray

    def arrays(self):
        return dict(vars(self))

    def centre_fraction(self, mode="midpoint"):
        if mode == "midpoint":
            return self.centre_fractions[..., 1:].mean(axis=-1)
        modes = {"raw": 0, "circle": 1, "diagonal": 2}
        if mode not in modes:
            raise ValueError("explicit source-area approximation required")
        return self.centre_fractions[..., modes[mode]]

    def surface_measures(self, mode="midpoint"):
        """Unlit surface area per plan area: ±X, ±Y, roof, floor; no visibility fit."""
        scale, pitch, fill, vacancy = self.frame[[2, 4, 5, 6]]
        centres = self.centre_fraction(mode) * (1 - vacancy)
        roof = centres * fill**2
        wall = centres * fill * (self.heights @ self.probabilities) / (pitch * scale)
        return np.stack([wall, wall, wall, wall, roof, roof], axis=-1)

    def height_survival(self, height):
        height = np.asarray(height, float)
        return ((self.heights >= height[..., None]) * self.probabilities).sum(axis=-1)


def compile_parent(
    document,
    *,
    divisions=16,
    projected_pitch_m=DEFAULT_PROJECTED_PITCH_M,
    footprint=DEFAULT_FOOTPRINT,
    area_method="direct",
):
    """Continuous area estimates; fixed lattice/hash and GEOS error stay unqualified."""
    import shapely
    from coarse_city_domains import domains
    from urban_parent_areas import region_areas

    if type(divisions) is not int or not 1 <= divisions <= 64:
        raise ValueError("bounded parent subdivision required")
    if not 8 <= projected_pitch_m <= 512 or not 0 < footprint < 1:
        raise ValueError("bounded child recipe parameters required")
    if area_method not in ("direct", "hierarchical"):
        raise ValueError("direct or hierarchical source-area method required")
    data = domains(document)
    left, bottom, right, top = data["tile_bounds"]
    x = np.linspace(left, right, divisions + 1)
    y = np.linspace(bottom, top, divisions + 1)
    half = projected_pitch_m * footprint / 2
    fractions = []
    geometry_vertices = []
    area_work = []
    for radius in [0, half, half * math.sqrt(2)]:
        geometry = data["allowed"]
        if radius:
            geometry = shapely.buffer(geometry, -radius, quad_segs=32)
        areas, work = region_areas(geometry, x, y, area_method)
        fractions.append(areas)
        area_work.append(work)
        geometry_vertices.append(int(shapely.get_num_coordinates(geometry)))
    fractions = np.stack(fractions, axis=-1) / (
        (right - left) * (top - bottom) / divisions**2
    )
    product = UrbanParent(
        np.clip(fractions, 0, 1),
        np.array(
            [
                *data["origin_scale"],
                right - left,
                projected_pitch_m,
                footprint,
                VACANCY_PERCENT / 100,
            ]
        ),
        np.array(HEIGHTS_M[1:], float),
        np.diff([0, *HEIGHT_CUMULATIVE, 1]),
    )
    metadata = {
        "recipe_revision": "urban-continuous-statistics-probe-v1",
        "source": document["source"],
        "supporting_sources": [
            parent["source"] for parent in document.get("parents", [])
        ],
        "source_records": data["records"],
        "source_geometry_vertices_after_offsets": geometry_vertices,
        "parent_regions": divisions**2,
        "area_cells": divisions**2 * 3,
        "area_intersections": sum(w["intersection_tests"] for w in area_work),
        "area_method": area_method,
        "area_work": area_work,
        "child_cells_generated": 0,
        "normals": [
            [-1, 0, 0],
            [1, 0, 0],
            [0, -1, 0],
            [0, 1, 0],
            [0, 0, 1],
            [0, 0, -1],
        ],
        "geometry_model": "Continuous expected centre area times child recipe. Circle/diagonal erosions bracket the square-kernel ideal mathematically, but GEOS approximation and fixed lattice/hash sampling are not certified bounds. Their midpoint is an unqualified candidate, never a place-fitted factor.",
        "height_model": "Procedural height prior, not observed original heights or a certified original maximum/occluder.",
        "appearance_model": "Surface-normal measures and height probabilities only; no baked lighting. Visibility, masking, cast shadows and projected filtering need a consumer and image qualification.",
        "status": "inferred_statistics_not_world_empty",
    }
    return product, metadata


def main():
    import argparse
    import hashlib
    import json
    import resource
    import subprocess
    import sys
    import tempfile
    import time
    from pathlib import Path

    from coarse_city_domains import load_snapshot

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--divisions", type=int, choices=range(1, 65), default=16)
    parser.add_argument(
        "--area-method", choices=("direct", "hierarchical"), default="direct"
    )
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    document = load_snapshot(args.input)
    input_seconds = time.perf_counter() - started
    started = time.perf_counter()
    parent, metadata = compile_parent(
        document, divisions=args.divisions, area_method=args.area_method
    )
    elapsed = time.perf_counter() - started
    metadata.update(
        head=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        input_snapshot_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
        divisions=args.divisions,
        input_load_s=input_seconds,
        compile_s=elapsed,
        parent_array_B=sum(a.nbytes for a in parent.arrays().values()),
        process_peak_RSS_B=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        memory_scope="Fresh-process peak includes inputs, imports, GEOS/native allocations and result; not allocator traffic, isolated phase peak or app/device budget.",
    )
    np.savez_compressed(args.output / "parent.npz", **parent.arrays())
    (args.output / "parent.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: metadata[k]
                for k in (
                    "head",
                    "divisions",
                    "area_method",
                    "compile_s",
                    "parent_array_B",
                    "process_peak_RSS_B",
                )
            }
        )
    )


if __name__ == "__main__":
    main()

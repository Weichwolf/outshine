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
):
    """Continuous area estimates; fixed lattice/hash and GEOS error stay unqualified."""
    import shapely
    from coarse_city_domains import domains

    if type(divisions) is not int or not 1 <= divisions <= 64:
        raise ValueError("bounded parent subdivision required")
    if not 8 <= projected_pitch_m <= 512 or not 0 < footprint < 1:
        raise ValueError("bounded child recipe parameters required")
    data = domains(document)
    left, bottom, right, top = data["tile_bounds"]
    x = np.linspace(left, right, divisions + 1)
    y = np.linspace(bottom, top, divisions + 1)
    lo_x, lo_y = np.meshgrid(x[:-1], y[:-1])
    hi_x, hi_y = np.meshgrid(x[1:], y[1:])
    regions = shapely.box(lo_x, lo_y, hi_x, hi_y)
    half = projected_pitch_m * footprint / 2
    fractions = []
    geometry_vertices = []
    for radius in [0, half, half * math.sqrt(2)]:
        geometry = data["allowed"]
        if radius:
            geometry = shapely.buffer(geometry, -radius, quad_segs=32)
        fractions.append(shapely.area(shapely.intersection(regions, geometry)))
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
        "area_intersections": divisions**2 * 3,
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

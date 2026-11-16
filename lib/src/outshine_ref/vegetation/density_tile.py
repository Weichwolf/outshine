"""Offline woody-density and canopy inputs on the existing world tile grid."""

from dataclasses import dataclass

import numpy as np
import shapely


@dataclass(frozen=True)
class DensityTile:
    bounds: tuple
    density: np.ndarray
    species: np.ndarray
    height_m: np.ndarray
    forest_fraction: np.ndarray
    slope_deg: np.ndarray
    available: object


def spruce_density(wood, blocked, ground_nodes, bounds, latitude, *,
                   per_m2, height_m, alpine_limit, slope_limit_deg):
    """Use mapped forest, predecessor reservations, DEM slope and latitude.

    Ground nodes are metres ASL, rows south to north. The existing latitude/
    altitude and slope ramps are supplied from world data. This first spruce
    recipe deliberately adds no inferred forest outside the mapped polygons.
    """
    heights = np.asarray(ground_nodes, dtype=np.float64)
    if heights.ndim != 2 or min(heights.shape) < 2 or not np.isfinite(heights).all():
        raise ValueError("finite terrain node grid required")
    rows, columns = np.array(heights.shape) - 1
    x0, y0, x1, y1 = bounds
    dx, dy = (x1 - x0) / columns, (y1 - y0) / rows
    xx, yy = np.meshgrid(x0 + np.arange(columns) * dx, y0 + np.arange(rows) * dy)
    available = shapely.make_valid(wood).difference(shapely.make_valid(blocked))
    cells = shapely.box(xx, yy, xx + dx, yy + dy)
    fraction = np.clip(shapely.area(shapely.intersection(available, cells)) / (dx * dy), 0, 1)
    gx = ((heights[:-1, 1:] + heights[1:, 1:])
          - (heights[:-1, :-1] + heights[1:, :-1])) / (2 * dx)
    gy = ((heights[1:, :-1] + heights[1:, 1:])
          - (heights[:-1, :-1] + heights[:-1, 1:])) / (2 * dy)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    altitude = (heights[:-1, :-1] + heights[:-1, 1:]
                + heights[1:, :-1] + heights[1:, 1:]) / 4
    lower = max(0, alpine_limit['treelineBaseM'] + alpine_limit['treelinePerDegM']
                * (abs(latitude) - alpine_limit['treelineBaseLatDeg']))
    altitude_factor = np.clip(1 - (altitude - lower) / alpine_limit['treelineBandM'], 0, 1)
    slope_factor = np.clip(1 - (slope - slope_limit_deg) / alpine_limit['slopeBandDeg'], 0, 1)
    density = (per_m2 * fraction * altitude_factor * slope_factor).astype(np.float32)
    present = density > 0
    return DensityTile(tuple(bounds), density, present.astype(np.uint8),
                       np.where(present, height_m, 0).astype(np.float32),
                       fraction.astype(np.float32), slope.astype(np.float32), available)


def pack_density(tile, *, density_max, species_height_m):
    """R32_UINT: low16 density UNORM; next8 species; high8 relative height.

    Species 0 is empty, 1 is spruce. Density scale and species maximum heights
    belong to the recipe metadata. No density clipping is permitted.
    """
    if density_max <= 0 or np.any(tile.density > density_max):
        raise ValueError("density range must contain the complete recipe")
    density = np.rint(tile.density / density_max * 65535).astype(np.uint32)
    height = np.rint(tile.height_m / species_height_m * 255).astype(np.uint32)
    if np.any(height > 255):
        raise ValueError("species height range is too small")
    return density | (tile.species.astype(np.uint32) << 16) | (height << 24)


def canopy_inputs(tile, ground_nodes, *, crown_width_m, leaf_albedo_linear):
    """Terrain height offset, coverage and linear albedo; material bake inputs.

    Crowns are clipped to available forest, with no spill into reservations.
    Density is averaged over the complete cell; coverage is the available area
    fraction times occupancy inside that area. Height nodes obey the same exact
    footprint. Subcell reservations must still be retained by the native bake;
    interpolation of this eight-metre reference alone cannot resolve thin paths.
    """
    fraction = tile.forest_fraction.astype(np.float64)
    conditional_density = np.zeros_like(fraction)
    np.divide(tile.density, fraction, out=conditional_density, where=fraction > 0)
    coverage = fraction * -np.expm1(-conditional_density * np.pi * (crown_width_m / 2) ** 2)
    linear = np.asarray(leaf_albedo_linear, dtype=np.float64)
    if linear.shape != (3,) or not np.isfinite(linear).all() or np.any((linear < 0) | (linear > 1)):
        raise ValueError('energy-bounded linear leaf reflection required')
    offset = np.zeros_like(ground_nodes, dtype=np.float64)
    for dy, dx in ((0, 0), (1, 0), (0, 1), (1, 1)):
        part = offset[dy:dy + tile.density.shape[0], dx:dx + tile.density.shape[1]]
        np.maximum(part, tile.height_m, out=part)
    x0, y0, x1, y1 = tile.bounds
    xx, yy = np.meshgrid(np.linspace(x0, x1, offset.shape[1]),
                         np.linspace(y0, y1, offset.shape[0]))
    offset[~shapely.intersects_xy(tile.available, xx, yy)] = 0
    return {'height_offset_m': offset.astype(np.float32),
            'coverage': coverage.astype(np.float32),
            'leaf_albedo_linear': np.broadcast_to(linear, (*tile.density.shape, 3)).astype(np.float32)}

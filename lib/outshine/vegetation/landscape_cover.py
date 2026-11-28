"""Filtered vegetation reflectance for terrain beyond individually drawn plants."""

from dataclasses import dataclass

import numpy as np
import shapely
from scipy.ndimage import distance_transform_edt, gaussian_filter

from outshine.materials.palette import SURFACES
from outshine.terrain.granodiorite import BEDROCK, OUTCROPS, RESIDUUM, ROCK_MATERIAL
from outshine.terrain.ground_surface import at_grade_routes
from outshine.vegetation.atlas import CELLS_PER_DEGREE
from outshine.vegetation.patagonian_shrub import PATAGONIAN_COMMUNITIES
from outshine.vegetation.plant_sites import patch_density

LANDSCAPE_REACH_M = 2400
LANDSCAPE_MATERIALS = frozenset(
    (
        "patagonian gravel soil",
        "mojave gravelly sand",
        "regional soil",
        "ground",
        "grass",
        "meadow",
        "scrub",
    )
)


@dataclass(frozen=True)
class LandscapeCover:
    """Premultiplied linear reflectance and blend weight on a geographic grid."""

    pixels: np.ndarray
    transform: tuple[float, float, float, float]


def raster_window(cover, field="potential_community"):
    source = cover.source
    origin = np.array([source.lon, source.lat])
    radius = LANDSCAPE_REACH_M / source.metres
    bounds = np.r_[origin - radius, origin + radius]
    tiles = list(
        cover.atlas.community_windows(bounds)
        if field == "potential_community"
        else cover.atlas.evidence_windows(bounds, field)
    )
    if not tiles:
        return None
    tiles = np.asarray(tiles)
    lower = np.maximum(bounds[:2], tiles[:, :2].min(axis=0))
    upper = np.minimum(bounds[2:], tiles[:, 2:].max(axis=0))
    first = np.floor(lower * CELLS_PER_DEGREE).astype(int)
    last = np.ceil(upper * CELLS_PER_DEGREE).astype(int)
    pitch = source.metres / CELLS_PER_DEGREE
    corner = (first / CELLS_PER_DEGREE - origin) * source.metres
    shape = last - first
    gx, gy = np.meshgrid(
        corner[0] + (np.arange(shape[0]) + 0.5) * pitch[0],
        corner[1] + (np.arange(shape[1]) + 0.5) * pitch[1],
    )
    return np.stack((gx, gy), axis=-1), pitch, (*corner, *(1 / (shape * pitch)))


def smooth_ramp(distance, start, end):
    t = np.clip((distance - start) / (end - start), 0, 1)
    return t * t * (3 - 2 * t)


def steppe_albedo(source, xy, land, seed):
    grass_patch = patch_density(source, xy, seed, scale=6)
    woody_patch = patch_density(source, xy, seed ^ 0x958B, scale=3.4)
    clumps = patch_density(source, xy, seed ^ 0x6BA7, scale=1.7)
    sparse = land == 60
    shrubland = land == 20
    # Class membership constrains the recipe. The sub-cell fractions are inferred.
    grass = np.where(
        sparse,
        0.065 * grass_patch**1.7,
        np.where(shrubland, 0.13, 0.51) + 0.39 * grass_patch**1.3,
    )
    shrub = np.where(
        sparse,
        0.034 * woody_patch**1.8,
        np.where(shrubland, 0.34, 0.053) + 0.39 * woody_patch**1.8,
    )
    grass *= 1 - shrub
    soil = SURFACES["patagonian gravel soil"][0]
    straw = np.array([0.155, 0.131, 0.063])
    green = np.array([0.048, 0.086, 0.029])
    leaves = np.array([0.037, 0.067, 0.026])
    grass_colour = straw + (green - straw) * (0.45 + 0.48 * grass_patch[:, None])
    colour = (
        (1 - grass - shrub)[:, None] * soil
        + grass[:, None] * grass_colour
        + shrub[:, None] * leaves
    )
    texture = 0.82 + 0.14 * woody_patch + 0.24 * clumps
    return colour * texture[:, None]


def reserved_routes(source, xy, pitch, bounds):
    transportation = [
        (geometry, tags)
        for name, geometry, tags in source.features
        if name == "transportation"
    ]
    routes = at_grade_routes(transportation, shapely.box(*bounds))
    occupied = shapely.union_all(list(routes.values()))
    if occupied.is_empty:
        return np.zeros(len(xy), dtype=bool)
    padding = np.linalg.norm(pitch) / 2 + 1
    return shapely.intersects_xy(occupied.buffer(padding), xy[:, 0], xy[:, 1])


def landscape_cover(cover, eye, seed):
    if cover is None:
        return None
    if getattr(cover.atlas, "geology_indices", None):
        geology = geology_landscape(cover, seed)
        if geology is not None:
            return geology
    if not PATAGONIAN_COMMUNITIES.intersection(cover.atlas.community_indices):
        return None
    window = raster_window(cover)
    if window is None:
        return None
    points, pitch, transform = window
    xy = points.reshape(-1, 2)
    land, _ = cover.land_cells(xy)
    allowed = cover.steppe(xy, land)
    if not allowed.any():
        return None
    distance = np.linalg.norm(xy - eye[:2], axis=1)
    bounds = (*(points[0, 0] - pitch / 2), *(points[-1, -1] + pitch / 2))
    allowed &= ~reserved_routes(cover.source, xy, pitch, bounds)
    shape = points.shape[:2]
    border = distance_transform_edt(
        np.pad(allowed.reshape(shape), 1), sampling=pitch[::-1]
    )[1:-1, 1:-1]
    weight = smooth_ramp(distance, 52, 145) * smooth_ramp(border.ravel(), 7, 28)
    weight = weight.reshape(shape)
    weight[[0, -1], :] = 0
    weight[:, [0, -1]] = 0
    picked = weight.ravel() > 0
    if not picked.any():
        return None
    pixels = np.zeros((*shape, 4), dtype=np.uint8)
    pixels[..., 3] = np.rint(255 * weight)
    colour = steppe_albedo(cover.source, xy[picked], land[picked], seed)
    pixels[..., :3].reshape(-1, 3)[picked] = np.rint(
        colour * (255 * weight.ravel()[picked, None])
    )
    return LandscapeCover(pixels, transform)


def geology_landscape(cover, seed):
    window = raster_window(cover, "geology")
    if window is None:
        return None
    points, pitch, transform = window
    xy = points.reshape(-1, 2)
    lon, lat = cover.coordinates(xy)
    units = cover.atlas.geology_cells(lat, lon)
    bedrock = cover.atlas.geology_mask(units, BEDROCK)
    weathered = cover.atlas.geology_mask(units, RESIDUUM)
    observations, _ = cover.atlas.observed_cells(lat, lon)
    bare = cover.atlas.observation_mask(observations, OUTCROPS)
    weight = np.select((bedrock & bare, bedrock, weathered), (0.67, 0.21, 0.12))
    bounds = (*(points[0, 0] - pitch / 2), *(points[-1, -1] + pitch / 2))
    weight[reserved_routes(cover.source, xy, pitch, bounds)] = 0
    shade = patch_density(cover.source, xy, seed ^ 0x9584, scale=27)
    base = np.where(
        bedrock[:, None],
        SURFACES[ROCK_MATERIAL][0],
        SURFACES["mojave gravelly sand"][0],
    )
    grey = base * (0.82 + 0.27 * shade[:, None])
    fields = np.column_stack((grey * weight[:, None], weight)).reshape(
        *points.shape[:2], 4
    )
    filtered = gaussian_filter(fields, (0.92, 0.92, 0), mode="constant")
    filtered[[0, -1], :, :] = 0
    filtered[:, [0, -1], :] = 0
    return LandscapeCover(np.rint(255 * filtered).astype(np.uint8), transform)

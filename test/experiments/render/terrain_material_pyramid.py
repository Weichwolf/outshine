"""Camera-independent joint-material cells; exact area query, approximate cell NDFs."""

from dataclasses import dataclass

import numpy as np
import shapely
from terrain_material_filter import joint_models
from terrain_material_grid import frozen
from terrain_material_plane import checked_at


@dataclass(frozen=True, slots=True)
class MaterialLevel:
    fields: np.ndarray
    weights: np.ndarray


@dataclass(frozen=True, slots=True)
class MaterialPyramid:
    origin_m: tuple[float, float]
    extent_m: float
    levels: tuple[MaterialLevel, ...]

    @property
    def retained_bytes(self):
        return sum(level.fields.nbytes + level.weights.nbytes for level in self.levels)


def bake(evaluate, origin_m, extent_m, side, *, capacity=4, dtype="float32"):
    origin = np.asarray(origin_m, dtype=float)
    storage = np.dtype(dtype)
    if (
        origin.shape != (2,)
        or not np.isfinite(origin).all()
        or not np.isfinite(extent_m)
        or extent_m <= 0
        or type(side) is not int
        or not 8 <= side <= 128
        or side & (side - 1)
        or type(capacity) is not int
        or capacity not in (1, 2, 4, 8)
        or storage not in (np.dtype("float16"), np.dtype("float32"))
    ):
        raise ValueError(
            "bounded metric tile, dyadic samples and material capacity required"
        )
    axis = (np.arange(side) + 0.5) * extent_m / side
    x, y = np.meshgrid(axis, axis)
    at = checked_at(origin + np.column_stack((x.ravel(), y.ravel())))
    step = 1e-7
    raw = np.asarray(
        evaluate(
            at, np.tile([step, 0], (len(at), 1)), np.tile([0, step], (len(at), 1))
        ),
        dtype=float,
    ).copy()
    if raw.shape != (len(at), 6) or not np.isfinite(raw).all():
        raise ValueError("six finite point-material components required")
    raw[:, :3] = np.clip(raw[:, :3], 0, 1)
    raw[:, 3] = np.clip(raw[:, 3], 0.04, 1)
    raw[:, 4:] /= step
    if np.any(np.abs(raw) > np.finfo(storage).max):
        raise ValueError("point material exceeds chosen storage range")
    points = raw.reshape(side, side, 6)
    levels = [
        MaterialLevel(
            frozen(points[:, :, None].astype(storage)),
            frozen(np.ones((side, side, 1), dtype=storage)),
        )
    ]
    width = side // 2
    while width:
        stride = side // width
        fields = np.zeros((width, width, capacity, 6), dtype=storage)
        weights = np.zeros((width, width, capacity), dtype=storage)
        for y in range(width):
            for x in range(width):
                samples = points[
                    y * stride : (y + 1) * stride, x * stride : (x + 1) * stride
                ].reshape(-1, 6)
                model, area = joint_models(
                    samples,
                    np.ones(len(samples)) / len(samples),
                    capacities=(capacity,),
                )[f"joint_{capacity}_toksvig"]
                fields[y, x, : len(model)] = model
                weights[y, x, : len(area)] = area
        levels.append(MaterialLevel(frozen(fields), frozen(weights)))
        width //= 2
    return MaterialPyramid(tuple(origin), float(extent_m), tuple(levels))


def filtered_components(pyramid, at_m, dx_m, dy_m, lod):
    """Filter distributions/responses, never interpolate representative normals."""
    frame = np.asarray((at_m, dx_m, dy_m), dtype=float)
    if (
        frame.shape != (3, 2)
        or not np.isfinite(frame).all()
        or not np.isfinite(lod)
        or not 0 <= lod <= len(pyramid.levels) - 1
        or abs(np.linalg.det(frame[1:])) <= 1e-30
    ):
        raise ValueError("finite nonsingular footprint and resident LOD required")
    centre = frame[0] - pyramid.origin_m
    dx, dy = frame[1:]
    polygon = shapely.Polygon(
        [
            centre - (dx + dy) / 2,
            centre + (dx - dy) / 2,
            centre + (dx + dy) / 2,
            centre + (dy - dx) / 2,
        ]
    )
    bounds = np.array(polygon.bounds).reshape(2, 2)
    if np.any(bounds[0] < 0) or np.any(bounds[1] > pyramid.extent_m):
        raise ValueError("footprint exceeds resident tile; no clamp or hidden fallback")
    first, last = int(np.floor(lod)), int(np.ceil(lod))
    mixed, coverage, selected_cells = [], [], 0
    for index in range(first, last + 1):
        amount = 1 if first == last else (last - lod if index == first else lod - first)
        level = pyramid.levels[index]
        width = len(level.fields)
        cell_m = pyramid.extent_m / width
        low = np.floor(bounds[0] / cell_m).astype(int)
        high = np.minimum(np.ceil(bounds[1] / cell_m).astype(int), width)
        x, y = np.meshgrid(np.arange(low[0], high[0]), np.arange(low[1], high[1]))
        x, y = x.ravel(), y.ravel()
        cells = shapely.box(x * cell_m, y * cell_m, (x + 1) * cell_m, (y + 1) * cell_m)
        areas = shapely.area(shapely.intersection(polygon, cells)) / polygon.area
        if not np.isclose(areas.sum(), 1, rtol=1e-9, atol=1e-12):
            raise ValueError("incomplete spatial coverage")
        active = areas > 0
        x, y, areas = x[active], y[active], areas[active]
        selected_cells += len(areas)
        mixed.append(level.fields[y, x].reshape(-1, 6))
        conditional = level.weights[y, x].astype(float)
        conditional /= conditional.sum(axis=-1, keepdims=True)
        coverage.append((conditional * (areas * amount)[:, None]).ravel())
    fields, weights = (
        np.concatenate(mixed).astype(float),
        np.concatenate(coverage).astype(float),
    )
    active = weights > 0
    weights = weights[active]
    weights /= weights.sum()
    return fields[active], weights, selected_cells


def sample(pyramid, at_m, dx_m, dy_m, lod, uniform):
    """One material sample per query, matching the stored distribution filter.

    Four independent uniform dimensions choose mip, footprint position and atom.
    No area clipping, recipe evaluation or multi-lobe shading on this path.
    """
    at = checked_at(at_m)
    dx, dy = np.asarray(dx_m, dtype=float), np.asarray(dy_m, dtype=float)
    lod, uniform = np.asarray(lod, dtype=float), np.asarray(uniform, dtype=float)
    if (
        dx.shape != at.shape
        or dy.shape != at.shape
        or lod.shape != (len(at),)
        or uniform.shape != (len(at), 4)
        or not np.isfinite(dx).all()
        or not np.isfinite(dy).all()
        or not np.isfinite(lod).all()
        or np.any(lod < 0)
        or np.any(lod > len(pyramid.levels) - 1)
        or not np.isfinite(uniform).all()
        or np.any(uniform < 0)
        or np.any(uniform >= 1)
        or np.any(np.abs(dx[:, 0] * dy[:, 1] - dx[:, 1] * dy[:, 0]) <= 1e-30)
    ):
        raise ValueError(
            "bounded footprints, resident LODs and four uniform dimensions required"
        )
    centre = at - pyramid.origin_m
    half = (np.abs(dx) + np.abs(dy)) / 2
    if np.any(centre - half < 0) or np.any(centre + half > pyramid.extent_m):
        raise ValueError("footprint exceeds resident tile")
    base = np.floor(lod).astype(int)
    level_indices = base + (uniform[:, 0] < lod - base)
    local = centre + (uniform[:, 1:2] - 0.5) * dx + (uniform[:, 2:3] - 0.5) * dy
    result = np.empty((len(at), 6), dtype=float)
    for index in np.unique(level_indices):
        selected = level_indices == index
        level = pyramid.levels[index]
        width = len(level.fields)
        xy = np.minimum(
            np.floor(local[selected] * width / pyramid.extent_m).astype(int), width - 1
        )
        weights = level.weights[xy[:, 1], xy[:, 0]].astype(float)
        cumulative = np.cumsum(weights, axis=-1)
        atom = np.sum(
            uniform[selected, 3:4] >= cumulative / cumulative[:, -1:], axis=-1
        )
        result[selected] = level.fields[xy[:, 1], xy[:, 0], atom]
    return result

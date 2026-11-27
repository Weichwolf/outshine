"""Fit mapped flights and flat landings before cutting them into terrain."""

from collections import deque
from dataclasses import dataclass

import numpy as np
import shapely
from ground_surface import at_grade_width


@dataclass(frozen=True)
class Stairway:
    treads: np.ndarray
    heights: np.ndarray
    footprint: shapely.Geometry


def fit_riser_counts(heights, capacities, count):
    if capacities.sum() < count:
        return None
    levels = np.linspace(heights[0], heights[-1], count + 1)
    cost = np.full(count + 1, np.inf)
    cost[0] = 0
    parents = np.empty((len(capacities), count + 1), dtype=int)
    for i, (capacity, target) in enumerate(zip(capacities, heights[1:], strict=True)):
        previous = cost
        cost = (levels - target) ** 2
        window = deque()
        for j in range(count + 1):
            while window and window[0] < j - capacity:
                window.popleft()
            while window and previous[window[-1]] >= previous[j]:
                window.pop()
            window.append(j)
            parents[i, j] = window[0]
            cost[j] += previous[window[0]]
    state = np.full(len(heights), count)
    for i in reversed(range(len(capacities))):
        state[i] = parents[i, state[i + 1]]
    return state, levels


def flight_sections(points, lengths, width, heights):
    along = np.diff(points, axis=0) / lengths[:, None]
    side = along[:, [1, 0]] * [1, -1] * (0.5 * width)
    corner_length = 1 + (along[:-1] * along[1:]).sum(axis=1)
    if np.any(corner_length < 0.25):
        return None
    miter = np.vstack(
        (side[0], (side[:-1] + side[1:]) / corner_length[:, None], side[-1])
    )
    start = abs((miter[:-1] * along).sum(axis=1))
    end = lengths - abs((miter[1:] * along).sum(axis=1))
    rise = abs(heights[-1] - heights[0])
    if rise < 0.10:
        return None
    count = max(round(rise / 0.16), int(np.ceil(rise / 0.20)))
    depth = np.clip(0.63 - 2 * rise / count, 0.27, 0.40)
    capacities = (np.maximum(0, end - start) / depth).astype(int)
    fit = fit_riser_counts(heights, capacities, count)
    if fit is None:
        return None
    state, levels = fit
    corners, surfaces = [], []
    for i, risers in enumerate(np.diff(state)):
        offset = 0.5 * (start[i] + end[i] - risers * depth)
        stations = offset + depth * (np.arange(risers) + 0.5)
        centres = points[i] + stations[:, None] * along[i]
        left = np.vstack(
            (points[i] + miter[i], centres + side[i], points[i + 1] + miter[i + 1])
        )
        right = np.vstack(
            (points[i] - miter[i], centres - side[i], points[i + 1] - miter[i + 1])
        )
        corners.extend(np.stack((left[:-1], right[:-1], right[1:], left[1:]), axis=1))
        surfaces.extend(levels[state[i] : state[i + 1] + 1])
    return np.asarray(corners), np.asarray(surfaces)


def fit_stairway(line, width, height):
    points = np.asarray(line.coords)
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    if not len(lengths) or np.any(lengths <= 1e-6):
        return None
    result = flight_sections(points, lengths, width, height(points))
    if result is None:
        return None
    corners, heights = result
    treads = shapely.polygons(corners)
    if not np.all(shapely.is_valid(treads)):
        return None
    treads = shapely.set_precision(treads, 1e-6)
    outline = shapely.union_all(treads)
    if (
        outline.geom_type != "Polygon"
        or np.any(shapely.is_empty(treads))
        or outline.area < shapely.area(treads).sum() - 1e-6
    ):
        return None
    return Stairway(treads, heights, outline)


def local_stairways(features, domain, water, height):
    flights = [
        (line, at_grade_width(tags))
        for name, geom, tags in features
        if name == "transportation" and tags.get("subclass") == "steps"
        for line in shapely.get_parts(geom)
        if line.geom_type == "LineString" and domain.covers(line)
    ]
    accepted = []
    for line, width in sorted(flights, key=lambda row: -row[0].length):
        if width is None:
            continue
        stair = fit_stairway(line, width, height)
        if (
            stair is not None
            and domain.covers(stair.footprint)
            and not water.intersects(stair.footprint)
            and not any(
                other.footprint.intersects(stair.footprint) for other in accepted
            )
        ):
            accepted.append(stair)
    return accepted

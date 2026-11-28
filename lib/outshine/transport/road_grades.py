"""Fit carriageways and shoulders on a single continuous terrain lattice."""

from collections import defaultdict
from dataclasses import dataclass
from math import ceil

import numpy as np
import shapely
from scipy.ndimage import binary_dilation, label

from outshine.terrain.ground_surface import (
    ROAD_WIDTH,
    at_grade_width,
    route_area,
    terrain_triangles,
)

from .heights import HeightBudget, _face_gradients, _gradient_weights
from .route_tiles import geography
from .weighted_heights._solve import solve

GRADE_LIMITS = {
    "motorway": 0.07,
    "trunk": 0.085,
    "primary": 0.10,
    "secondary": 0.12,
    "tertiary": 0.14,
    "minor": 0.16,
    "service": 0.17,
    "track": 0.22,
    "busway": 0.10,
}
SHOULDER_GRADE = 0.55
GRADE_BUDGET = HeightBudget()


@dataclass(frozen=True, slots=True)
class RoadGradeFit:
    vertices: np.ndarray
    faces: np.ndarray
    limits: np.ndarray
    unresolved_faces: int


def ground_carriageways(paths, project, extent, features=()):
    areas = defaultdict(list)
    bounds = {
        width: extent.buffer(width / 2, quad_segs=1)
        for width in set(ROAD_WIDTH.values())
    }
    for name, geometry, tags in features:
        grade = GRADE_LIMITS.get(tags.get("class"))
        if name == "transportation" and grade is not None:
            width = at_grade_width(tags)
            if width is not None:
                area = route_area(geometry, width).intersection(extent)
                if not area.is_empty:
                    areas[grade].append(area)
    for path in paths:
        attributes = path.attributes
        limit = GRADE_LIMITS.get(attributes.kind)
        if limit is None or attributes.domain != ("land", 0, "", "", False):
            continue
        width = at_grade_width(dict(attributes.tags))
        if width is None:
            continue
        coords = project(*np.asarray([geography(node.point) for node in path.nodes]).T)
        axis = shapely.LineString(coords)
        clipped = axis.intersection(bounds[width])
        if clipped.geom_type.endswith("LineString") and not clipped.is_empty:
            areas[limit].append(route_area(clipped, width))
    return {limit: shapely.union_all(parts) for limit, parts in areas.items()}


def _base_limits(areas, carriageways):
    index = shapely.STRtree(areas)
    limits = np.full(len(areas), np.inf)
    for grade, pavement in carriageways.items():
        hits = index.query(shapely.get_parts(pavement), predicate="intersects")[1]
        np.minimum.at(limits, hits, grade)
    return limits


def _patches(quads, unsafe, violations, spacing):
    side = int(np.sqrt(len(quads)))
    shape = (side, side)
    safe = (~unsafe).reshape(shape)
    reach = max(2, ceil(2 * max(ROAD_WIDTH.values()) / spacing))
    active = binary_dilation(
        violations.reshape(shape) & safe,
        structure=np.ones((3, 3), dtype=bool),
        iterations=reach,
        mask=safe,
    )
    regions, count = label(active, structure=np.ones((3, 3), dtype=int))
    for i in range(1, count + 1):
        yield np.repeat(regions.ravel() == i, 2)


def _fit_patch(vertices, faces, selected, exterior, prior, limits):
    nodes, local = np.unique(faces[selected], return_inverse=True)
    local = local.reshape(-1, 3)
    fixed = exterior[nodes]
    body = ~fixed[local].any(axis=1)
    caps = np.maximum(prior[selected], SHOULDER_GRADE)
    road = np.isfinite(limits[selected])
    perimeter = road & ~body
    caps[perimeter] = np.maximum(
        prior[selected][perimeter], limits[selected][perimeter]
    )
    certified = body & road
    caps[certified] = limits[selected][certified]
    if not np.any(certified & (prior[selected] > caps + 1e-8)):
        return None
    frames = np.eye(2)[None, :, :] / caps[:, None, None]
    xyz = vertices[nodes]
    coefficients = _gradient_weights(xyz[:, :2], local)
    heights, _ = solve(
        local,
        xyz[:, 2],
        frames,
        coefficients,
        np.flatnonzero(fixed),
        xyz[fixed, 2],
        np.ones(len(nodes)),
        GRADE_BUDGET,
    )
    if heights is None or not np.isfinite(heights).all():
        return None
    heights[fixed] = xyz[fixed, 2]
    slopes = np.linalg.norm(_face_gradients(xyz[:, :2], local, heights), axis=1)
    if np.any(slopes > caps + 1e-7):
        return None
    return nodes[~fixed], heights[~fixed]


def fit_road_grades(vertices, quads, carriageways, protected):
    """Keep the original XY mesh and pin every exterior and reserved contact."""
    faces = terrain_triangles(quads)
    areas = shapely.polygons(vertices[faces, :2])
    limits = _base_limits(areas, carriageways)
    prior = np.linalg.norm(
        _face_gradients(vertices[:, :2], faces, vertices[:, 2]), axis=1
    )
    needs_fit = prior > limits + 1e-8
    result = vertices
    if needs_fit.any():
        shapely.prepare(protected)
        unsafe = shapely.intersects(protected, areas).reshape(-1, 2).any(axis=1)
        xy = vertices[:, :2]
        border = ((xy == xy.min(axis=0)) | (xy == xy.max(axis=0))).any(axis=1)
        spacing = xy[1, 0] - xy[0, 0]
        for selected in _patches(
            quads, unsafe, needs_fit.reshape(-1, 2).any(axis=1), spacing
        ):
            exterior = border.copy()
            exterior[faces[~selected].ravel()] = True
            patch = _fit_patch(vertices, faces, selected, exterior, prior, limits)
            if patch is not None:
                if result is vertices:
                    result = vertices.copy()
                nodes, elevations = patch
                result[nodes, 2] = elevations
    grades = (
        prior
        if result is vertices
        else np.linalg.norm(_face_gradients(result[:, :2], faces, result[:, 2]), axis=1)
    )
    certified = np.isfinite(limits) & (grades <= limits + 1e-7)
    return RoadGradeFit(
        result,
        faces[certified],
        limits[certified],
        int((np.isfinite(limits) & ~certified).sum()),
    )

"""Fit transit and carriageways on the same tile surface."""

import numpy as np
import shapely

from outshine.terrain.ground_surface import terrain_triangles

from .heights import _face_gradients, _gradient_weights
from .rail_hardware import RailRecipe
from .road_grades import GRADE_BUDGET, SHOULDER_GRADE, _patches
from .tram_rails import TRAM_BED_WIDTH_M
from .weighted_heights._solve import solve

TRAM_GRADE_LIMIT = 0.06
TRAM_CROSSLEVEL_M = 0.075
SHARED_ROAD_CROSSFALL_LIMIT = 0.08
GRADE_TOLERANCE = 1e-7


def _corridor_faces(index, lines, widths):
    points, owners = shapely.get_coordinates(lines, return_index=True)
    start = np.flatnonzero(owners[:-1] == owners[1:])
    segments = points[start[:, None] + [0, 1]]
    directions = segments[:, 1] - segments[:, 0]
    length = np.linalg.norm(directions, axis=1)
    valid = length > 0
    directions = directions[valid] / length[valid, None]
    owners = owners[start[valid]]
    bed = shapely.buffer(
        shapely.linestrings(segments[valid]),
        np.broadcast_to(widths, (len(lines),))[owners] / 2,
        quad_segs=2,
    )
    route, face = index.query(bed, predicate="intersects")
    order = np.lexsort((route, face))
    return face[order], directions[route[order]], owners[route[order]]


def _axial_frames(direction, limit):
    frames = np.zeros((len(direction), 2, 2))
    frames[:, 0] = direction / np.broadcast_to(limit, (len(direction),))[:, None]
    return frames


def _tram_faces(index, lines):
    faces, direction, _ = _corridor_faces(index, lines, TRAM_BED_WIDTH_M)
    side = direction[:, [1, 0]] * [-1, 1]
    return np.tile(faces, 2), np.concatenate(
        (
            _axial_frames(direction, TRAM_GRADE_LIMIT),
            _axial_frames(side, TRAM_CROSSLEVEL_M / RailRecipe().gauge_m),
        )
    )


def _road_faces(index, carriageways, gradients):
    if not carriageways:
        return np.empty(0, dtype=int), np.empty((0, 2, 2))
    lines, widths, limits = zip(*carriageways, strict=True)
    faces, direction, source = _corridor_faces(index, lines, widths)
    side = direction[:, [1, 0]] * [-1, 1]
    grade = abs(np.einsum("fi,fi->f", direction, gradients[faces]))
    crossfall = abs(np.einsum("fi,fi->f", side, gradients[faces]))
    return np.tile(faces, 2), np.concatenate(
        (
            _axial_frames(direction, np.maximum(grade, np.asarray(limits)[source])),
            _axial_frames(side, np.maximum(crossfall, SHARED_ROAD_CROSSFALL_LIMIT)),
        )
    )


def _fit_patch(vertices, faces, selected, fixed, caps, roads, trams):
    tram_faces, frames = trams
    rail = selected[tram_faces]
    corridor = faces[tram_faces[rail]]
    if not len(corridor):
        return None
    rail_frames = frames[rail].copy()
    contact = fixed[corridor].any(axis=1)
    gradients = _face_gradients(vertices[:, :2], corridor, vertices[:, 2])
    before = np.linalg.norm(np.einsum("fki,fi->fk", rail_frames, gradients), axis=1)
    if not np.any((before > 1 + GRADE_TOLERANCE) & ~contact):
        return None
    rail_frames[contact] /= np.maximum(1, before[contact])[:, None, None]
    road_faces, road_frames = roads
    road = selected[road_faces]
    nodes, local = np.unique(
        np.vstack((faces[selected], faces[road_faces[road]], corridor)),
        return_inverse=True,
    )
    local = local.reshape(-1, 3)
    limits = np.concatenate(
        (np.eye(2)[None] / caps[selected, None, None], road_frames[road], rail_frames)
    )
    if len(nodes) > GRADE_BUDGET.max_vertices or len(local) > GRADE_BUDGET.max_faces:
        return None
    locked = np.flatnonzero(fixed[nodes])
    xy, prior = vertices[nodes, :2], vertices[nodes, 2]
    coefficients = _gradient_weights(xy, local)
    solve_args = (
        local,
        prior,
        limits,
        coefficients,
        locked,
        prior[locked],
        np.ones(len(nodes)),
        GRADE_BUDGET,
    )
    excess = np.zeros(len(local))
    rail_excess = excess[-len(corridor) :]
    for allow_partial in (False, True):
        if allow_partial:
            rail_excess[~contact] = np.maximum(before[~contact] - 1, 0)
        heights, _ = solve(*solve_args, excess_limits=excess if allow_partial else None)
        if heights is None or not np.isfinite(heights).all():
            continue
        heights[locked] = prior[locked]
        slopes = _face_gradients(xy, local, heights)
        deviation = np.linalg.norm(np.einsum("fki,fi->fk", limits, slopes), axis=1)
        if np.all(deviation <= 1 + excess + GRADE_TOLERANCE):
            return nodes, heights
    return None


def fit_tram_grades(vertices, quads, trams, carriageways, protected):
    """Regrade shared pavement while retaining fixed terrain and structural contacts."""
    if not trams:
        return vertices
    xy, prior = vertices[:, :2], vertices[:, 2]
    faces = terrain_triangles(quads)
    index = shapely.STRtree(shapely.polygons(xy[faces]))
    tram_faces, frames = _tram_faces(index, trams)
    gradients = _face_gradients(xy, faces, prior)
    tram_slopes = np.linalg.norm(
        np.einsum("fki,fi->fk", frames, gradients[tram_faces]), axis=1
    )
    violations = np.zeros(len(quads), dtype=bool)
    violations[tram_faces[tram_slopes > 1 + GRADE_TOLERANCE] // 2] = True
    if not violations.any():
        return vertices
    roads = _road_faces(index, carriageways, gradients)
    caps = np.maximum(np.linalg.norm(gradients, axis=1), SHOULDER_GRADE)
    border = ((xy == xy.min(axis=0)) | (xy == xy.max(axis=0))).any(axis=1)
    result = vertices
    for selected in _patches(vertices, quads, violations, protected):
        fixed = border.copy()
        fixed[faces[~selected].ravel()] = True
        fit = _fit_patch(
            vertices, faces, selected, fixed, caps, roads, (tram_faces, frames)
        )
        if fit is not None:
            if result is vertices:
                result = vertices.copy()
            nodes, heights = fit
            result[nodes, 2] = heights
    return result

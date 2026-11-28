"""Keep the longitudinal grade of the emitted terrain triangles within bounds."""

from dataclasses import dataclass

import numpy as np
import shapely

from .heights import _gradient_weights


@dataclass(frozen=True, slots=True)
class CrownConstraints:
    vertices: np.ndarray
    coefficients: np.ndarray
    lower: np.ndarray
    upper: np.ndarray


def _affected_faces(shape, delta):
    rows, columns = shape
    y, x = np.nonzero(delta.reshape(shape))
    yy = (y[:, None] + [-1, -1, 0, 0]).ravel()
    xx = (x[:, None] + [-1, 0, -1, 0]).ravel()
    inside = (xx >= 0) & (yy >= 0) & (xx < columns - 1) & (yy < rows - 1)
    first = np.unique(xx[inside] + yy[inside] * columns)
    return first[:, None, None] + [[0, 1, columns + 1], [0, columns + 1, columns]]


def crown_constraints(xy, latitude, prior, target, segments, widths, limits):
    base = prior.ravel()
    delta = target.ravel() - base
    faces = _affected_faces(prior.shape, delta).reshape(-1, 3)
    if not len(faces):
        return None
    scales = np.cos(np.deg2rad(latitude))
    pavement = shapely.buffer(
        shapely.linestrings(segments), widths / (2 * scales.min()), quad_segs=2
    )
    route, face = shapely.STRtree(shapely.polygons(xy[faces])).query(
        pavement, predicate="intersects"
    )
    if not len(face):
        return None
    order = np.lexsort((route, face))
    route, face = route[order], face[order]
    nodes = faces[face]
    direction = segments[route, 1] - segments[route, 0]
    direction /= np.linalg.norm(direction, axis=1)[:, None]
    scale = scales[nodes // prior.shape[1]].mean(axis=1)
    weights = np.einsum(
        "fki,fk->fi", _gradient_weights(xy, nodes), direction / scale[:, None]
    )
    initial = (weights * (base[nodes] - base[nodes[:, :1]])).sum(axis=1)
    bound = np.maximum(limits[route], abs(initial))
    lower, upper = -bound - initial, bound - initial
    coefficients = weights * delta[nodes]
    possible = (coefficients.clip(max=0).sum(axis=1) < lower - 1e-10) | (
        coefficients.clip(min=0).sum(axis=1) > upper + 1e-10
    )
    if not possible.any():
        return None
    return CrownConstraints(
        nodes[possible], coefficients[possible], lower[possible], upper[possible]
    )

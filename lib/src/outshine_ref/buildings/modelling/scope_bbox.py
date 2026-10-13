"""Minimum projected-area scope fitting while retaining stationary source geometry."""

import numpy as np
import shapely

from .extrude import euler
from .scope import Frame, Scope, fixed, rotation
from .scope_geometry import effective_size, rebake
from .shape import Shape

SELECTORS = ('xy', 'yx', 'xz', 'zx', 'yz', 'zy')


def referenced_vertices(geometry):
    used = np.zeros(len(geometry.unit_vertices), dtype=bool)
    for name in ('faces', 'edges', 'points'):
        if name in geometry.fields:
            used[geometry.fields[name].ravel()] = True
    for face in geometry.polygons:
        for ring in (face.outer, *face.holes):
            used[list(ring)] = True
    return np.flatnonzero(used)


def minimum_axis(points):
    """Return a canonical long-side direction; GEOS solves the area objective."""
    rectangle = shapely.minimum_rotated_rectangle(shapely.multipoints(points)).normalize()
    if rectangle.geom_type == 'Point':
        return np.array([1., 0.])
    coordinates = np.asarray(rectangle.coords if rectangle.geom_type == 'LineString' else rectangle.exterior.coords)
    edges = np.diff(coordinates, axis=0)
    lengths = np.linalg.norm(edges, axis=1)
    longest = lengths.max()
    candidates = []
    for edge, length in zip(edges, lengths, strict=True):
        if length == 0 or longest-length > longest*1e-12:
            continue
        axis = edge/length
        if axis[0] < 0 or axis[0] == 0 and axis[1] < 0:
            axis = -axis
        candidates.append(axis)
    return max(candidates, key=lambda axis: (axis[0], axis[1]))


def align_scope_to_geometry_bbox(shape: Shape, selector: str = 'xy') -> Shape:
    """Fit the indexed geometry's minimum-area rectangle in a chosen axis pair.

    The first named axis has the longer extent; the third axis stays fixed.
    Pivot and scope origins coincide. Equal-area ties follow GEOS; equal side
    lengths prefer the direction nearest the old first axis. No vertices weld.
    """
    if not isinstance(selector, str) or selector not in SELECTORS:
        raise ValueError('BBox alignment requires xy/yx/xz/zx/yz/zy')
    used = referenced_vertices(shape.geometry)
    if not len(used):
        raise ValueError('BBox alignment requires referenced geometry')
    a, b = ('xyz'.index(axis) for axis in selector)
    c = 3-a-b
    size = effective_size(shape)
    local = shape.geometry.unit_vertices*np.asarray(size)
    fixed(local)
    projected = np.unique(local[used][:, [a, b]], axis=0)
    axis = minimum_axis(projected)
    change = np.zeros((3, 3))
    change[a, a], change[b, a] = axis
    change[c, c] = 1.
    parity = 1 if (a, b, c) in ((0, 1, 2), (1, 2, 0), (2, 0, 1)) else -1
    change[:, b] = parity*np.cross(change[:, c], change[:, a])
    original = shape.scope
    object_axes = original.pivot.axes @ rotation(original.orientation)
    pivot_axes = object_axes @ change
    angles = euler(pivot_axes)
    change = object_axes.T @ rotation(angles)
    fitted = local @ change
    low, high = fitted[used].min(axis=0), fitted[used].max(axis=0)
    position = (np.asarray(original.pivot.position) + original.pivot.axes @ original.position
                + rotation(angles) @ low)
    scope = Scope(size=tuple(high-low), pivot=Frame(tuple(position), angles), object_frame=original.object_frame)
    return rebake(shape, scope, fitted, low, change, size)

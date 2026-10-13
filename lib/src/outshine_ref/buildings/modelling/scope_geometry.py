"""Rotate a modelling scope while keeping its emitted geometry stationary."""

from dataclasses import replace
from types import MappingProxyType

import numpy as np

from .scope import fixed, rotation
from .shape import Shape


def rotate_scope(shape: Shape, x: float, y: float, z: float) -> Shape:
    """Add pivot Euler angles and fit a new axis-aligned box to the geometry.

    Work in the pivot frame so fitting does not subtract large world positions.
    Native indices, logical boundaries, UVs and material metadata remain shared.
    Instance placements become independent geometry; their resources are retained.
    """
    scope = shape.scope.r(x, y, z)
    geometry = shape.geometry
    if not len(geometry.unit_vertices):
        return replace(shape, scope=scope)
    prepared = (shape.asset is not None and geometry.unit_vertices is shape.asset.geometry.unit_vertices
                and geometry.reference_size == shape.asset.geometry.reference_size)
    size = shape.asset.render_size(shape.scope.size) if prepared else np.asarray(shape.scope.size)
    old_axes, new_axes = rotation(shape.scope.orientation), rotation(scope.orientation)
    change = np.diag(size) @ old_axes.T @ new_axes
    local = geometry.unit_vertices @ change
    low, high = local.min(axis=0), local.max(axis=0)
    dimensions = high - low
    position = np.asarray(shape.scope.position) + new_axes @ low
    scope = replace(scope, position=tuple(position), size=tuple(dimensions))
    unit = np.divide(local-low, dimensions, out=np.zeros_like(local), where=dimensions != 0)
    fields = dict(geometry.fields)
    for name in ('normals', 'edges_normals', 'points_normals'):
        if name not in fields:
            continue
        scale = np.divide(size, geometry.reference_size, out=np.ones(3),
                          where=np.asarray(geometry.reference_size) != 0)
        if np.any(scale == 0):
            raise ValueError('Singular authored-normal transforms need a verified cleanup contract')
        normals = fields[name] @ (np.diag(1/scale) @ old_axes.T @ new_axes)
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        fields[name] = fixed(normals)
    fitted = replace(geometry, unit_vertices=fixed(unit), reference_size=scope.size,
                     fields=MappingProxyType(fields))
    return replace(shape, geometry=fitted, scope=scope)

"""Existing roof/wall material domains plus the original downward floor domain.

Source-complete construction needs undersides as well as today's visible faces.
This does not imply equivalence to native tessellation, welds or raster samples.
"""

import numpy as np
import shapely
from column_roof_patches import bottom_m
from source_material_patches import MaterialPatch, shape_material_patches
from source_surface_patches import SurfacePatch


def complete_shape_patches(surface, shape, source, part, basis, anchor):
    patches = shape_material_patches(surface, shape, source, part, basis, anchor)
    domain = shapely.Polygon(shape["ring"], shape["holes"])
    if not domain.is_valid or domain.area <= 0:
        raise ValueError("Complete original floor with its holes required")
    local_origin = np.array([0.0, 0.0, bottom_m(surface, shape)])
    local_axes = np.array([[1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])
    frame = np.array([surface[key] for key in ("east", "north", "up")]) @ basis.T
    origin = np.array(surface["origin"]) @ basis.T - anchor + local_origin[2] * frame[2]
    normal = -np.cross(frame[0], frame[1])
    normal /= np.linalg.norm(normal)
    geometry = SurfacePatch(domain, origin, frame[:2], normal, source)
    patches.append(
        MaterialPatch(
            geometry, shape, part, 0, False, local_origin, local_axes, surface, -1
        )
    )
    return patches

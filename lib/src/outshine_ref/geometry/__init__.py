"""Shared geometry procedures with explicit ownership and numerical contracts."""

from .clearance import vertical_clearance
from .projected_surface import ProjectedSurfaceBudget, projected_height_surface
from .sampling import SurfaceSampler, SurfaceSamples
from .triangulation import conform_triangles
from .welding import weld_vertices

__all__ = [
    "ProjectedSurfaceBudget",
    "SurfaceSampler",
    "SurfaceSamples",
    "conform_triangles",
    "projected_height_surface",
    "vertical_clearance",
    "weld_vertices",
]

"""Shared geometry procedures with explicit ownership and numerical contracts."""

from .triangulation import conform_triangles
from .sampling import SurfaceSampler, SurfaceSamples
from .projected_surface import ProjectedSurfaceBudget, projected_height_surface

__all__ = [
    "conform_triangles",
    "SurfaceSampler",
    "SurfaceSamples",
    "ProjectedSurfaceBudget",
    "projected_height_surface",
]

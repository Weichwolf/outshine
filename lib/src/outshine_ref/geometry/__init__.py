"""Shared geometry procedures with explicit ownership and numerical contracts."""

from .triangulation import conform_triangles
from .sampling import SurfaceSampler, SurfaceSamples

__all__ = ["conform_triangles", "SurfaceSampler", "SurfaceSamples"]


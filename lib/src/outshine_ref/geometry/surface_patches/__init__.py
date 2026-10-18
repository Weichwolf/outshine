"""Exact resident height-surface patches; storage/LOD/source semantics stay outside."""

from ._field import SurfacePatches
from ._patch import SurfacePatch
from ._query import SurfaceQuery

__all__ = ["SurfacePatch", "SurfacePatches", "SurfaceQuery"]

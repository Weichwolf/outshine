"""Diagnostic access to the single shared three-arm geometry implementation."""

from outshine_ref.transport.canonical_ports._ground_star_geometry import (
    arm_polygons,
    layout,
    profile_cuts,
)

__all__ = ["arm_polygons", "layout", "profile_cuts"]

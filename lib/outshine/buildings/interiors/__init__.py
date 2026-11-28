"""Bounded activity-aware furnishing placement within supplied connected rooms.

The caller owns room IDs, use programmes, frame and height datum. Local XY inputs
use metres. These functions plan complete footprints, activity zones and actor
centre paths; the caller separately verifies generated 3D meshes and headroom.
"""

from .placement import Furnishing, access, candidates, layout, placed

__all__ = ["Furnishing", "access", "candidates", "layout", "placed"]

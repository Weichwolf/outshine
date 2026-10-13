"""Reference contracts and procedures shared by Outshine's generator experiments."""

from . import buildings, geometry, transport, vegetation
from .contracts import (
    BuildingFootprint, FootprintAdjustment, HeightInterval, Hint, LocalFrame,
    Mesh, Reservation, SourceFeature, SourceInputs,
)
from .pipeline import PreparedWorld, prepare

__all__ = [
    "BuildingFootprint", "FootprintAdjustment", "HeightInterval", "Hint", "LocalFrame",
    "Mesh", "PreparedWorld", "Reservation", "SourceFeature", "SourceInputs",
    "buildings", "geometry", "prepare", "transport", "vegetation",
]


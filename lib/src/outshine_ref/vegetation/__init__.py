"""Vegetation consumes binding spaces and optional hints from both predecessors."""

from dataclasses import dataclass
from importlib import import_module

from ..contracts import Hint, LocalFrame, Mesh, Reservation, SourceInputs, collection
from .blade_surface import Blade
from .needle_surface import Needle
from .placement_patterns import DistancePattern
from .profiles import Profile


@dataclass(frozen=True, slots=True)
class Inputs:
    sources: SourceInputs
    reservations: tuple[Reservation, ...]
    hints: tuple[Hint, ...]


@dataclass(frozen=True, slots=True)
class Plan:
    frame: LocalFrame
    meshes: tuple[Mesh, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.frame, LocalFrame):
            raise ValueError("one explicit local frame is required")
        collection(self.meshes, Mesh, "vegetation meshes")


_SPATIAL_EXPORTS = {
    "AttractionField": "force_fields",
    "DirectionField": "force_fields",
    "RadialFalloff": "force_fields",
    "GrowthSpine": "growth_spine",
    "GrowthResult": "growth_spine",
    "AxisLevel": "plant_levels",
    "AxisState": "plant_plan",
    "ArcMeasure": "spine_measure",
    "CubicSpine": "spine_curve",
    "FinAttachment": "fin_attachment",
    "FrondCurl": "frond_curl",
    "FrondGuide": "frond_surface",
    "LeafletShape": "leaflet_shape",
    "PinnatePattern": "frond_pattern",
    "PlantPlan": "plant_plan",
    "Shoot": "shoot_plan",
    "SpatialAxis": "spatial_axis",
    "SpatialFrondGuide": "spatial_frond",
    "SpineField": "spine_field",
}


def __getattr__(name):
    if name not in _SPATIAL_EXPORTS:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    value = getattr(import_module(f"{__name__}.{_SPATIAL_EXPORTS[name]}"), name)
    globals()[name] = value
    return value


__all__ = [
    "ArcMeasure",
    "AttractionField",
    "AxisLevel",
    "AxisState",
    "Blade",
    "CubicSpine",
    "DirectionField",
    "DistancePattern",
    "FinAttachment",
    "FrondCurl",
    "FrondGuide",
    "GrowthResult",
    "GrowthSpine",
    "Inputs",
    "LeafletShape",
    "Needle",
    "PinnatePattern",
    "Plan",
    "PlantPlan",
    "Profile",
    "RadialFalloff",
    "Shoot",
    "SpatialAxis",
    "SpatialFrondGuide",
    "SpineField",
]

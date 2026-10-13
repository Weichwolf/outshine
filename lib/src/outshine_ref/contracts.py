"""Local ENU metres, retained source identities and camera-independent products."""

from dataclasses import dataclass, field
from math import isfinite
from types import MappingProxyType
from typing import Mapping

import numpy as np
import shapely
from shapely.geometry.base import BaseGeometry


def identifier(value: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise ValueError("a nonempty stable identifier is required")


def collection(values: tuple, kind: type, name: str) -> None:
    if not isinstance(values, tuple) or not all(isinstance(value, kind) for value in values):
        raise ValueError(name + " must be an immutable tuple of " + kind.__name__)


def geometry(value: BaseGeometry, *, polygonal: bool = False) -> None:
    if not isinstance(value, BaseGeometry) or value.is_empty or not value.is_valid:
        raise ValueError("a nonempty valid geometry is required")
    if value.has_z or not np.isfinite(shapely.get_coordinates(value)).all():
        raise ValueError("geometry must use finite local XY metres; heights are explicit")
    if polygonal and value.geom_type not in ("Polygon", "MultiPolygon"):
        raise ValueError("a polygon or multipolygon is required")


def immutable(value: object) -> object:
    if isinstance(value, Mapping):
        if not all(isinstance(key, str) for key in value):
            raise ValueError("source attribute keys must be strings")
        return MappingProxyType({key: immutable(item) for key, item in value.items()})
    if isinstance(value, (list, tuple)):
        return tuple(immutable(item) for item in value)
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float) and isfinite(value):
        return value
    raise ValueError("source attributes must contain finite JSON values")


@dataclass(frozen=True, slots=True)
class LocalFrame:
    """One named right-handed east/north/up frame, shared by all region products."""

    identifier: str

    def __post_init__(self) -> None:
        identifier(self.identifier)


@dataclass(frozen=True, slots=True)
class HeightInterval:
    """Local Z bounds in metres, using the region frame's common height datum."""

    lower_m: float
    upper_m: float

    def __post_init__(self) -> None:
        if not all(isfinite(v) for v in (self.lower_m, self.upper_m)) or self.lower_m >= self.upper_m:
            raise ValueError("a finite positive height interval is required")

    def overlaps(self, other: "HeightInterval") -> bool:
        return max(self.lower_m, other.lower_m) < min(self.upper_m, other.upper_m)


@dataclass(frozen=True, slots=True)
class SourceFeature:
    source_id: str
    geometry: BaseGeometry
    attributes: Mapping[str, object] = field(default_factory=dict)

    def __post_init__(self) -> None:
        identifier(self.source_id)
        geometry(self.geometry)
        if not isinstance(self.attributes, Mapping):
            raise ValueError("source attributes must be a mapping")
        object.__setattr__(self, "attributes", immutable(self.attributes))


@dataclass(frozen=True, slots=True)
class BuildingFootprint:
    source_id: str
    geometry: BaseGeometry
    heights: HeightInterval
    part: tuple[int, ...] = ()
    attributes: Mapping[str, object] = field(default_factory=dict)

    def __post_init__(self) -> None:
        identifier(self.source_id)
        geometry(self.geometry, polygonal=True)
        if not isinstance(self.heights, HeightInterval):
            raise ValueError("explicit building heights are required")
        if not isinstance(self.part, tuple) or any(type(v) is not int or v < 0 for v in self.part):
            raise ValueError("part identity requires a tuple of nonnegative integers")
        if not isinstance(self.attributes, Mapping):
            raise ValueError("building source attributes must be a mapping")
        object.__setattr__(self, "attributes", immutable(self.attributes))

    @property
    def key(self) -> tuple[str, tuple[int, ...]]:
        return self.source_id, self.part


@dataclass(frozen=True, slots=True)
class FootprintAdjustment:
    """Replace one original footprint with explained parts of the same source."""

    original: BuildingFootprint
    adjusted: tuple[BuildingFootprint, ...]
    reason: str

    def __post_init__(self) -> None:
        identifier(self.reason)
        if not isinstance(self.original, BuildingFootprint):
            raise ValueError("a source building footprint is required")
        collection(self.adjusted, BuildingFootprint, "adjusted footprints")
        if any(p.source_id != self.original.source_id for p in self.adjusted):
            raise ValueError("adjusted footprints must retain their source identity")
        if any(p.attributes != self.original.attributes for p in self.adjusted):
            raise ValueError("footprint changes must retain source attributes")
        if len({p.key for p in self.adjusted}) != len(self.adjusted):
            raise ValueError("adjusted part identities must be distinct")


@dataclass(frozen=True, slots=True)
class Reservation:
    """A binding spatial constraint with an explicit owner and height range."""

    owner_id: str
    geometry: BaseGeometry
    heights: HeightInterval
    purpose: str

    def __post_init__(self) -> None:
        identifier(self.owner_id)
        identifier(self.purpose)
        geometry(self.geometry, polygonal=True)
        if not isinstance(self.heights, HeightInterval):
            raise ValueError("reservation heights must be explicit")

    def overlaps(self, footprint: BuildingFootprint) -> bool:
        return (self.heights.overlaps(footprint.heights)
                and self.geometry.intersection(footprint.geometry).area > 0)


@dataclass(frozen=True, slots=True)
class Hint:
    """Optional design guidance; it never substitutes for a reservation."""

    owner_id: str
    kind: str
    geometry: BaseGeometry

    def __post_init__(self) -> None:
        identifier(self.owner_id)
        identifier(self.kind)
        geometry(self.geometry)


@dataclass(frozen=True, slots=True, eq=False)
class Mesh:
    """Owned immutable local XYZ and triangle buffers for reference comparisons."""

    vertices: np.ndarray
    triangles: np.ndarray

    def __post_init__(self) -> None:
        vertices = np.array(self.vertices, dtype=np.float64, copy=True)
        triangles = np.asarray(self.triangles)
        if vertices.ndim != 2 or vertices.shape[1] != 3 or not np.isfinite(vertices).all():
            raise ValueError("mesh vertices require finite Nx3 local coordinates")
        if triangles.ndim != 2 or triangles.shape[1] != 3 or triangles.dtype.kind not in "iu":
            raise ValueError("mesh triangles require an Mx3 integer index array")
        if len(triangles) and (triangles.min() < 0 or triangles.max() >= len(vertices)):
            raise ValueError("mesh triangle indices lie outside their vertices")
        if len(vertices) > np.iinfo(np.uint32).max:
            raise ValueError("reference mesh indices exceed uint32 capacity")
        triangles = np.array(triangles, dtype=np.uint32, copy=True)
        vertices.setflags(write=False)
        triangles.setflags(write=False)
        object.__setattr__(self, "vertices", vertices)
        object.__setattr__(self, "triangles", triangles)


@dataclass(frozen=True, slots=True)
class SourceInputs:
    frame: LocalFrame
    transport: tuple[SourceFeature, ...] = ()
    buildings: tuple[BuildingFootprint, ...] = ()
    vegetation: tuple[SourceFeature, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.frame, LocalFrame):
            raise ValueError("one explicit local frame is required")
        collection(self.transport, SourceFeature, "transport sources")
        collection(self.buildings, BuildingFootprint, "building sources")
        collection(self.vegetation, SourceFeature, "vegetation sources")
        if len({p.key for p in self.buildings}) != len(self.buildings):
            raise ValueError("building source/part identities must be distinct")
        for values in (self.transport, self.vegetation):
            if len({p.source_id for p in values}) != len(values):
                raise ValueError("source identities must be distinct within each domain")


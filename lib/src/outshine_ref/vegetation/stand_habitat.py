"""Prepared source habitat on an explicitly named, square planar metre region."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
import shapely
from shapely.geometry.base import BaseGeometry

from ..contracts import LocalFrame


def polygon_components(value):
    if value.geom_type == "Polygon":
        return [value] if value.area > 0 else []
    if value.geom_type in ("MultiPolygon", "GeometryCollection"):
        return [
            polygon
            for part in shapely.get_parts(value)
            for polygon in polygon_components(part)
        ]
    return []


@dataclass(frozen=True)
class StandHabitat:
    """Source footprint and effective support in one shared frame; no height is inferred."""

    frame: LocalFrame
    source_id: str
    region_id: str
    source_geometry: BaseGeometry
    domain: BaseGeometry
    lower: tuple[float, float]
    side_m: float
    source_area_m2: float
    area_m2: float

    def __post_init__(self):
        if not isinstance(self.frame, LocalFrame):
            raise TypeError("shared named east/north/up frame required")
        if any(
            not isinstance(s, str) or not 1 <= len(s.encode()) <= 512
            for s in (self.source_id, self.region_id)
        ):
            raise ValueError(
                "bounded explicit source and canonical region IDs required"
            )
        if (
            not isinstance(self.lower, tuple)
            or len(self.lower) != 2
            or not np.isfinite(self.lower).all()
            or max(abs(v) for v in self.lower) > 1e8
        ):
            raise ValueError("finite bounded planar metre origin required")
        if (
            not np.isfinite((self.side_m, self.source_area_m2, self.area_m2)).all()
            or not 0.25 <= self.side_m <= 262144
            or min(self.source_area_m2, self.area_m2) < 0
        ):
            raise ValueError(
                "bounded region side and nonnegative prepared areas required"
            )
        for geometry in (self.source_geometry, self.domain):
            if (
                not isinstance(geometry, BaseGeometry)
                or geometry.geom_type not in ("Polygon", "MultiPolygon")
                or (
                    not geometry.is_empty
                    and shapely.get_coordinate_dimension(geometry) != 2
                )
                or not geometry.is_valid
                or not np.isfinite(shapely.get_coordinates(geometry)).all()
            ):
                raise ValueError("finite valid prepared polygon geometry required")
            if shapely.get_num_coordinates(geometry) > 16384:
                raise ValueError("habitat boundary vertex budget")
        for geometry, area in (
            (self.source_geometry, self.source_area_m2),
            (self.domain, self.area_m2),
        ):
            if abs(float(geometry.area) - area) > max(1e-8, area * 1e-10):
                raise ValueError("prepared habitat area does not match geometry")
        if self.area_m2 > self.source_area_m2 + max(1e-8, self.source_area_m2 * 1e-10):
            raise ValueError("prepared habitat exceeds source area")
        if self.area_m2:
            bounds = np.asarray(self.domain.bounds)
            expected = np.r_[self.lower, np.asarray(self.lower) + self.side_m]
            if np.any(bounds[:2] < expected[:2] - 1e-8) or np.any(
                bounds[2:] > expected[2:] + 1e-8
            ):
                raise ValueError("prepared domain outside canonical region")

    @classmethod
    def prepare(
        cls, frame, source_id, region_id, geometry, lower, side_m, *, support=None
    ):
        """Normalize/clip once; caller resolves height-aware reservations into support."""
        if (
            not isinstance(geometry, BaseGeometry)
            or geometry.geom_type not in ("Polygon", "MultiPolygon")
            or not geometry.is_valid
        ):
            raise ValueError("valid source polygons required")
        lower = tuple(float(v) for v in lower)
        side_m = float(side_m)
        # Validate dimensions and source complexity before any spatial clipping.
        empty = shapely.Polygon()
        cls(
            frame,
            source_id,
            region_id,
            geometry,
            empty,
            lower,
            side_m,
            float(geometry.area),
            0.0,
        )
        source = shapely.normalize(geometry)
        effective = source if support is None else support
        if not isinstance(effective, BaseGeometry):
            raise TypeError("polygonal effective support required")
        cls(
            frame,
            source_id,
            region_id,
            effective,
            empty,
            lower,
            side_m,
            float(effective.area),
            0.0,
        )
        if support is not None and effective.difference(source).area > max(
            1e-8, source.area * 1e-10
        ):
            raise ValueError("effective support extends outside source habitat")
        clipped = effective.intersection(
            shapely.box(*lower, lower[0] + side_m, lower[1] + side_m)
        )
        polygons = polygon_components(clipped)
        domain = shapely.normalize(shapely.union_all(polygons)) if polygons else empty
        return cls(
            frame,
            source_id,
            region_id,
            source,
            domain,
            lower,
            side_m,
            float(source.area),
            float(domain.area),
        )

    @cached_property
    def _domain_wkb(self):
        return self.domain.wkb_hex

    @property
    def layout(self):
        return {
            "frame": self.frame.identifier,
            "source_id": self.source_id,
            "region_id": self.region_id,
            "lower": list(self.lower),
            "side_m": self.side_m,
            "domain_wkb": self._domain_wkb,
        }

    def recipe(self):
        return self.layout | {
            "format": "stand-habitat-1",
            "source_wkb": self.source_geometry.wkb_hex,
            "source_area_m2": self.source_area_m2,
            "area_m2": self.area_m2,
        }

    @classmethod
    def from_recipe(cls, recipe):
        if recipe["format"] != "stand-habitat-1":
            raise ValueError("supported prepared habitat recipe required")
        # Decode prepared products; do not repeat source normalization/clipping.
        return cls(
            LocalFrame(recipe["frame"]),
            recipe["source_id"],
            recipe["region_id"],
            shapely.from_wkb(recipe["source_wkb"]),
            shapely.from_wkb(recipe["domain_wkb"]),
            tuple(recipe["lower"]),
            recipe["side_m"],
            recipe["source_area_m2"],
            recipe["area_m2"],
        )

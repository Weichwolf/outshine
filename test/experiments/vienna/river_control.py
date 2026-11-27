"""Separately meshed reaches, pinned survey grades and solid gate faces."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.water.geodetic import curvature_radii
from shapely.ops import split


@dataclass(frozen=True)
class RiverProfile:
    origin: np.ndarray
    metres: np.ndarray
    axis: shapely.LineString
    stations_m: np.ndarray
    drops_m: np.ndarray

    @classmethod
    def from_recipe(cls, recipe):
        lonlat = np.asarray(recipe["downstream_centerline_lon_lat"], dtype=float)
        stations = np.asarray(recipe["station_km_and_adria_m"], dtype=float)
        if any(
            rows.ndim != 2
            or rows.shape[1] != 2
            or len(rows) < 2
            or not np.isfinite(rows).all()
            for rows in (lonlat, stations)
        ):
            raise ValueError(
                "River profile requires a finite axis and at least two stations"
            )
        origin = lonlat[0]
        prime, meridian = curvature_radii(origin[1])
        metres = np.deg2rad(1) * np.array(
            [prime * np.cos(np.deg2rad(origin[1])), meridian]
        )
        axis = shapely.LineString((lonlat - origin) * metres)
        chain = stations[:, 0]
        heights = stations[:, 1]
        if (
            not axis.is_simple
            or axis.length == 0
            or not np.all(np.diff(chain) > 0)
            or (np.diff(heights) > 0).any()
        ):
            raise ValueError(
                "River profile requires an ordered axis and downhill stations"
            )
        distance = (chain - chain[0]) * axis.length / (chain[-1] - chain[0])
        return cls(origin, metres, axis, distance, heights - heights[0])

    def drop(self, lonlat):
        points = shapely.points((lonlat - self.origin) * self.metres)
        along = shapely.line_locate_point(self.axis, points)
        return np.interp(along, self.stations_m, self.drops_m)


@dataclass(frozen=True)
class RiverControl:
    key: tuple
    section: np.ndarray
    upstream_m: float
    downstream_m: float
    profile: RiverProfile

    @classmethod
    def from_recipe(cls, recipe):
        upstream, downstream = recipe["plateaus_model_m"]
        section = np.asarray(recipe["section_lon_lat"], dtype=float)
        if (
            section.shape != (2, 2)
            or not np.isfinite(section).all()
            or np.array_equal(section[0], section[1])
        ):
            raise ValueError("Gate section requires two distinct finite endpoints")
        if not np.isfinite([upstream, downstream]).all() or not upstream > downstream:
            raise ValueError("Expected a positive gate head")
        return cls(
            (recipe["water_class"], str(recipe["water_id"])),
            section,
            upstream,
            downstream,
            RiverProfile.from_recipe(recipe),
        )

    def reaches(self, source, footprint):
        origin = [source.lon, source.lat]
        a, b = (self.section - origin) * source.metres
        along = (b - a) / np.linalg.norm(b - a)
        normal = along[[1, 0]] * [1, -1]
        downstream = self.profile.axis.interpolate(30).coords[0]
        downstream = (
            downstream / self.profile.metres + self.profile.origin - origin
        ) * source.metres
        normal *= np.sign((downstream - a) @ normal)
        cut = shapely.LineString([a - 20 * along, b + 20 * along])
        for part in split(footprint, cut).geoms:
            witness = np.asarray(part.representative_point().coords[0])
            yield part, (witness - a) @ normal > 0


def gate_faces(source, bodies):
    controls = {
        id(body.control): body.control for body in bodies if body.control is not None
    }
    for control in controls.values():
        high = [
            body for body in bodies if body.control is control and body.profile is None
        ]
        low = [
            body
            for body in bodies
            if body.control is control and body.profile is not None
        ]
        for upper in high:
            for lower in low:
                common = upper.footprint.boundary.intersection(
                    lower.footprint.boundary, grid_size=1e-6
                )
                for line in shapely.get_parts(common):
                    if line.geom_type != "LineString" or line.length < 1e-5:
                        continue
                    xy = shapely.get_coordinates(shapely.segmentize(line, 8))
                    top = np.column_stack((xy, upper.heights(source, xy)))
                    bottom = np.column_stack((xy, lower.heights(source, xy) - 0.25))
                    points = np.concatenate((top, bottom))
                    a = np.arange(len(xy) - 1)
                    b = a + len(xy)
                    tris = np.column_stack((a, a + 1, b, b, a + 1, b + 1))
                    yield points, tris.reshape(-1, 3)

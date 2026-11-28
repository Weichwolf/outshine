"""Shared inferred water levels for the Vienna reference meshes."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely

from outshine.terrain.ground_surface import polygons
from outshine.transport.bridges import cap_mesh
from outshine.water.geodetic import local_points
from outshine.water.river_control import RiverControl, RiverProfile


@dataclass(frozen=True)
class WaterBody:
    footprint: shapely.Geometry
    level: float
    profile: RiverProfile | None = None
    control: RiverControl | None = None
    water_class: str = ""
    source_id: str = ""

    def level_at(self, lonlat):
        if self.profile is None:
            return np.full(len(lonlat), self.level)
        return self.level + self.profile.drop(lonlat)

    def heights(self, source, points):
        lonlat = points / source.metres + [source.lon, source.lat]
        level = self.level_at(lonlat) + source.datum
        return (
            local_points((source.lon, source.lat), lonlat, level)[:, 2] - source.datum
        )

    def mesh(self, source, footprint):
        return cap_mesh(
            shapely.segmentize(footprint, 24.0), lambda xy: self.heights(source, xy)
        )


def water_footprints(features):
    groups = defaultdict(list)
    for name, geometry, props in features:
        if name == "water":
            key = props.get("class", ""), str(props.get("id", ""))
            groups[key].extend(polygons(shapely.make_valid(geometry)))
    # Touching mapped reaches keep their identities. Resolve area overlaps once.
    entries = [
        (key, part)
        for key in sorted(groups)
        for part in polygons(shapely.union_all(groups[key]))
    ]
    shapes = [shape for _, shape in entries]
    index = shapely.STRtree(shapes)
    for i, (key, shape) in enumerate(entries):
        earlier = [
            shapes[j] for j in index.query(shape, predicate="intersects") if j < i
        ]
        if earlier:
            for part in polygons(shape.difference(shapely.union_all(earlier))):
                yield key, part
        else:
            yield key, shape


def water_level(body, xy, height):
    samples = xy[shapely.contains_xy(body, *xy.T)]
    if not len(samples):
        samples = np.asarray(body.representative_point().coords)
    return float(np.median(height(samples)))


def water_bodies(
    features,
    domain,
    samples,
    local_height,
    regional_height,
    *,
    source=None,
    controls=(),
):
    control_by_key = {}
    for control in controls:
        if source is None or control.key in control_by_key:
            raise ValueError(
                "Water control requires a source frame and a unique mapped ID"
            )
        control_by_key[control.key] = control
    for key, footprint in water_footprints(features):
        if control := control_by_key.get(key):
            for part, downstream in control.reaches(source, footprint):
                level = control.downstream_m if downstream else control.upstream_m
                profile = control.profile if downstream else None
                yield WaterBody(part, level - source.datum, profile, control, *key)
            continue
        local = footprint.intersection(domain)
        if local.area > 0:
            level = water_level(local, samples, local_height)
        else:
            x0, y0, x1, y1 = footprint.bounds
            x = x0 + (0.5 + np.arange(24)) * (x1 - x0) / 24
            y = y0 + (0.5 + np.arange(24)) * (y1 - y0) / 24
            xy = np.stack(np.meshgrid(x, y), axis=-1).reshape(-1, 2)
            level = water_level(footprint, xy, regional_height)
        yield WaterBody(footprint, level, water_class=key[0], source_id=key[1])


def clipped_water(bodies, domain):
    for body in bodies:
        if body.footprint.intersects(domain):
            for part in polygons(body.footprint.intersection(domain)):
                yield body, part

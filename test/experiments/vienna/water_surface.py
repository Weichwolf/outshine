"""Shared inferred water levels for the Vienna reference meshes."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from bridges import cap_mesh
from ground_surface import polygons
from outshine_ref.water.geodetic import local_points


@dataclass(frozen=True)
class WaterBody:
    footprint: shapely.Geometry
    level: float

    def heights(self, source, points):
        lonlat = points / source.metres + [source.lon, source.lat]
        return (
            local_points((source.lon, source.lat), lonlat, self.level + source.datum)[
                :, 2
            ]
            - source.datum
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
    shapes = [
        part
        for key in sorted(groups)
        for part in polygons(shapely.union_all(groups[key]))
    ]
    index = shapely.STRtree(shapes)
    for i, shape in enumerate(shapes):
        earlier = [
            shapes[j] for j in index.query(shape, predicate="intersects") if j < i
        ]
        if earlier:
            yield from polygons(shape.difference(shapely.union_all(earlier)))
        else:
            yield shape


def water_level(body, xy, height):
    samples = xy[shapely.contains_xy(body, *xy.T)]
    if not len(samples):
        samples = np.asarray(body.representative_point().coords)
    return float(np.median(height(samples)))


def water_bodies(features, domain, samples, local_height, regional_height):
    for footprint in water_footprints(features):
        local = footprint.intersection(domain)
        if local.area > 0:
            level = water_level(local, samples, local_height)
        else:
            x0, y0, x1, y1 = footprint.bounds
            x = x0 + (0.5 + np.arange(24)) * (x1 - x0) / 24
            y = y0 + (0.5 + np.arange(24)) * (y1 - y0) / 24
            xy = np.stack(np.meshgrid(x, y), axis=-1).reshape(-1, 2)
            level = water_level(footprint, xy, regional_height)
        yield WaterBody(footprint, level)


def clipped_water(bodies, domain):
    for body in bodies:
        if body.footprint.intersects(domain):
            for part in polygons(body.footprint.intersection(domain)):
                yield body, part

"""Grade the open bank beside a mapped route without moving its pavement."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.transport.bridge_passages import Passage

BANK_REACH_M = 8.0
MOUTH_REACH_M = 12.0
END_TAPER_M = 3.0
TRANSPORT_MARGIN_M = 0.2


@dataclass(frozen=True)
class BankSlope:
    footprint: shapely.Geometry
    contact: shapely.Geometry
    exterior: shapely.Geometry
    ends: shapely.Geometry
    passage: Passage

    @property
    def retaining_edge(self):
        return shapely.LineString()

    @property
    def breaks(self):
        bands = shapely.buffer(self.contact, [1.25, 2.5, 5.0], quad_segs=2)
        return shapely.intersection(shapely.boundary(bands), self.footprint)

    def heights(self, ground):
        points = shapely.points(ground[:, :2])
        inside = shapely.distance(points, self.contact)
        outside = shapely.distance(points, self.exterior)
        span = inside + outside
        blend = np.divide(outside, span, out=np.zeros_like(span), where=span > 1e-6)
        blend = blend * blend * (3 - 2 * blend)
        if not self.ends.is_empty:
            taper = np.clip(shapely.distance(points, self.ends) / END_TAPER_M, 0, 1)
            blend *= taper * taper * (3 - 2 * taper)
        nearest = shapely.get_coordinates(
            shapely.get_point(shapely.shortest_line(points, self.contact), -1)
        )
        edge = self.passage.surface.sample(nearest).points_enu_m[:, 2]
        if not np.isfinite(edge).all():
            raise ValueError("Bank slope must meet the solved pavement")
        return ground[:, 2] + blend * np.minimum(edge - ground[:, 2], 0)


def bank_edges(route, water):
    xy = route.sections.mean(axis=1)
    points = shapely.points(xy)
    nearest = shapely.get_coordinates(
        shapely.get_point(shapely.shortest_line(points, water), -1)
    )
    lateral = route.sections[:, 1] - route.sections[:, 0]
    side = np.sign(((nearest - xy) * lateral).sum(axis=1))
    under = np.unique(np.concatenate([nodes for _, nodes, _ in route.ceilings]))
    distance = np.min(abs(route.station[:, None] - route.station[under]), axis=1)
    side[distance >= MOUTH_REACH_M] = 0
    changes = np.flatnonzero(np.diff(side)) + 1
    for part in np.split(np.arange(len(side)), changes):
        if len(part) < 2 or not side[part[0]]:
            continue
        direction = int(side[part[0]])
        yield shapely.LineString(route.sections[part, int(direction > 0)]), direction


def bank_slopes(passages, water, protected, extent):
    if not passages:
        return []
    occupied = shapely.union_all([p.footprint for p in passages], grid_size=1e-6)
    result = []
    for passage in passages:
        for edge, side in bank_edges(passage.route, water):
            band = edge.buffer(
                side * BANK_REACH_M, single_sided=True, join_style="mitre"
            )
            available = extent.difference(protected.union(occupied), grid_size=1e-6)
            band = band.intersection(available, grid_size=1e-6)
            for part in shapely.get_parts(band):
                if part.geom_type != "Polygon" or part.area < 1:
                    continue
                contact = part.boundary.intersection(edge, grid_size=1e-6)
                runs = shapely.get_parts(shapely.line_merge(contact))
                lines = [line for line in runs if line.geom_type == "LineString"]
                if not lines or sum(line.length for line in lines) < 2 * END_TAPER_M:
                    continue
                contact = shapely.union_all(lines)
                exterior = part.boundary.difference(contact, grid_size=1e-6)
                ends = shapely.union_all([line.boundary for line in lines])
                result.append(BankSlope(part, contact, exterior, ends, passage))
                occupied = occupied.union(part, grid_size=1e-6)
    return result


def bank_rim(ground, slopes):
    heights = ground[:, 2].copy()
    points = shapely.points(ground[:, :2])
    for slope in slopes:
        hit = shapely.dwithin(points, slope.contact, 2e-6)
        if hit.any():
            heights[hit] = slope.heights(ground[hit])
    return heights

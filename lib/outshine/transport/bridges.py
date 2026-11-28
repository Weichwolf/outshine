"""Assemble closed slabs and edge guards for mapped bank-to-bank bridges."""

from collections import defaultdict
from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROAD_WIDTH, path_material
from outshine.transport.bridge_truss import draw_open_truss
from outshine.transport.water_span import WaterSpan, solve_water_span


def cap_mesh(polygon, height):
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    xy = np.array(
        [p.exterior.coords[:3] for p in shapely.orient_polygons(triangles)]
    ).reshape(-1, 2)
    return np.column_stack((xy, height(xy))), np.arange(len(xy)).reshape(-1, 3)


def slab(meshes, material, polygon, height, thickness, *, paved=None, pedestrian=None):
    polygon = shapely.orient_polygons(polygon)
    top, faces = cap_mesh(polygon, height)
    surfaces = []
    remaining = polygon
    for surface, area in (("road", paved), ("paving", pedestrian)):
        if area is not None and not area.is_empty:
            assigned = remaining.intersection(area)
            remaining = remaining.difference(assigned)
            if assigned.area > 0:
                surfaces.append((surface, assigned))
    if not surfaces:
        meshes.add(material, top, faces)
    else:
        meshes.add(material, *cap_mesh(remaining, height))
        for surface, area in surfaces:
            meshes.add(surface, *cap_mesh(area, height))
    meshes.add(material, top - [0, 0, thickness], faces[:, ::-1])
    for ring in [polygon.exterior, *polygon.interiors]:
        xy = np.asarray(ring.coords)
        top = np.column_stack((xy, height(xy)))
        count = len(xy)
        a = np.arange(count - 1)
        sides = np.column_stack((a, a + count, a + 1, a + 1, a + count, a + count + 1))
        meshes.add(
            material, np.vstack((top, top - [0, 0, thickness])), sides.reshape(-1, 3)
        )


def guard_segments(polygon, direction, access, water, *, carriageway=None):
    openings = shapely.union_all(access).buffer(2e-6)
    wet = water.difference(polygon).buffer(2e-6) if water is not None else None
    if wet is not None and carriageway is not None:
        wet = wet.difference(carriageway.buffer(2e-6))
    for ring in [polygon.exterior, *polygon.interiors]:
        rails = shapely.linestrings(list(pairwise(ring.coords)))
        if not openings.is_empty:
            rails = shapely.get_parts(shapely.difference(rails, openings))
        for rail in rails:
            if rail.geom_type != "LineString" or rail.is_empty:
                continue
            a, b = np.asarray(rail.coords)[[0, -1]]
            delta = b - a
            length = np.linalg.norm(delta)
            if abs(delta @ direction) >= 0.72 * length:
                yield rail
            elif wet is not None:
                for part in shapely.get_parts(rail.intersection(wet)):
                    if part.geom_type == "LineString" and not part.is_empty:
                        start, end = np.asarray(part.coords)[[0, -1]]
                        yield (
                            part if (end - start) @ delta > 0 else shapely.reverse(part)
                        )


def edge_guards(meshes, polygon, span, access=(), *, water=None, carriageway=None):
    for rail in guard_segments(
        polygon, span.direction, access, water, carriageway=carriageway
    ):
        length = rail.length
        if length < 0.16:
            continue
        a, b = np.asarray(rail.coords)[[0, -1]]
        delta = b - a
        # Move the rail just inside the walking edge.
        inset = np.array([-delta[1], delta[0]]) * (0.08 / length)
        start, end = a + inset, b + inset
        for rise in (0.55, 1.05):
            bar = shapely.LineString([start, end]).buffer(0.032, cap_style="flat")
            slab(
                meshes,
                "bridge metal",
                bar,
                lambda xy, h=rise: span.height(xy) + h,
                0.065,
            )
        divisions = max(1, int(np.ceil(length / 1.8)))
        for x, y in np.linspace(start, end, divisions + 1):
            post = shapely.box(x - 0.036, y - 0.036, x + 0.036, y + 0.036)
            slab(meshes, "bridge metal", post, lambda xy: span.height(xy) + 1.05, 1.05)


def opens_deck_side(edges, span):
    for edge in edges:
        for line in shapely.get_parts(edge):
            if line.geom_type != "LineString":
                continue
            sides = np.diff(np.asarray(line.coords), axis=0)
            length = np.linalg.norm(sides, axis=1)
            if np.any((length > 1e-6) & (abs(sides @ span.direction) >= 0.72 * length)):
                return True
    return False


@dataclass(frozen=True)
class WaterCrossing:
    footprint: shapely.Geometry
    span: WaterSpan
    pavement: shapely.Geometry
    layer: int
    bank_contact: shapely.Geometry | None = None
    pedestrian_paving: shapely.Geometry | None = None

    @property
    def thickness(self):
        return min(3.2, self.span.length / 40)


def plan_water_crossings(features, extent, water, height, *, exclude=None):
    outlines, routes, lanes = defaultdict(list), defaultdict(list), defaultdict(list)
    pedestrian = defaultdict(list)
    for name, geom, prop in features:
        if name != "transportation":
            continue
        if (
            geom.geom_type.endswith("Polygon")
            and prop.get("class") == "path"
            and path_material(prop) == "paving"
        ):
            pedestrian[
                prop.get("layer", 1 if prop.get("brunnel") == "bridge" else None)
            ].append(shapely.make_valid(geom))
        if prop.get("brunnel") != "bridge":
            continue
        level = prop.get("layer", 1)
        if prop["class"] == "bridge" and geom.geom_type.endswith("Polygon"):
            outlines[level].append(shapely.make_valid(geom))
        elif geom.geom_type.endswith("LineString"):
            routes[level].append(geom)
            kind = prop.get("class")
            if kind in ROAD_WIDTH and kind != "path":
                lanes[level].append((geom, ROAD_WIDTH[kind]))
    for level, pieces in outlines.items():
        for footprint in shapely.get_parts(shapely.union_all(pieces)):
            if footprint.geom_type != "Polygon" or not extent.covers(footprint):
                continue
            if exclude is not None and exclude.covers(footprint):
                continue
            span = solve_water_span(footprint, routes[level], water, height)
            if span is None:
                continue
            footprint = shapely.orient_polygons(footprint)
            paved = shapely.union_all(
                [
                    g.intersection(footprint).buffer(width / 2, cap_style="flat")
                    for g, width in lanes[level]
                    if g.intersects(footprint)
                ]
            ).intersection(footprint)
            matching = footprint.buffer(2e-6)
            walks = shapely.union_all(
                [
                    *pedestrian[level],
                    *(area for area in pedestrian[None] if matching.covers(area)),
                ]
            ).intersection(footprint)
            yield WaterCrossing(footprint, span, paved, level, pedestrian_paving=walks)


def draw_water_crossings(
    meshes, crossings, water, height, approaches=(), *, walk_edges=None
):
    covered = defaultdict(list)
    for crossing in crossings:
        footprint, span = crossing.footprint, crossing.span
        pavement = shapely.union_all(
            [crossing.pavement, *(a.deck_contact for a in approaches if a.span is span)]
        )
        thickness = crossing.thickness
        slab(
            meshes,
            "bridge deck",
            footprint,
            span.height,
            thickness,
            paved=pavement,
            pedestrian=crossing.pedestrian_paving,
        )
        access = () if walk_edges is None else walk_edges.get(id(span), ())
        if crossing.bank_contact is not None:
            access = (*access, crossing.bank_contact)
        if opens_deck_side(access, span) or not draw_open_truss(
            meshes, footprint, span
        ):
            edge_guards(
                meshes, footprint, span, access, water=water, carriageway=pavement
            )
        width = footprint.area / span.length
        intervals = int(np.ceil(span.length / max(35.0, 3 * width)))
        lateral = np.array([-span.direction[1], span.direction[0]])
        for t in np.linspace(0, span.length, intervals + 1)[1:-1]:
            xy = span.origin + t * span.direction
            if not footprint.contains(shapely.Point(xy)) or not water.contains(
                shapely.Point(xy)
            ):
                continue
            pier = shapely.LineString(
                [xy - 0.22 * width * lateral, xy + 0.22 * width * lateral]
            ).buffer(min(2.0, 0.065 * width))
            top = lambda points, deck=span, offset=thickness - 0.03: (
                deck.height(points) - offset
            )
            depth = max(1.0, float(top(xy) - height(xy[None])[0]) + 3.0)
            slab(meshes, "bridge deck", pier, top, depth)
        covered[crossing.layer].append(footprint)
    return {layer: shapely.union_all(parts) for layer, parts in covered.items()}

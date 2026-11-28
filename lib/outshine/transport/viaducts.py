"""Associate bridge outlines with carried roads and fit clearance above lower routes."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROAD_WIDTH, route_area
from outshine.transport.bridges import edge_guards, slab
from outshine.transport.water_span import solve_water_span

SLAB_DEPTH = 1.0
LOWER_CLEARANCE = {"path": 2.7, "track": 2.7, "rail": 5.4, "transit": 5.4}


@dataclass(frozen=True)
class Viaduct:
    footprint: shapely.Geometry
    pavement: shapely.Geometry
    outline_layer: int
    carriageways: frozenset
    lower_routes: shapely.Geometry
    origin: np.ndarray
    direction: np.ndarray
    length: float
    elevation: float
    grade: float

    def height(self, points):
        return self.elevation + ((points - self.origin) @ self.direction) * self.grade


def footprint_axis(footprint):
    points = shapely.get_coordinates(footprint.minimum_rotated_rectangle)
    sides = np.diff(points, axis=0)
    lengths = np.linalg.norm(sides, axis=1)
    along = sides[lengths.argmax()] / lengths.max()
    if along[np.argmax(abs(along))] < 0:
        along = -along
    positions = shapely.get_coordinates(footprint) @ along
    return along, positions.min(), positions.max()


def carried_roads(footprint, along, candidates):
    for geom, tags in candidates:
        width = ROAD_WIDTH.get(tags["class"])
        if not width or tags["class"] == "path":
            continue
        for line in shapely.get_parts(geom.intersection(footprint)):
            if line.geom_type != "LineString" or line.length < 2 * width:
                continue
            start, end = shapely.get_coordinates(line)[[0, -1]]
            if abs((end - start) @ along) >= 0.9 * line.length:
                yield line, id(geom), tags, width


def clearance_surfaces(footprint, along, ends, lower, road_classes):
    for geom, tags in lower:
        kind = tags["class"]
        width = ROAD_WIDTH.get(kind, 4.0)
        overlap = route_area(geom, width).intersection(footprint)
        if overlap.area == 0:
            continue
        points = shapely.get_coordinates(shapely.segmentize(overlap, 2.0))
        stations = points @ along
        near_mouth = (
            stations.max() <= ends[0] + width or stations.min() >= ends[1] - width
        )
        if kind in road_classes and near_mouth:
            continue
        yield overlap, points, LOWER_CLEARANCE.get(kind, 4.8)


def fit_viaduct(footprint, layer, upper, lower, water, height):
    along, start, stop = footprint_axis(footprint)
    length = stop - start
    carried = list(carried_roads(footprint, along, upper))
    if not carried:
        return None
    road_classes = {tags["class"] for _, _, tags, _ in carried}
    nearby = list(
        clearance_surfaces(footprint, along, (start, stop), lower, road_classes)
    )
    if not nearby:
        return None
    # Anchor the slope at the extremities of the mapped carriageways.
    points = np.concatenate(
        [shapely.get_coordinates(line) for line, _, _, _ in carried]
    )
    stations = points @ along
    stops = [stations.argmin(), stations.argmax()]
    run = np.ptp(stations[stops])
    if run < 0.75 * length:
        return None
    end_heights = height(points[stops])
    grade = float(np.diff(end_heights)[0] / run)
    if abs(grade) > 0.06:
        return None
    centre = shapely.get_coordinates(footprint.representative_point())[0]
    origin = centre + (start - centre @ along) * along
    lift = max(
        float(np.max(height(xy) + clearance - ((xy - origin) @ along) * grade))
        for _, xy, clearance in nearby
    )
    wet = water.intersection(footprint)
    if wet.area:
        xy = shapely.get_coordinates(shapely.segmentize(wet, 2.0))
        lift = max(lift, float(np.max(height(xy) - ((xy - origin) @ along) * grade)))
    pavement = shapely.union_all(
        [route_area(line, width) for line, _, _, width in carried]
    ).intersection(footprint)
    return Viaduct(
        footprint,
        pavement,
        layer,
        frozenset(key for _, key, _, _ in carried),
        shapely.union_all([wet, *(area for area, _, _ in nearby)]),
        origin,
        along,
        length,
        lift + SLAB_DEPTH + 0.15,
        grade,
    )


def local_viaducts(features, domain, height):
    outlines, upper, lower, by_layer = [], [], [], {}
    water = shapely.union_all(
        [shapely.make_valid(geom) for name, geom, _ in features if name == "water"]
    )
    for name, geom, tags in features:
        if name != "transportation" or not geom.intersects(domain):
            continue
        kind = tags.get("class")
        if tags.get("brunnel") == "bridge":
            if kind == "bridge" and geom.geom_type.endswith("Polygon"):
                outlines.append((geom, tags.get("layer", 1)))
            elif geom.geom_type.endswith("LineString"):
                by_layer.setdefault(tags.get("layer", 1), []).append(geom)
                if kind in ROAD_WIDTH:
                    upper.append((geom, tags))
        elif (
            not tags.get("brunnel")
            and tags.get("layer", 0) == 0
            and (kind in ROAD_WIDTH or kind in {"rail", "transit"})
        ):
            lower.append((geom, tags))
    candidate_decks = []
    for geom, layer in outlines:
        for footprint in shapely.get_parts(shapely.make_valid(geom)):
            if (
                footprint.geom_type == "Polygon"
                and solve_water_span(footprint, by_layer.get(layer, ()), water, height)
                is None
            ):
                deck = fit_viaduct(footprint, layer, upper, lower, water, height)
                if deck is not None:
                    candidate_decks.append(deck)
    return candidate_decks


def elevated_outline(geometry, layer, viaducts):
    return any(
        layer == deck.outline_layer
        and geometry.intersection(deck.footprint).area >= 0.8 * geometry.area
        for deck in viaducts
    )


def draw_viaduct(meshes, deck, height, approaches=()):
    outline = shapely.orient_polygons(deck.footprint)
    pavement = shapely.union_all(
        [deck.pavement, *(a.deck_contact for a in approaches if a.span is deck)]
    )
    slab(meshes, "bridge deck", outline, deck.height, SLAB_DEPTH, paved=pavement)
    edge_guards(meshes, outline, deck)
    width = outline.area / deck.length
    divisions = int(np.ceil(deck.length / max(24.0, 0.75 * width)))
    side = deck.direction[::-1] * [1, -1]
    for distance in np.linspace(0, deck.length, divisions + 1)[1:-1]:
        axis = deck.origin + distance * deck.direction
        for position in (axis - 0.24 * width * side, axis + 0.24 * width * side):
            column = shapely.Point(position).buffer(0.95, quad_segs=2)
            if not outline.covers(column) or deck.lower_routes.intersects(column):
                continue
            depth = float((deck.height(position[None]) - height(position[None]))[0])
            if depth > SLAB_DEPTH:
                slab(
                    meshes,
                    "bridge deck",
                    column,
                    lambda xy: deck.height(xy) - SLAB_DEPTH + 0.03,
                    depth - SLAB_DEPTH + 0.23,
                )

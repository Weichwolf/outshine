"""Bounded road profiles at mapped bridge mouths."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.geometry import LineString, Point
from shapely.ops import substring

from outshine.terrain.ground_surface import ROAD_WIDTH
from outshine.transport.endpoint_grades import plan_endpoint_transition

APPROACH_REACH = 100.0
APPROACH_SEARCH = APPROACH_REACH + 20.0


@dataclass(frozen=True)
class BridgeApproach:
    footprint: object
    mouth_edge: object
    deck_contact: object
    origin: np.ndarray
    direction: np.ndarray
    span: object
    profile: object

    @property
    def retaining_edge(self):
        return self.footprint.boundary.difference(self.mouth_edge.buffer(2e-6))

    def heights(self, ground):
        station = (ground[:, :2] - self.origin) @ self.direction
        length = self.profile.length_m
        target = self.profile.evaluate(np.clip(station, 0, length))
        deck = self.span.height(ground[:, :2])
        # The surveyed deck edge can be oblique to a carriageway.
        mouth = np.clip(station / 4, 0, 1)
        mouth = mouth * mouth * (3 - 2 * mouth)
        target = deck + mouth * (target - deck)
        touches_deck = shapely.dwithin(
            shapely.points(ground[:, :2]), self.mouth_edge, 2e-6
        )
        target[touches_deck] = deck[touches_deck]
        tail = np.clip((length - station) / (length * 0.25), 0, 1)
        tail = tail * tail * (3 - 2 * tail)
        return ground[:, 2] + tail * (target - ground[:, 2])


def approach_profile(line, deck, span, terrain, width):
    max_length = min(line.length, APPROACH_REACH)
    if max_length < 18:
        return None
    for length in reversed(np.linspace(18.0, max_length, 9)):
        approach = fit_approach(line, deck, span, terrain, width, length)
        if approach is not None:
            return approach
    return None


def fit_approach(line, deck, span, terrain, width, length):
    origin = np.asarray(line.coords)[0]
    end_xy = np.asarray(line.interpolate(length).coords)[0]
    direction = end_xy - origin
    run = np.linalg.norm(direction)
    if run < 0.95 * length or abs(direction @ span.direction) < 0.87 * run:
        return None
    direction /= run
    step = 2.0
    z0, z1 = terrain(np.stack((end_xy - step * direction, end_xy + step * direction)))
    profile, _ = plan_endpoint_transition(
        float(span.height(origin)),
        float(terrain(end_xy[None])[0]),
        float(run),
        float(span.grade * (direction @ span.direction)),
        float((z1 - z0) / (2 * step)),
    )
    if profile is None:
        return None
    # Extend the strip into the deck, then use the mapped edge as the exact cut.
    path = substring(line, 0, length)
    extended = LineString([origin - width * direction, *path.coords])
    corridor = extended.buffer(width / 2, cap_style="flat", quad_segs=2)
    footprint = corridor.difference(deck)
    if footprint.geom_type != "Polygon":
        return None
    mouth_edge = corridor.intersection(deck.boundary)
    if mouth_edge.length < width / 2:
        return None
    return BridgeApproach(
        footprint,
        mouth_edge,
        corridor.intersection(deck),
        origin,
        direction,
        span,
        profile,
    )


def earthwork_meshes(ground, faces, earthwork, material, rim_height=None):
    used, remap = np.unique(faces, return_inverse=True)
    ground = ground[used]
    faces = remap.reshape(faces.shape)
    top = ground.copy()
    top[:, 2] = earthwork.heights(ground)
    yield material, top, faces
    edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    mid = top[edges, :2].mean(axis=1)
    rim = shapely.dwithin(shapely.points(mid), earthwork.retaining_edge, 2e-6)
    edges = edges[rim]
    if rim_height is not None and len(edges):
        ground[:, 2] = rim_height(ground)
    changed = (np.abs(top[edges, 2] - ground[edges, 2]) > 0.005).any(axis=1)
    edges = edges[changed]
    if len(edges):
        walls = np.concatenate((top[edges], ground[edges[:, ::-1]]), axis=1)
        faces = np.arange(walls.size // 3).reshape(-1, 4)
        yield (
            "bridge deck",
            walls.reshape(-1, 3),
            faces[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3),
        )


def bridge_approaches(spans, features, terrain, water):
    if not spans:
        return []
    roads = {}
    for _, line, props in features:
        kind = props.get("class")
        if (
            kind in ROAD_WIDTH
            and kind != "path"
            and not props.get("brunnel")
            and props.get("layer", 0) == 0
            and line.geom_type.endswith("LineString")
        ):
            roads.setdefault((kind, bool(props.get("ramp"))), []).append(line)
    result = []
    for (kind, _), lines in roads.items():
        index = shapely.STRtree(lines)
        for deck, span in spans:
            for end, outward in (
                (span.origin, -span.direction),
                (span.origin + span.length * span.direction, span.direction),
            ):
                mouth = Point(end).buffer(deck.area / span.length)
                neighboring = index.geometries[
                    index.query(Point(end).buffer(APPROACH_SEARCH))
                ]
                segments = shapely.get_parts(
                    shapely.line_merge(shapely.union_all(neighboring))
                )
                candidates = np.r_[segments, shapely.get_parts(neighboring)]
                accepted = []
                for line in sorted(
                    candidates, key=lambda line: line.length, reverse=True
                ):
                    if not mouth.intersects(line):
                        continue
                    ends = shapely.get_coordinates(shapely.get_point(line, [0, -1]))
                    distances = shapely.distance(deck.boundary, shapely.points(ends))
                    near = int(np.argmin(distances))
                    if distances[near] > 1.0 or (ends[near] - end) @ outward < -2:
                        continue
                    if near:
                        line = shapely.reverse(line)
                    test = np.asarray(line.interpolate(8).coords)[0]
                    if (test - ends[near]) @ outward < 6:
                        continue
                    approach = approach_profile(
                        line, deck, span, terrain, ROAD_WIDTH[kind]
                    )
                    if (
                        approach is not None
                        and not approach.footprint.intersects(water)
                        and not any(
                            approach.footprint.intersection(x).area > 1
                            for x in accepted
                        )
                    ):
                        accepted.append(approach.footprint)
                        result.append(approach)
    return result

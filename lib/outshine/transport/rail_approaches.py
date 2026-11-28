"""Fit dry rail approaches to the deck and surrounding terrain."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from shapely.ops import substring

from outshine.contracts import Mesh
from outshine.geometry.sampling import SurfaceSampler
from outshine.terrain.ground_surface import terrain_triangles
from outshine.transport.bridge_passages import route_sections
from outshine.transport.elevated_rail import track_centreline
from outshine.transport.endpoint_grades import plan_endpoint_transition
from outshine.transport.passage_pavement import profile_triangles
from outshine.transport.rail_hardware import build_rail_hardware

MAXIMUM_RAIL_GRADE = 0.03
MAXIMUM_GRADE_CHANGE = 0.002
MAXIMUM_BALLAST_CUT = 0.08


@dataclass(frozen=True)
class RailApproach:
    footprint: shapely.Polygon
    centreline: np.ndarray
    surface: SurfaceSampler

    @property
    def retaining_edge(self):
        return self.footprint.boundary

    @property
    def breaks(self):
        mesh = self.surface.mesh
        return shapely.get_parts(
            shapely.union_all(
                shapely.boundary(shapely.polygons(mesh.vertices[mesh.triangles, :2]))
            )
        )

    def heights(self, ground):
        z = self.surface.sample(ground[:, :2]).points_enu_m[:, 2]
        if not np.isfinite(z).all():
            raise ValueError("Unresolved rail approach surface")
        return z


def rail_neighbors(features):
    ports = defaultdict(list)
    seen = set()
    for name, geometry, tags in features:
        if (
            name != "transportation"
            or tags.get("class") != "rail"
            or tags.get("brunnel")
            or tags.get("layer")
        ):
            continue
        for line in shapely.get_parts(geometry):
            if (
                line.geom_type != "LineString"
                or line.is_empty
                or not line.is_simple
                or line.is_ring
            ):
                continue
            key = shapely.normalize(line)
            if key not in seen:
                seen.add(key)
                ports[line.coords[0]].append(line)
                ports[line.coords[-1]].append(shapely.reverse(line))
    return ports, shapely.STRtree(sorted(seen, key=lambda line: line.wkb))


def maximum_cut(mesh, terrain, height):
    xyz = mesh.vertices[mesh.triangles]
    panels = shapely.polygons(xyz[:, :, :2])
    upper, lower = terrain.query(panels, "intersects")
    points, owner = shapely.get_coordinates(
        shapely.intersection(panels[upper], terrain.geometries[lower]),
        return_index=True,
    )
    a, b, c = xyz[upper[owner]].transpose(1, 0, 2)
    normal = np.cross(b - a, c - a)
    z = a[:, 2] - ((points - a[:, :2]) * normal[:, :2]).sum(axis=1) / normal[:, 2]
    return float(np.max(height(points) - z))


def fit_rail_approach(
    line, elevation, width, extent, blocked, terrain, height, ballast_depth
):
    local = next(
        (
            part
            for part in shapely.get_parts(line.intersection(extent))
            if part.geom_type == "LineString" and part.coords[0] == line.coords[0]
        ),
        None,
    )
    if local is None:
        return None
    # Leave room for the end-grade estimate on the mapped continuation.
    reach = local.length - 6
    for run in np.arange(20, reach + 1e-6, 5):
        anchors = shapely.get_coordinates(
            shapely.line_interpolate_point(line, [run - 6, run, run + 6])
        )
        target = height(anchors)
        end_grade = float(
            np.clip(
                (target[2] - target[0]) / 12, -MAXIMUM_RAIL_GRADE, MAXIMUM_RAIL_GRADE
            )
        )
        transition, _ = plan_endpoint_transition(
            elevation,
            target[1] + ballast_depth,
            run,
            0,
            end_grade,
            grade=MAXIMUM_RAIL_GRADE,
            second_derivative=MAXIMUM_GRADE_CHANGE,
        )
        if transition is None:
            continue
        course = substring(line, 0, run)
        profile = route_sections(course, width)
        if profile is None:
            return None
        stations, sections, _, footprint = profile
        if not extent.covers(footprint) or footprint.intersection(blocked).area > 1e-6:
            return None
        z = transition.evaluate(np.minimum(stations, transition.length_m))
        vertices = np.c_[sections.reshape(-1, 2), np.repeat(z, 2)]
        mesh = Mesh(vertices, profile_triangles(len(stations), width=2)[:, ::-1])
        if maximum_cut(mesh, terrain, height) <= MAXIMUM_BALLAST_CUT:
            xy = shapely.get_coordinates(
                shapely.line_interpolate_point(course, stations)
            )
            return RailApproach(
                footprint, xy, SurfaceSampler(mesh, xy_tolerance_m=2e-6)
            )
    return None


def plan_rail_approaches(
    decks, features, extent, blocked, vertices, quads, height, width, ballast_depth
):
    if not decks:
        return {}
    ports, rails = rail_neighbors(features)
    terrain = shapely.STRtree(shapely.polygons(vertices[terrain_triangles(quads), :2]))
    result = {}
    for deck in decks:
        other_decks = shapely.union_all(
            [other.footprint for other in decks if other is not deck]
        )
        for track in deck.tracks:
            for end, neighbor in ((0, 1), (-1, -2)):
                point = track.coords[end]
                lines = ports.get(point, ())
                if len(lines) != 1:
                    continue
                line = lines[0]
                tangent = np.subtract(point, track.coords[neighbor])
                following = np.subtract(line.coords[1], point)
                if tangent @ following < 0.97 * np.linalg.norm(
                    tangent
                ) * np.linalg.norm(following):
                    continue
                occupied = shapely.union_all(
                    [
                        blocked,
                        other_decks,
                        *(
                            other.buffer(width / 2, cap_style="flat")
                            for other in rails.geometries[
                                rails.query(line, "dwithin", width)
                            ]
                            if other != shapely.normalize(line)
                        ),
                    ]
                )
                approach = fit_rail_approach(
                    line,
                    deck.elevation + ballast_depth,
                    width,
                    extent,
                    occupied,
                    terrain,
                    height,
                    ballast_depth,
                )
                if approach is None:
                    continue
                start = approach if end == 0 else result.get((track, 0))
                finish = approach if end == -1 else None
                hardware, _ = build_rail_hardware(
                    track_centreline(track, start, finish),
                    deck.elevation + ballast_depth,
                )
                if hardware is not None:
                    result[(track, end)] = approach
                    blocked = blocked.union(approach.footprint)
    return result

"""Join mapped stair landings, bridge footways and their at-grade branches."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from bridge_junctions import patch_mesh
from bridge_passages import route_sections
from ground_surface import at_grade_width, path_material
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget, fit_surface_heights
from passage_pavement import MAXIMUM_GRADE, GroundSections
from shapely.ops import substring


@dataclass(frozen=True)
class DeckWalkLink:
    footprint: shapely.Geometry
    mesh: Mesh
    ground: np.ndarray
    material: str
    stair_edge: shapely.Geometry
    deck_edge: shapely.Geometry
    ground_edges: tuple
    source: shapely.Geometry
    span_id: int

    def retaining_sections(self):
        contacts = shapely.union_all(
            [self.stair_edge, self.deck_edge, *self.ground_edges]
        )
        rim = self.footprint.boundary.difference(contacts, grid_size=1e-6)
        edges = self.mesh.triangles[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
        points = self.mesh.vertices[edges]
        cut = shapely.dwithin(shapely.points(points[:, :, :2].mean(axis=1)), rim, 2e-6)
        cut &= (abs(points[:, :, 2] - self.ground[edges]) > 0.005).any(axis=1)
        return points[cut], self.ground[edges[cut]]


def mapped_links(transport, stairs, crossings):
    ground_paths, deck_ports = defaultdict(list), defaultdict(set)
    for geom, tags in transport:
        if tags.get("class") != "path" or tags.get("subclass") != "footway":
            continue
        for line in shapely.get_parts(geom):
            if line.geom_type != "LineString" or line.is_empty:
                continue
            if tags.get("brunnel") == "bridge":
                layer = tags.get("layer", 1)
                for end in line.coords[0], line.coords[-1]:
                    deck_ports[end].add(layer)
            elif width := at_grade_width(tags):
                for point in set(line.coords):
                    ground_paths[point].append((line, width, path_material(tags)))
    for stair in stairs:
        for port in stair.ports:
            for line, width, material in ground_paths[port.point]:
                if port.point == line.coords[-1]:
                    line = shapely.reverse(line)
                if port.point != line.coords[0] or line.length > 24:
                    continue
                end = shapely.Point(line.coords[-1])
                eligible = [
                    c
                    for c in crossings
                    if c.layer in deck_ports[end.coords[0]] and c.footprint.covers(end)
                ]
                if len(eligible) != 1:
                    continue
                branches = [
                    (point, branch, w, m)
                    for point in line.coords[1:-1]
                    for branch, w, m in ground_paths[point]
                    if not branch.equals(line)
                ]
                if any(m != material for _, _, _, m in branches):
                    continue
                yield stair, port, eligible[0], line, width, material, branches


def stair_outline(line, width, port):
    xy = np.asarray(line.coords)
    forward = xy[-1] - xy[-2]
    path = shapely.LineString(
        np.vstack((xy, xy[-1] + width * forward / np.linalg.norm(forward)))
    )
    layout = route_sections(path, width)
    if layout is None:
        return None
    _, sections, _, _ = layout
    edge = port.edge
    if (edge[1] - edge[0]) @ (sections[0, 1] - sections[0, 0]) < 0:
        edge = edge[::-1]
    sections[0] = edge
    outline = shapely.Polygon(np.vstack((sections[:, 0], sections[::-1, 1])))
    return outline if outline.is_valid else None


def branch_bands(branches, reach):
    bands, ends = [], []
    for point, line, width, _ in branches:
        station = line.project(shapely.Point(point))
        start, end = max(0, station - reach), min(line.length, station + reach)
        band = substring(line, start, end)
        layout = route_sections(band, width)
        if layout is None:
            return None
        _, sections, _, footprint = layout
        bands.append(footprint)
        for length, edge in (
            (station - start, sections[0]),
            (end - station, sections[-1]),
        ):
            if length > 2e-6:
                ends.append(shapely.LineString(edge))
    return bands, ends


def fit_deck_link(footprint, ground, lattice, port, deck, stair_edge, deck_edge, ends):
    mesh = patch_mesh(
        footprint, ground.heights, lattice, 0.72, breaks=ground.breaks_in(footprint)
    )
    xy = mesh.vertices[:, :2]
    points = shapely.points(xy)
    pins = {}
    for edge, values in [
        (stair_edge, np.full(len(xy), port.elevation)),
        (deck_edge, deck.span.height(xy)),
        *((end, mesh.vertices[:, 2]) for end in ends),
    ]:
        on = np.flatnonzero(shapely.dwithin(points, edge, 2e-6))
        if len(on) < 2:
            return None
        pins.update((int(i), values[i]) for i in on)
    frames = np.broadcast_to(np.eye(2) / MAXIMUM_GRADE, (len(mesh.triangles), 2, 2))
    fitted = fit_surface_heights(
        mesh, frames, fixed=pins, budget=HeightBudget(time_limit_s=1)
    ).mesh
    return None if fitted is None else (fitted, mesh.vertices[:, 2])


def deck_walk_links(
    transport, stairs, crossings, protected, vertices, quads, terrain, junctions
):
    if not stairs or not crossings:
        return []
    links = []
    ground = None
    stair_area = shapely.union_all([s.footprint for s in stairs])
    for stair, port, deck, line, width, material, branches in mapped_links(
        transport, stairs, crossings
    ):
        outline = stair_outline(line, width, port)
        if outline is None:
            continue
        stair_edge = shapely.LineString(port.edge)
        for reach in (3, 5, 8, 12) if branches else (0,):
            layout = branch_bands(branches, reach)
            if layout is None:
                continue
            bands, ends = layout
            footprint = shapely.union_all([outline, *bands], grid_size=1e-6).difference(
                deck.footprint.union(stair.footprint), grid_size=1e-6
            )
            if (
                footprint.geom_type != "Polygon"
                or footprint.intersection(protected.union(stair_area)).area > 1e-5
            ):
                continue
            deck_edge = footprint.boundary.intersection(
                deck.footprint.boundary, grid_size=1e-6
            )
            boundary = footprint.boundary.buffer(2e-6)
            if deck_edge.length < width / 2 or not all(
                boundary.covers(edge) for edge in (stair_edge, *ends)
            ):
                continue
            if ground is None:
                ground = GroundSections(vertices, quads, terrain, junctions)
            fitted = fit_deck_link(
                footprint,
                ground,
                vertices[0, :2],
                port,
                deck,
                stair_edge,
                deck_edge,
                ends,
            )
            if fitted is None:
                continue
            mesh, ground_z = fitted
            links.append(
                DeckWalkLink(
                    footprint,
                    mesh,
                    ground_z,
                    material,
                    stair_edge,
                    deck_edge,
                    tuple(ends),
                    shapely.normalize(line),
                    id(deck.span),
                )
            )
            protected = protected.union(footprint, grid_size=1e-6)
            break
    return links

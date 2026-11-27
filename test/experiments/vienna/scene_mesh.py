"""Assemble source geometry in local east/north/up metres for Blender."""

import itertools
from dataclasses import replace
from functools import partial

import numpy as np
import shapely
from bridge_approach import earthwork_meshes
from bridge_banks import BANK_REACH_M, TRANSPORT_MARGIN_M, bank_rim, bank_slopes
from bridge_contacts import bank_cuts
from bridge_junctions import junction_pavement
from bridge_passages import bridge_terrain, water_level
from bridges import draw_water_crossings, plan_water_crossings
from ground_surface import (
    at_grade_routes,
    at_grade_width,
    partition_ground,
    partition_triangles,
    terrain_triangles,
)
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface
from outshine_ref.buildings.structures import suppressed
from outshine_ref.water.geodetic import local_points
from shapely.geometry import Point, box
from stair_branches import local_stair_branches
from stair_guards import draw_stair_guards
from stair_landings import local_landings
from stairs import local_stairways
from tram_rails import draw_tram_rails, tram_routes
from viaducts import draw_viaduct


class Meshes(dict):
    def add(self, name, vertices, faces):
        points, triangles = self.setdefault(name, ([], []))
        triangles.extend((np.asarray(faces) + len(points)).tolist())
        points.extend(np.asarray(vertices).tolist())


def polygons(geometry):
    if geometry.geom_type == "Polygon" and not geometry.is_empty:
        yield shapely.orient_polygons(geometry)
    elif hasattr(geometry, "geoms"):
        for part in geometry.geoms:
            yield from polygons(part)


def cap(meshes, name, polygon, elevation):
    triangles = shapely.orient_polygons(
        shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    )
    if not len(triangles):
        return
    xy = np.array([t.exterior.coords[:3] for t in triangles]).reshape(-1, 2)
    z = np.broadcast_to(elevation(xy), len(xy))
    meshes.add(name, np.column_stack((xy, z)), np.arange(len(xy)).reshape(-1, 3))


def building(meshes, polygon, properties, source):
    floor = properties.get("render_min_height", 0)
    roof = properties.get("render_height", 0)
    if roof <= floor or suppressed(properties):
        return
    base = float(source.height(np.asarray(polygon.representative_point().coords))[0])
    cap(meshes, "roof", polygon, lambda xy: roof + base)
    for ring in [polygon.exterior, *polygon.interiors]:
        corners = np.asarray(ring.coords)
        for a, b in itertools.pairwise(corners):
            span = b - a
            length = np.linalg.norm(span)
            if length < 0.01:
                continue
            height = roof - floor
            if np.linalg.norm((a + b) / 2) < 85 and length > 2 and height > 2:
                repeat = FacadeRepeat(
                    length,
                    height,
                    max(1, round(length / 3.4)),
                    max(1, round(height / 3.2)),
                    0.47,
                    0.57,
                )
                surface = recess_surface(RecessPlan(repeat))
                u, v, n = surface["vertices"].T
                axis = span / length
                xy = a + u[:, None] * axis + n[:, None] * [axis[1], -axis[0]]
                vertices = np.column_stack((xy, base + floor + v))
                for role, name in enumerate(("wall", "glass", "frame")):
                    faces = surface["faces"][surface["roles"] == role]
                    used, inverse = np.unique(faces, return_inverse=True)
                    meshes.add(name, vertices[used], inverse.reshape(-1, 3))
            else:
                xy = np.array([a, b, b, a])
                meshes.add(
                    "wall",
                    np.column_stack((xy, base + np.array([floor, floor, roof, roof]))),
                    [(0, 1, 2), (0, 2, 3)],
                )


def terrain_grid(source, radius):
    axis = np.linspace(-radius, radius, 65)
    x, y = np.meshgrid(axis, axis)
    xy = np.column_stack((x.ravel(), y.ravel()))
    cell = np.arange(len(xy)).reshape(x.shape)[:-1, :-1].ravel()
    quads = cell[:, None] + [0, 1, len(axis) + 1, len(axis)]
    return np.column_stack((xy, source.height(xy))), quads


def terrain_height(vertices, points):
    size = int(np.sqrt(len(vertices)))
    spacing = vertices[1, 0] - vertices[0, 0]
    grid = (points - vertices[0, :2]) / spacing
    cell = np.clip(grid.astype(int), 0, size - 2)
    x, y = (grid - cell).T
    first = cell[:, 0] + cell[:, 1] * size
    a, b, c, d = vertices[first[:, None] + [0, 1, size + 1, size], 2].T
    return a + np.where(x >= y, x * (b - a) + y * (c - b), y * (d - a) + x * (c - d))


def boundary_edges(edges, boundary):
    xy = edges[:, :, :2]
    on_boundary = shapely.dwithin(
        shapely.points(xy.reshape(-1, 2)), boundary, 1e-6
    ).reshape(-1, 2).all(axis=1) & shapely.dwithin(
        shapely.points(xy.mean(axis=1)), boundary, 1e-6
    )
    return edges[on_boundary]


def vertical_faces(meshes, name, edges, heights):
    if len(edges):
        other = edges.copy()
        other[:, :, 2] = heights
        walls = np.concatenate((edges, other), axis=1).reshape(-1, 3)
        faces = np.arange(len(edges))[:, None] * 4 + [0, 2, 1, 1, 2, 3]
        meshes.add(name, walls, faces.reshape(-1, 3))


def staircase_risers(meshes, stairs):
    for stair in stairs:
        changes = np.flatnonzero(np.diff(stair.heights))
        edges = shapely.intersection(stair.treads[changes], stair.treads[changes + 1])
        for i, edge in zip(changes, edges, strict=True):
            xy = shapely.get_coordinates(edge)
            if len(xy) == 2:
                face = np.column_stack((xy, np.full(2, stair.heights[i])))
                vertical_faces(meshes, "sidewalk", face[None], stair.heights[i + 1])


def water_surfaces(meshes, source, bodies, vertices, banks):
    xy = vertices[:, :2]
    for body in polygons(bodies):
        level = water_level(body, xy, source.height) + source.datum

        def water_height(points, height=level):
            lonlat = points / source.metres + [source.lon, source.lat]
            return (
                local_points((source.lon, source.lat), lonlat, height)[:, 2]
                - source.datum
            )

        cap(meshes, "water", body, water_height)
        if not len(banks):
            continue
        top = boundary_edges(banks, body.boundary)
        shore = top[:, :, :2].reshape(-1, 2)
        vertical_faces(meshes, "ground", top, water_height(shore).reshape(-1, 2) - 0.1)


def compound_ground(vertices, quads, junctions, footprint):
    if not junctions:
        return vertices, terrain_triangles(quads)
    points, faces = [], []
    offset = 0
    parts = partition_ground(
        vertices,
        quads,
        np.zeros(len(quads), dtype=int),
        {0: "land", 1: "junction"},
        {1: footprint},
    )
    for name, xyz, indices in parts:
        if name == "land":
            used, inverse = np.unique(indices, return_inverse=True)
            points.append(xyz[used])
            faces.append(offset + inverse.reshape(-1, 3))
            offset += len(used)
    for junction in junctions:
        points.append(junction.mesh.vertices)
        faces.append(offset + junction.mesh.triangles)
        offset += len(junction.mesh.vertices)
    return np.concatenate(points), np.concatenate(faces)


def bank_protection(source, extent, bodies, openings, roads, crossings, fixtures):
    interest = openings.buffer(BANK_REACH_M + TRANSPORT_MARGIN_M).intersection(extent)
    shapely.prepare(interest)
    buildings = [
        shapely.make_valid(geom).intersection(interest)
        for layer, geom, _ in source.features
        if layer == "building" and interest.intersects(geom)
    ]
    decks = shapely.union_all([crossing.footprint for crossing in crossings])
    tracks = [
        geom.intersection(interest).buffer(1.2, cap_style="flat").difference(decks)
        for layer, geom, tags in source.features
        if layer == "transportation"
        and tags.get("class") in {"rail", "transit"}
        and not tags.get("brunnel")
        and tags.get("layer", 0) == 0
        and interest.intersects(geom)
    ]
    land_routes = roads["road"].difference(decks)
    paths = roads["sidewalk"].union(roads["paving"]).difference(openings.buffer(0.4))
    occupied = shapely.union_all(
        [
            land_routes.intersection(interest),
            paths.intersection(interest),
            *buildings,
            *tracks,
            *(fixture.footprint.intersection(interest) for fixture in fixtures),
        ]
    )
    return occupied.buffer(TRANSPORT_MARGIN_M).union(bodies)


def ground_surfaces(
    meshes, source, extent, bodies, vertices, quads, approaches=(), crossings=()
):
    transport = [(g, p) for layer, g, p in source.features if layer == "transportation"]
    roads = at_grade_routes(transport, extent)
    height = lambda points: terrain_height(vertices, points)
    stairs = local_stairways(source.features, extent, bodies, height)
    branches, aprons, replaced_paths = local_stair_branches(
        source.features, stairs, extent, bodies.union(roads["road"]), height
    )
    stairs.extend(branches)
    original = crossings
    crossings, junctions, joined_spans, passages = bridge_terrain(
        source,
        crossings,
        extent,
        bodies,
        roads["road"],
        vertices,
        quads,
        height,
        stairs,
    )
    openings = shapely.union_all([passage.footprint for passage in passages])
    junction_area = shapely.union_all([j.footprint for j in junctions])
    approaches = [
        a
        for a in approaches
        if not any(
            a.span is old.span and any(new.span is span for span in joined_spans)
            for old, new in zip(original, crossings, strict=True)
        )
    ]
    if replaced_paths:
        roads = at_grade_routes(transport, extent, replaced_paths=replaced_paths)
    carried = junction_pavement(transport, crossings, junctions, joined_spans, roads)
    routes = tram_routes(
        source.features,
        [c for c in crossings if any(c.span is span for span in joined_spans)],
    )
    track_paving = shapely.union_all(
        [line.buffer(1.05, cap_style="flat") for _, line in routes]
    )
    if routes:
        pavement = track_paving.intersection(junction_area, grid_size=1e-6)
        roads["road"] = roads["road"].union(pavement, grid_size=1e-6)
    landings, joined_edges = local_landings(
        source.features, stairs, extent, bodies, height
    )
    bank_grades = []
    if passages:
        bank_grades = bank_slopes(
            passages,
            bodies,
            bank_protection(
                source,
                extent,
                bodies,
                openings,
                roads,
                crossings,
                [*stairs, *landings, *aprons, *approaches],
            ),
            extent,
        )
    pieces = [
        (foot, z, stair.material)
        for stair in stairs
        for foot, z in zip(stair.treads, stair.heights, strict=True)
    ]
    names = dict(enumerate(("ground", "water", "road", "sidewalk", "paving")))
    first_stair = len(names)
    stair_tops = {
        i: (z, name) for i, (_, z, name) in enumerate(pieces, start=first_stair)
    }
    earthworks = (
        [(passage, passage.route.material) for passage in passages]
        + [(landing, landing.material) for landing in [*landings, *aprons]]
        + [(a, "road") for a in approaches if a.footprint.intersects(extent)]
        + [(slope, "ground") for slope in bank_grades]
        + [
            (cut, "ground")
            for cut in bank_cuts(
                vertices,
                quads,
                crossings,
                extent,
                joined_ground=junction_area.buffer(2e-6) if junctions else None,
            )
        ]
    )
    graded = {
        i: item for i, item in enumerate(earthworks, start=first_stair + len(pieces))
    }
    passage_ports = [
        contact.opening
        for passage in passages
        for contact in passage.route.stair_contacts
    ]
    stair_limit = shapely.LineString()
    if stairs:
        perimeter = shapely.union_all([stair.footprint for stair in stairs]).boundary
        contacts = shapely.union_all([joined_edges, *(apron.edge for apron in aprons)])
        stair_limit = perimeter.difference(contacts, grid_size=1e-6)
        if passage_ports:
            stair_limit = stair_limit.difference(
                shapely.union_all(passage_ports), grid_size=1e-6
            )
    names.update({i: i for i in stair_tops.keys() | graded.keys()})
    footprints = {1: bodies}
    footprints.update(
        {i: foot for i, (foot, _, _) in enumerate(pieces, start=first_stair)}
    )
    footprints.update(
        {i: work.footprint.intersection(extent) for i, (work, _) in graded.items()}
    )
    footprints.update({2: roads["road"], 3: roads["sidewalk"], 4: roads["paving"]})
    banks = np.empty((0, 2, 3))
    surface, indices = compound_ground(vertices, quads, junctions, junction_area)
    for name, points, faces in partition_triangles(
        surface,
        indices,
        np.zeros(len(indices), dtype=int),
        names,
        footprints,
        breaks=[edge for shape in [*passages, *bank_grades] for edge in shape.breaks],
    ):
        if name in graded:
            earthwork, ground_material = graded[name]
            slopes = [slope for slope in bank_grades if slope.passage is earthwork]
            rim = partial(bank_rim, slopes=slopes) if slopes else None
            for material, graded_vertices, indices in earthwork_meshes(
                points, faces, earthwork, ground_material, rim
            ):
                meshes.add(material, graded_vertices, indices)
            continue
        if name == "water" or name in stair_tops:
            edges = points[faces][:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2, 3)
            if name == "water":
                banks = np.concatenate((banks, edges))
                continue
            height, name = stair_tops[name]
            vertical_faces(
                meshes, "sidewalk", boundary_edges(edges, stair_limit), height
            )
            points = points.copy()
            points[:, 2] = height
        if len(faces):
            used, inverse = np.unique(faces, return_inverse=True)
            meshes.add(name, points[used], inverse.reshape(-1, 3))
    staircase_risers(meshes, stairs)
    draw_stair_guards(meshes, stairs, [*landings, *aprons], passage_ports)
    water_surfaces(meshes, source, bodies, vertices, banks)
    draw_tram_rails(meshes, routes, junctions)
    return approaches, carried, track_paving, crossings, openings


def assemble(source, radius, viaducts=(), approaches=(), *, crossings=None):
    meshes = Meshes()
    extent = box(-radius, -radius, radius, radius)
    vertices, quads = terrain_grid(source, radius)
    bodies = shapely.union_all(
        [
            shapely.make_valid(g).intersection(extent)
            for layer, g, _ in source.features
            if layer == "water"
        ]
    )
    if crossings is None:
        crossings = list(
            plan_water_crossings(source.features, extent, bodies, source.height)
        )
    approaches, carried, track_paving, crossings, openings = ground_surfaces(
        meshes, source, extent, bodies, vertices, quads, approaches, crossings
    )
    if not track_paving.is_empty:
        crossings = [
            replace(
                c, pavement=c.pavement.union(track_paving.intersection(c.footprint))
            )
            for c in crossings
        ]
    bridges = draw_water_crossings(meshes, crossings, bodies, source.height, approaches)
    for viaduct in viaducts:
        draw_viaduct(meshes, viaduct, source.height, approaches)
    for name, geom, prop in source.features:
        geometry = shapely.make_valid(geom) if not geom.is_valid else geom
        if name == "building":
            for poly in polygons(geometry):
                if poly.distance(Point(0, 0)) <= radius:
                    building(meshes, poly, prop, source)
        elif name == "transportation" and prop.get("brunnel") != "tunnel":
            if at_grade_width(prop) is not None or id(geom) in carried:
                continue
            material = "sidewalk" if prop.get("class") == "path" else "road"
            clipped = geometry.intersection(extent)
            if "LineString" in clipped.geom_type:
                width = {"sidewalk": 1.6, "road": 5.5}[material]
                if prop.get("class") in ("transit", "rail"):
                    continue
                clipped = clipped.buffer(width / 2, cap_style="flat")
            if prop.get("brunnel") == "bridge" and prop.get("layer", 1) in bridges:
                clipped = clipped.difference(bridges[prop.get("layer", 1)])
            if prop.get("brunnel") == "bridge":
                for viaduct in viaducts:
                    is_outline = (
                        prop.get("class") == "bridge"
                        and prop.get("layer", 1) == viaduct.outline_layer
                    )
                    if is_outline or id(geom) in viaduct.carriageways:
                        clipped = clipped.difference(viaduct.footprint)
            if not openings.is_empty:
                clipped = clipped.difference(openings)
            for poly in polygons(clipped):
                cap(
                    meshes,
                    material,
                    poly,
                    lambda xy, rise=0.13 if material == "sidewalk" else 0.035: (
                        source.height(xy) + rise
                    ),
                )
    return meshes

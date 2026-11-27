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
from bridge_passages import bridge_terrain
from bridges import draw_water_crossings, plan_water_crossings
from building_foundations import BuildingStamps, building_foundations
from building_passages import building_passages, vault_panels
from deck_walk_links import DeckWalkLink, deck_walk_links
from ground_surface import (
    ROUTE_MATERIALS,
    at_grade_routes,
    at_grade_width,
    grassland_material,
    partition_ground,
    partition_triangles,
    polygons,
    terrain_triangles,
)
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface
from outshine_ref.buildings.structures import suppressed
from passage_trim import draw_passage_trim
from shapely.geometry import Point, box
from shore_grade import SHORE_REACH_M, shore_grade
from stair_branches import local_stair_branches
from stair_guards import draw_stair_guards
from stair_landings import local_landings
from stairs import local_stairways
from tram_rails import draw_tram_rails, tram_routes
from viaducts import draw_viaduct
from water_surface import clipped_water, water_bodies


class Meshes(dict):
    shore = None

    def add(self, name, vertices, faces):
        points, triangles = self.setdefault(name, ([], []))
        triangles.extend((np.asarray(faces) + len(points)).tolist())
        points.extend(np.asarray(vertices).tolist())


def cap(meshes, name, polygon, elevation, *, underside=False):
    triangles = shapely.orient_polygons(
        shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    )
    if not len(triangles):
        return
    xy = np.array([t.exterior.coords[:3] for t in triangles]).reshape(-1, 2)
    z = np.broadcast_to(elevation(xy), len(xy))
    faces = np.arange(len(xy)).reshape(-1, 3)
    meshes.add(name, np.column_stack((xy, z)), faces[:, ::-1] if underside else faces)


def building(meshes, polygon, properties, source, foundation, passages, grade):
    floor = properties.get("render_min_height", 0)
    roof = properties.get("render_height", 0)
    if roof <= floor or suppressed(properties):
        return
    base = (
        foundation.seat_m
        if foundation
        else float(source.height(np.asarray(polygon.representative_point().coords))[0])
    )
    cap(meshes, "roof", polygon, lambda xy: roof + base)
    if passages:
        soffit = max(item.crown_m for item in passages)
        corridor = shapely.union_all([item.corridor for item in passages])
        passage_ceilings(meshes, polygon, passages, corridor, soffit, grade)
        for pier in polygons(polygon.difference(corridor)):
            building_walls(meshes, pier, base + floor, soffit, foundation, bays=False)
        building_walls(meshes, polygon, soffit, base + roof)
    else:
        building_walls(meshes, polygon, base + floor, base + roof, foundation)


def passage_ceilings(meshes, polygon, passages, corridor, storey_top, grade):
    if sum(item.corridor.area for item in passages) - corridor.area > 1e-6:
        for cut in polygons(polygon.intersection(corridor)):
            cap(meshes, "wall", cut, lambda xy: storey_top, underside=True)
        return
    for item in passages:
        cut = polygon.intersection(item.corridor)
        panels = list(vault_panels(item, cut, item.crown_m))
        if panels:
            draw_passage_trim(meshes, polygon, item, grade)
        if not panels:
            for patch in polygons(cut):
                passage_ceiling(
                    meshes,
                    patch,
                    polygon.boundary,
                    storey_top,
                    lambda xy, z=item.crown_m: np.full(xy.shape[:-1], z),
                )
        for piece, normal, low, offset, slope in panels:

            def elevation(xy, low=low, normal=normal, offset=offset, slope=slope):
                return low + (xy @ normal - offset) * slope

            for panel in polygons(piece):
                passage_ceiling(meshes, panel, polygon.boundary, storey_top, elevation)


def passage_ceiling(meshes, panel, boundary, storey_top, elevation):
    cap(meshes, "wall", panel, elevation, underside=True)
    for ring in [panel.exterior, *panel.interiors]:
        xy = np.asarray(ring.coords)
        top = np.column_stack((xy, np.full(len(xy), storey_top)))
        edges = boundary_edges(np.stack((top[:-1], top[1:]), axis=1), boundary)
        vertical_faces(meshes, "wall", edges, elevation(edges[:, :, :2]))


def building_walls(meshes, polygon, bottom, top, foundation=None, *, bays=True):
    for ring in [polygon.exterior, *polygon.interiors]:
        corners = np.asarray(ring.coords)
        if foundation:
            support = np.column_stack((corners, np.full(len(corners), bottom)))
            edges = np.stack((support[:-1], support[1:]), axis=1)
            vertical_faces(meshes, "foundation", edges, foundation.sole_m)
        for a, b in itertools.pairwise(corners):
            span = b - a
            length = np.linalg.norm(span)
            if length < 0.01:
                continue
            height = top - bottom
            if bays and np.linalg.norm((a + b) / 2) < 85 and length > 2 and height > 2:
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
                vertices = np.column_stack((xy, bottom + v))
                for role, name in enumerate(("wall", "glass", "frame")):
                    faces = surface["faces"][surface["roles"] == role]
                    used, inverse = np.unique(faces, return_inverse=True)
                    meshes.add(name, vertices[used], inverse.reshape(-1, 3))
            else:
                xy = np.array([a, b, b, a])
                meshes.add(
                    "wall",
                    np.column_stack((xy, [bottom, bottom, top, top])),
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
    shapely.prepare(boundary)
    midpoint = shapely.dwithin(boundary, shapely.points(xy.mean(axis=1)), 1e-6)
    endpoints = shapely.dwithin(
        boundary, shapely.points(xy[midpoint].reshape(-1, 2)), 1e-6
    ).reshape(-1, 2)
    return edges[midpoint][endpoints.all(axis=1)]


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


def retained_wall_edges(body, source, edges):
    if body.control is None or body.control.retaining_limits_m is None:
        return np.zeros(len(edges), dtype=bool)
    strength, _ = body.control.retaining_profile(source, edges[:, :, :2].mean(axis=1))
    return strength > 0.02


def dry_shore_edges(shore, vertices, faces):
    triangles = vertices[faces]
    a, b, c = triangles[:, :, :2].transpose(1, 0, 2)
    u, v = b - a, c - a
    up = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] > 1e-12
    sides = triangles[up][:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2, 3)
    return shore.shoreline_edges(sides)


def shore_walls(meshes, material, source, shore, banks, *, dry_banks=None):
    banks = shore.shoreline_edges(banks)
    if not len(banks):
        return
    for body, contact in zip(shore.bodies, shore.contacts, strict=True):
        edge = boundary_edges(banks, contact)
        if not len(edge):
            continue
        points = shore.deform(source, edge.reshape(-1, 3))
        level = body.heights(source, points[:, :2]).reshape(-1, 2)
        top = points.reshape(-1, 2, 3)
        wall = (abs(top[:, :, 2] - level) > 0.005).any(axis=1)
        retained = retained_wall_edges(body, source, top)
        natural = wall & ~retained
        vertical_faces(meshes, material, top[natural], level[natural] - 0.005)
        if dry_banks is None:
            wet_side = wall & retained
            vertical_faces(
                meshes, "retaining wall", top[wet_side], level[wet_side] - 0.005
            )
            continue
        if body.control is None or body.control.retaining_limits_m is None:
            continue
        dry = boundary_edges(dry_banks, contact)
        dry = dry[retained_wall_edges(body, source, dry)]
        water = body.heights(source, dry.reshape(-1, 3)[:, :2]).reshape(-1, 2)
        exposed = (dry[:, :, 2] > water + 0.005).any(axis=1)
        vertical_faces(meshes, "retaining wall", dry[exposed], water[exposed] - 0.005)


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
    paths = shapely.union_all(
        [shape for name, shape in roads.items() if name != "road"]
    ).difference(openings.buffer(0.4))
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
    meshes,
    source,
    extent,
    bodies,
    vertices,
    quads,
    approaches=(),
    crossings=(),
    *,
    water=(),
    water_plan=(),
    foundations=(),
    covered_walks=(),
):
    transport = [(g, p) for layer, g, p in source.features if layer == "transportation"]
    paved = [*transport, *((p.corridor, p.properties) for p in covered_walks)]
    roads = at_grade_routes(paved, extent)
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
        roads,
        vertices,
        quads,
        height,
        stairs,
        [(footprint, body) for body, footprint in water],
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
        roads = at_grade_routes(paved, extent, replaced_paths=replaced_paths)
    routes = tram_routes(
        source.features,
        [c for c in crossings if any(c.span is span for span in joined_spans)],
    )
    track_paving = shapely.union_all(
        [line.buffer(1.05, cap_style="flat") for _, line in routes]
    )
    protected = (
        shapely.union_all(
            [
                bodies,
                roads["road"],
                track_paving,
                openings,
                *(c.footprint for c in crossings),
                *(item.footprint for item in [*aprons, *approaches]),
            ]
        )
        if stairs and joined_spans
        else shapely.Polygon()
    )
    links = deck_walk_links(
        transport,
        stairs,
        [c for c in crossings if any(c.span is span for span in joined_spans)],
        protected,
        vertices,
        quads,
        height,
        junctions,
    )
    if links:
        replaced_paths.update(link.source for link in links)
        roads = at_grade_routes(paved, extent, replaced_paths=replaced_paths)
    landings, joined_edges = local_landings(
        source.features,
        stairs,
        extent,
        bodies,
        height,
        reserved=shapely.union_all([link.footprint for link in links])
        if links
        else None,
    )
    carried = junction_pavement(transport, crossings, junctions, joined_spans, roads)
    if routes:
        pavement = track_paving.intersection(junction_area, grid_size=1e-6)
        roads["road"] = roads["road"].union(pavement, grid_size=1e-6)
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
                [*stairs, *landings, *aprons, *approaches, *links],
            ),
            extent,
        )
    pieces = [
        (foot, z, stair.material)
        for stair in stairs
        for foot, z in zip(stair.treads, stair.heights, strict=True)
    ]
    names = dict(enumerate(("ground", "water", *ROUTE_MATERIALS)))
    first_stair = len(names)
    stair_tops = {
        i: (z, name) for i, (_, z, name) in enumerate(pieces, start=first_stair)
    }
    earthworks = (
        [(passage, passage.route.material) for passage in passages]
        + [(link, link.material) for link in links]
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
                approaches=approaches,
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
    deck_stair_edges = [link.stair_edge for link in links]
    stair_limit = shapely.LineString()
    if stairs:
        perimeter = shapely.union_all([stair.footprint for stair in stairs]).boundary
        contacts = shapely.union_all(
            [joined_edges, *(apron.edge for apron in aprons), *deck_stair_edges]
        )
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
    footprints.update({i: roads[name] for i, name in enumerate(ROUTE_MATERIALS, 2)})
    protected = shapely.Polygon()
    if water_plan:
        margin = bodies.buffer(SHORE_REACH_M, quad_segs=2)
        shapely.prepare(margin)
        protected = shapely.union_all(
            [
                track_paving,
                *(c.footprint for c in crossings),
                *(
                    geom
                    for key, geom in footprints.items()
                    if key != 1 and margin.intersects(geom)
                ),
                *(
                    shapely.make_valid(geom)
                    for name, geom, _ in source.features
                    if name == "building" and margin.intersects(geom)
                ),
            ],
            grid_size=1e-6,
        )
    shore = shore_grade(water_plan, extent, protected)
    meshes.shore = shore
    banks = np.empty((0, 2, 3))
    dry_banks = []
    retain = any(
        body.control is not None and body.control.retaining_limits_m is not None
        for body in shore.bodies
    )
    surface, indices = compound_ground(vertices, quads, junctions, junction_area)
    building_stamps = None
    breaks = [
        *shore.breaks(bodies, extent),
        *(edge for shape in [*passages, *bank_grades] for edge in shape.breaks),
    ]
    if foundations:
        reserved = shapely.union_all(
            [*footprints.values(), junction_area, extent.boundary]
        )
        building_stamps = BuildingStamps(foundations, reserved)
        breaks.extend(building_stamps.breaks())
    grassland = {"meadow": [], "grass": []}
    for layer, geometry, properties in source.features:
        if layer == "landcover" and (material := grassland_material(properties)):
            grassland[material].extend(polygons(shapely.make_valid(geometry)))
    cover_start = first_stair + len(pieces) + len(earthworks)
    for label, (material, areas) in enumerate(grassland.items(), cover_start):
        cover = shapely.union_all(areas).intersection(extent)
        if not cover.is_empty:
            names[label] = material
            footprints[label] = cover
    for name, points, faces in partition_triangles(
        surface,
        indices,
        np.zeros(len(indices), dtype=int),
        names,
        footprints,
        breaks=breaks,
    ):
        if name in graded:
            earthwork, ground_material = graded[name]
            if isinstance(earthwork, DeckWalkLink):
                continue
            slopes = [slope for slope in bank_grades if slope.passage is earthwork]
            rim = partial(bank_rim, slopes=slopes) if slopes else None
            for material, graded_vertices, indices in earthwork_meshes(
                points, faces, earthwork, ground_material, rim
            ):
                meshes.add(material, graded_vertices, indices)
                if retain:
                    dry_banks.append(dry_shore_edges(shore, graded_vertices, indices))
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
            terrain = points[used]
            if name in {"ground", "grass", "meadow"}:
                if building_stamps is not None:
                    terrain[:, 2] = building_stamps.heights(terrain)
                terrain = shore.deform(source, terrain)
            triangles = inverse.reshape(-1, 3)
            meshes.add(name, terrain, triangles)
            if retain:
                dry_banks.append(dry_shore_edges(shore, terrain, triangles))
    for link in links:
        meshes.add(link.material, link.mesh.vertices, link.mesh.triangles)
        edges, ground_z = link.retaining_sections()
        vertical_faces(meshes, "bridge deck", edges, ground_z)
    staircase_risers(meshes, stairs)
    draw_stair_guards(
        meshes, stairs, [*landings, *aprons], [*passage_ports, *deck_stair_edges]
    )
    for body, footprint in water:
        meshes.add("water", *body.mesh(source, footprint))
    shore_walls(
        meshes,
        "ground",
        source,
        shore,
        banks,
        dry_banks=np.concatenate(dry_banks) if dry_banks else None,
    )
    draw_tram_rails(meshes, routes, junctions)
    walk_edges = {}
    for link in links:
        walk_edges.setdefault(link.span_id, []).append(link.deck_edge)
    return approaches, carried, track_paving, crossings, openings, walk_edges


def assemble(
    source, radius, viaducts=(), approaches=(), *, crossings=None, water_plan=None
):
    meshes = Meshes()
    extent = box(-radius, -radius, radius, radius)
    vertices, quads = terrain_grid(source, radius)
    if water_plan is None:
        water_plan = water_bodies(
            source.features, extent, vertices[:, :2], source.height, source.height
        )
    water_plan = tuple(water_plan)
    water = tuple(clipped_water(water_plan, extent))
    bodies = shapely.union_all([footprint for _, footprint in water])
    if crossings is None:
        crossings = list(
            plan_water_crossings(source.features, extent, bodies, source.height)
        )
    centre = Point(0, 0)
    buildings = [
        (polygon, properties)
        for name, geometry, properties in source.features
        if name == "building" and not suppressed(properties)
        for polygon in polygons(
            geometry if geometry.is_valid else shapely.make_valid(geometry)
        )
        if polygon.distance(centre) <= radius
    ]
    foundations = building_foundations(
        [
            polygon
            for polygon, properties in buildings
            if properties.get("render_min_height", 0) == 0
            and properties.get("render_height", 0) > 0
            and extent.covers(polygon)
        ],
        source.height,
    )
    by_footprint = {foundation.footprint: foundation for foundation in foundations}
    grade = partial(terrain_height, vertices)
    passages = building_passages(
        [(g, p) for name, g, p in source.features if name == "transportation"],
        [
            polygon
            for polygon, properties in buildings
            if polygon in by_footprint and properties.get("render_height", 0) > 3.4
        ],
        extent,
        grade,
    )
    for polygon, properties in buildings:
        if polygon in passages:
            ridge = by_footprint[polygon].seat_m + properties["render_height"]
            passages[polygon] = [
                item for item in passages[polygon] if item.crown_m + 0.4 < ridge
            ]
    approaches, carried, track_paving, crossings, openings, walk_edges = (
        ground_surfaces(
            meshes,
            source,
            extent,
            bodies,
            vertices,
            quads,
            approaches,
            crossings,
            water=water,
            water_plan=water_plan,
            foundations=foundations,
            covered_walks=[item for group in passages.values() for item in group],
        )
    )
    if not track_paving.is_empty:
        crossings = [
            replace(
                c, pavement=c.pavement.union(track_paving.intersection(c.footprint))
            )
            for c in crossings
        ]
    bridges = draw_water_crossings(
        meshes, crossings, bodies, source.height, approaches, walk_edges=walk_edges
    )
    for viaduct in viaducts:
        draw_viaduct(meshes, viaduct, source.height, approaches)
    for polygon, properties in buildings:
        foundation = (
            by_footprint.get(polygon)
            if properties.get("render_min_height", 0) == 0
            else None
        )
        building(
            meshes,
            polygon,
            properties,
            source,
            foundation,
            passages.get(polygon, ()),
            grade,
        )
    for name, geom, prop in source.features:
        if name == "transportation" and prop.get("brunnel") != "tunnel":
            if at_grade_width(prop) is not None:
                continue
            geometry = shapely.make_valid(geom) if not geom.is_valid else geom
            if owned := carried.get(id(geom)):
                geometry = geometry.difference(owned)
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

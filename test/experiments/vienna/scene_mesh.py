"""Assemble source geometry in local east/north/up metres for Blender."""

import itertools

import numpy as np
import shapely
from bridges import build_water_bridges
from ground_surface import at_grade_routes, at_grade_width, partition_ground
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface
from outshine_ref.buildings.structures import suppressed
from outshine_ref.water.geodetic import local_points
from shapely.geometry import Point, box
from stairs import local_stairways
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
        samples = xy[shapely.contains_xy(body, xy[:, 0], xy[:, 1])]
        if not len(samples):
            samples = np.asarray(body.representative_point().coords)
        level = float(np.median(source.height(samples))) + source.datum

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


def ground_surfaces(meshes, source, extent, bodies, vertices, quads):
    roads = at_grade_routes(
        ((g, p) for layer, g, p in source.features if layer == "transportation"),
        extent,
    )
    stairs = local_stairways(
        source.features, extent, bodies, lambda points: terrain_height(vertices, points)
    )
    pieces = [
        (foot, z) for stair in stairs for foot, z in zip(stair.treads, stair.heights)
    ]
    heights = {i: z for i, (_, z) in enumerate(pieces, start=4)}
    stair_limit = shapely.union_all([stair.footprint for stair in stairs]).boundary
    names = dict(enumerate(("ground", "water", "road", "sidewalk")))
    names.update({i: i for i in heights})
    footprints = {1: bodies}
    footprints.update({i: foot for i, (foot, _) in enumerate(pieces, start=4)})
    footprints.update({2: roads["road"], 3: roads["sidewalk"]})
    banks = np.empty((0, 2, 3))
    for name, points, faces in partition_ground(
        vertices, quads, np.zeros(len(quads), dtype=int), names, footprints
    ):
        if name == "water" or name in heights:
            edges = points[faces][:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2, 3)
            if name == "water":
                banks = np.concatenate((banks, edges))
                continue
            height = heights[name]
            vertical_faces(
                meshes, "sidewalk", boundary_edges(edges, stair_limit), height
            )
            points = points.copy()
            points[:, 2] = height
            name = "sidewalk"
        if len(faces):
            used, inverse = np.unique(faces, return_inverse=True)
            meshes.add(name, points[used], inverse.reshape(-1, 3))
    staircase_risers(meshes, stairs)
    water_surfaces(meshes, source, bodies, vertices, banks)


def assemble(source, radius, viaducts=()):
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
    ground_surfaces(meshes, source, extent, bodies, vertices, quads)
    bridges = build_water_bridges(
        meshes, source.features, extent, bodies, source.height
    )
    for viaduct in viaducts:
        draw_viaduct(meshes, viaduct, source.height)
    for name, geom, prop in source.features:
        geometry = shapely.make_valid(geom) if not geom.is_valid else geom
        if name == "building":
            for poly in polygons(geometry):
                if poly.distance(Point(0, 0)) <= radius:
                    building(meshes, poly, prop, source)
        elif name == "transportation" and prop.get("brunnel") != "tunnel":
            if at_grade_width(prop) is not None:
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

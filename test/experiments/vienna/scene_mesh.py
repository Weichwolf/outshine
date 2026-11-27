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
        xy_banks = banks[:, :, :2]
        boundary = body.boundary
        on_shore = shapely.dwithin(
            shapely.points(xy_banks.reshape(-1, 2)), boundary, 1e-6
        ).reshape(-1, 2).all(axis=1) & shapely.dwithin(
            shapely.points(xy_banks.mean(axis=1)), boundary, 1e-6
        )
        top = banks[on_shore]
        shore = top[:, :, :2].reshape(-1, 2)
        bottom = np.column_stack((shore, water_height(shore) - 0.1)).reshape(-1, 2, 3)
        walls = np.concatenate((top, bottom), axis=1).reshape(-1, 3)
        faces = np.arange(len(top))[:, None] * 4 + [0, 2, 1, 1, 2, 3]
        if len(walls):
            meshes.add("ground", walls, faces.reshape(-1, 3))


def assemble(source, radius):
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
    roads = at_grade_routes(
        ((g, p) for layer, g, p in source.features if layer == "transportation"),
        extent,
    )
    names = ("ground", "water", "road", "sidewalk")
    footprints = {1: bodies, 2: roads["road"], 3: roads["sidewalk"]}
    banks = np.empty((0, 2, 3))
    for name, points, faces in partition_ground(
        vertices, quads, np.zeros(len(quads), dtype=int), names, footprints
    ):
        if name == "water":
            edges = points[faces][:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2, 3)
            banks = np.concatenate((banks, edges))
        elif len(faces):
            used, inverse = np.unique(faces, return_inverse=True)
            meshes.add(name, points[used], inverse.reshape(-1, 3))
    water_surfaces(meshes, source, bodies, vertices, banks)

    bridges = build_water_bridges(
        meshes, source.features, extent, bodies, source.height
    )
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

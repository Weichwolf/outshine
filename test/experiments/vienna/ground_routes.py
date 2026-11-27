"""Cut mapped road and path surfaces into the regional terrain triangles."""

import numpy as np
import shapely
from shapely.geometry import shape

ROAD_WIDTH = {
    "motorway": 7.5,
    "trunk": 7.0,
    "primary": 7.0,
    "secondary": 6.5,
    "tertiary": 6.0,
    "minor": 5.5,
    "service": 3.4,
    "busway": 3.4,
    "track": 2.5,
    "path": 1.8,
}


def route_footprints(group, nw, se, domain, focal):
    footprints = {"road": [], "sidewalk": []}
    for feature in group.get("features", ()):
        props = feature["properties"]
        kind = props.get("class")
        width = ROAD_WIDTH.get(kind)
        if width is None or props.get("brunnel") or props.get("layer", 0) != 0:
            continue
        geom = shapely.transform(
            shape(feature["geometry"]),
            lambda xy: nw + xy / group["extent"] * (se - nw),
        )
        if geom.is_empty:
            continue
        if geom.geom_type.endswith("LineString"):
            if width * focal < 0.85 * geom.distance(shapely.Point(0, 0)):
                continue
            geom = geom.buffer(
                width / 2, cap_style="flat", join_style="round", quad_segs=2
            )
        else:
            geom = shapely.make_valid(geom)
        footprints["sidewalk" if kind == "path" else "road"].append(geom)
    roads = shapely.union_all(footprints["road"]).intersection(domain)
    paths = shapely.union_all(footprints["sidewalk"]).intersection(domain)
    return {"road": roads, "sidewalk": paths.difference(roads)}


def project_triangles(areas, ground):
    polygons, parent = shapely.get_parts(areas, return_index=True)
    keep = shapely.get_type_id(polygons) == 3
    polygons, parent = polygons[keep], parent[keep]
    triangles, owner = shapely.get_parts(
        shapely.constrained_delaunay_triangles(polygons), return_index=True
    )
    xy = shapely.get_coordinates(shapely.orient_polygons(triangles)).reshape(-1, 4, 2)[
        :, :3
    ]
    xyz = ground[parent[owner]]
    a = xyz[:, 0]
    n = np.cross(xyz[:, 1] - a, xyz[:, 2] - a)
    dz = -np.einsum("ijk,ik->ij", xy - a[:, None, :2], n[:, :2]) / n[:, 2, None]
    vertices = np.dstack((xy, a[:, 2, None] + dz)).reshape(-1, 3)
    return vertices, np.arange(len(vertices)).reshape(-1, 3)


def cut_ground_routes(vertices, quads, ground_name, footprints):
    triangles = quads[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    if not len(triangles):
        return []
    routes = shapely.union_all(list(footprints.values()))
    if routes.is_empty:
        return [(ground_name, vertices, triangles)]
    xyz = vertices[triangles]
    areas = shapely.polygons(xyz[:, :, :2])
    # A corner moved onto the mapped shoreline can collapse one of its two triangles.
    valid = shapely.area(areas) > 1e-9
    areas, triangles, xyz = areas[valid], triangles[valid], xyz[valid]
    shapely.prepare(routes)
    touches = shapely.intersects(routes, areas)
    products = [(ground_name, vertices, triangles[~touches])]
    if not touches.any():
        return products
    areas, xyz = areas[touches], xyz[touches]
    # Restrict GEOS overlays to the relevant part of the network before cutting each triangle.
    bins = np.floor(xyz[:, :, :2].mean(axis=1) / 96).astype(int)
    _, groups = np.unique(bins, axis=0, return_inverse=True)
    order = np.argsort(groups)
    patches = np.split(order, np.flatnonzero(np.diff(groups[order])) + 1)
    for name, footprint in (*footprints.items(), (ground_name, routes)):
        parts = []
        for patch in patches:
            corners = xyz[patch, :, :2].reshape(-1, 2)
            bounds = shapely.box(*corners.min(axis=0), *corners.max(axis=0))
            local = shapely.intersection(footprint, bounds)
            operator = (
                shapely.difference if name == ground_name else shapely.intersection
            )
            points, faces = project_triangles(operator(areas[patch], local), xyz[patch])
            if len(faces):
                parts.append(points)
        if parts:
            points = np.concatenate(parts)
            products.append((name, points, np.arange(len(points)).reshape(-1, 3)))
    return products

"""Partition terrain along vector shorelines, roads and paths."""

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


def route_area(geom, width):
    if geom.is_empty:
        return geom
    if geom.geom_type.endswith("LineString"):
        return geom.buffer(width / 2, cap_style="flat", join_style="round", quad_segs=2)
    return shapely.make_valid(geom)


def at_grade_width(props):
    if not props.get("brunnel") and props.get("layer", 0) == 0:
        return ROAD_WIDTH.get(props.get("class"))
    return None


def path_material(props):
    surface = props.get("surface")
    if surface in {"paving_stones", "sett", "cobblestone"} or (
        surface == "paved" and props.get("subclass") in {"pedestrian", "steps"}
    ):
        return "paving"
    return "sidewalk"


def at_grade_routes(features, domain, *, focal=None, replaced_paths=()):
    footprints = {"road": [], "sidewalk": [], "paving": []}
    for geom, props in features:
        kind = props.get("class")
        width = at_grade_width(props)
        if width is None:
            continue
        if (
            replaced_paths
            and props.get("subclass") == "footway"
            and geom.geom_type.endswith("LineString")
        ):
            kept = [
                part
                for part in shapely.get_parts(geom)
                if shapely.normalize(part) not in replaced_paths
            ]
            if not kept:
                continue
            geom = shapely.union_all(kept)
        if (
            focal is not None
            and geom.geom_type.endswith("LineString")
            and width * focal < 0.85 * geom.distance(shapely.Point(0, 0))
        ):
            continue
        material = path_material(props) if kind == "path" else "road"
        footprints[material].append(route_area(geom, width))
    roads = shapely.union_all(footprints["road"]).intersection(domain)
    paths = shapely.union_all(footprints["sidewalk"]).intersection(domain)
    paving = shapely.union_all(footprints["paving"]).intersection(domain)
    return {
        "road": roads.difference(paths.union(paving)),
        "sidewalk": paths.difference(paving),
        "paving": paving,
    }


def route_footprints(group, nw, se, domain, focal):
    features = (
        (
            shapely.transform(
                shape(feature["geometry"]),
                lambda xy: nw + xy / group["extent"] * (se - nw),
            ),
            feature["properties"],
        )
        for feature in group.get("features", ())
        if at_grade_width(feature["properties"]) is not None
    )
    return at_grade_routes(features, domain, focal=focal)


def project_triangles(cells, ground):
    triangles, owner = shapely.get_parts(
        shapely.constrained_delaunay_triangles(cells), return_index=True
    )
    xy = shapely.get_coordinates(shapely.orient_polygons(triangles)).reshape(-1, 4, 2)[
        :, :3
    ]
    xyz = ground[owner]
    a = xyz[:, 0]
    n = np.cross(xyz[:, 1] - a, xyz[:, 2] - a)
    dz = -np.einsum("ijk,ik->ij", xy - a[:, None, :2], n[:, :2]) / n[:, 2, None]
    vertices = np.dstack((xy, a[:, 2, None] + dz)).reshape(-1, 3)
    return vertices, np.arange(len(vertices)).reshape(-1, 3)


def surface_labels(points, base, footprints):
    labels = base.copy()
    xy = shapely.get_coordinates(points).T
    for label, footprint in reversed(tuple(footprints.items())):
        shapely.prepare(footprint)
        labels[shapely.contains_xy(footprint, *xy)] = label
    return labels


def partition_ground(vertices, quads, base, names, footprints):
    triangles = quads[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    base = np.repeat(base, 2)
    xyz = vertices[triangles]
    areas = shapely.polygons(xyz[:, :, :2])
    boundary = shapely.boundary(list(footprints.values()))
    edges = shapely.union_all(boundary)
    coords, owner = shapely.get_coordinates(shapely.get_parts(edges), return_index=True)
    start = np.flatnonzero(owner[:-1] == owner[1:])
    segments = shapely.linestrings(coords[start[:, None] + [0, 1]])
    _, touched = shapely.STRtree(areas).query(segments, predicate="intersects")
    crossing = np.zeros(len(areas), dtype=bool)
    crossing[touched] = True
    fixed_xyz, fixed_faces = xyz[~crossing], triangles[~crossing]
    fixed = surface_labels(
        shapely.points(fixed_xyz[:, :, :2].mean(axis=1)), base[~crossing], footprints
    )
    for label in np.unique(fixed):
        yield names[label], vertices, fixed_faces[fixed == label]
    if not crossing.any():
        return

    source = areas[crossing]
    # A micron grid makes coincident edges stable during GEOS noding.
    border = shapely.union_all(np.r_[shapely.boundary(source), edges], grid_size=1e-6)
    cells = shapely.get_parts(shapely.polygonize(shapely.get_parts(border)))
    samples = shapely.point_on_surface(cells)
    cell, parent = shapely.STRtree(source).query(samples, predicate="within")
    labels = surface_labels(samples[cell], base[crossing][parent], footprints)
    ground = xyz[crossing][parent]
    for label in np.unique(labels):
        mask = labels == label
        yield names[label], *project_triangles(cells[cell[mask]], ground[mask])

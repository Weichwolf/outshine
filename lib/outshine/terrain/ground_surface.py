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
CARRIAGEWAY_MATERIALS = ("road", "unpaved road")
ROUTE_MATERIALS = ("road", "sidewalk", "paving", "unpaved path", "unpaved road")
SOIL_MATERIALS = ("ground", "forest floor", "grass", "meadow")
TRAIL_SHOULDER_M = 0.42
MINERAL_TRAIL_SHOULDER_M = 0.54
MEADOW_MARGIN_M = 2.4
DIRT_ROAD_SHOULDER_M = 0.82
DIRT_ROAD_CENTRE_M = (1.60, 2.45)
UNPAVED_SURFACES = {
    "unpaved",
    "compacted",
    "fine_gravel",
    "gravel",
    "ground",
    "dirt",
    "earth",
    "sand",
    "pebblestone",
}


def polygons(geometry):
    if geometry.geom_type == "Polygon" and not geometry.is_empty:
        yield shapely.orient_polygons(geometry)
    elif hasattr(geometry, "geoms"):
        for part in geometry.geoms:
            yield from polygons(part)


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
    kind = props.get("subclass")
    if (
        surface in {"paving_stones", "sett", "cobblestone"}
        or (surface == "paved" and kind in {"pedestrian", "steps"})
        or (surface is None and kind == "pedestrian")
    ):
        return "paving"
    if surface in UNPAVED_SURFACES or (surface is None and kind == "path"):
        return "unpaved path"
    return "sidewalk"


def clip_route_areas(areas, domain):
    clipped = shapely.union_all(areas).intersection(domain)
    if clipped.geom_type in ("Polygon", "MultiPolygon"):
        return clipped
    parts = shapely.get_parts(clipped)
    areal = parts[shapely.get_dimensions(parts) == 2]
    return shapely.union_all(areal) if len(areal) else shapely.Polygon()


def forest_trail_edges(path, woodland, reserved):
    return shared_edges(
        path,
        woodland.difference(path),
        reserved.buffer(2 * TRAIL_SHOULDER_M, quad_segs=2),
    )


def shared_edges(area, neighbour, reserved):
    # The overlay may place an intersection a few ulps off the input edge.
    contact = neighbour.buffer(1e-4, quad_segs=2)
    edge = area.boundary.intersection(contact)
    if edge.is_empty:
        return edge
    parts = shapely.get_parts(edge.difference(reserved))
    return shapely.union_all(parts[(shapely.get_dimensions(parts) == 1)])


def margin_weights(edge, vertices, width):
    xy = np.asarray(vertices)[:, :2]
    distance = shapely.distance(shapely.points(xy), edge)
    return np.clip(1 - distance / width, 0, 1).astype("f4")


def dirt_road_weights(edge, vertices):
    distance = shapely.distance(shapely.points(np.asarray(vertices)[:, :2]), edge)
    shoulder = np.clip(1 - distance / DIRT_ROAD_SHOULDER_M, 0, 1)
    low, high = DIRT_ROAD_CENTRE_M
    centre = 0.64 * np.clip((distance - low) / (high - low), 0, 1)
    return np.maximum(shoulder, centre).astype("f4")


def carriageway_footprint(roads):
    return roads["road"].union(roads.get("unpaved road", shapely.Polygon()))


def _path_footprints(paths, carriageways):
    if not paths:
        return
    cutouts = {}
    if carriageways:
        axes, road_areas = zip(*carriageways, strict=True)
        index = shapely.STRtree(axes)
        pairs = index.query([line for _, line, _ in paths], predicate="intersects")
        for path, road in pairs.T:
            _, line, width = paths[path]
            area = road_areas[road]
            crossing = [
                part
                for part in shapely.get_parts(line.intersection(area))
                if part.geom_type == "LineString" and part.intersects(axes[road])
            ]
            if crossing:
                near = shapely.union_all(crossing).buffer(width / 2, quad_segs=2)
                cutouts.setdefault(path, []).append(near.intersection(area))
    for index, (material, line, width) in enumerate(paths):
        footprint = route_area(line, width)
        if index in cutouts:
            footprint = footprint.difference(
                shapely.union_all(cutouts[index], grid_size=1e-6), grid_size=1e-6
            )
        yield material, footprint


def at_grade_routes(features, domain, *, focal=None, replaced_paths=()):
    footprints = {name: [] for name in ROUTE_MATERIALS}
    carriageways, paths = [], []
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
        if kind == "path":
            material = path_material(props)
        elif props.get("surface") in UNPAVED_SURFACES:
            material = "unpaved road"
        else:
            material = "road"
        if geom.geom_type.endswith("LineString"):
            for part in shapely.get_parts(geom):
                if part.is_empty:
                    continue
                if material in CARRIAGEWAY_MATERIALS:
                    area = route_area(part, width)
                    carriageways.append((part, area))
                    footprints[material].append(area)
                else:
                    paths.append((material, part, width))
            continue
        footprints[material].append(route_area(geom, width))
    for material, footprint in _path_footprints(paths, carriageways):
        footprints[material].append(footprint)
    roads, paths, paving, unpaved, unpaved_roads = (
        clip_route_areas(footprints[name], domain) for name in ROUTE_MATERIALS
    )
    hard_paths = paths.union(paving)
    reserved = hard_paths.union(unpaved)
    return {
        "road": roads.difference(reserved),
        "sidewalk": paths.difference(paving),
        "paving": paving,
        "unpaved path": unpaved.difference(hard_paths),
        "unpaved road": unpaved_roads.difference(reserved.union(roads)),
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


def surface_elevations(mesh, xy):
    vertices, faces = mesh
    if not len(faces):
        return np.full(len(xy), -np.inf)
    xy = np.asarray(xy)
    triangles = np.asarray(vertices)[np.asarray(faces)]
    footprints = shapely.polygons(triangles[..., :2])
    valid = shapely.area(footprints) > 1e-10
    triangles = triangles[valid]
    site, triangle = shapely.STRtree(footprints[valid]).query(
        shapely.points(xy), predicate="covered_by"
    )
    origin, east, north = triangles[triangle].transpose(1, 0, 2)
    east = east - origin
    north = north - origin
    delta = xy[site] - origin[:, :2]
    det = east[:, 0] * north[:, 1] - east[:, 1] * north[:, 0]
    weight_east = (delta[:, 0] * north[:, 1] - delta[:, 1] * north[:, 0]) / det
    weight_north = (east[:, 0] * delta[:, 1] - east[:, 1] * delta[:, 0]) / det
    heights = np.full(len(xy), -np.inf)
    np.maximum.at(
        heights,
        site,
        origin[:, 2] + east[:, 2] * weight_east + north[:, 2] * weight_north,
    )
    return heights


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


def grassland_material(properties):
    if properties.get("class") == "grass":
        return "meadow" if properties.get("subclass") == "meadow" else "grass"
    return None


def surface_labels(points, base, footprints):
    labels = base.copy()
    xy = shapely.get_coordinates(points).T
    for label, footprint in reversed(tuple(footprints.items())):
        shapely.prepare(footprint)
        labels[shapely.contains_xy(footprint, *xy)] = label
    return labels


def terrain_triangles(quads):
    return quads[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)


def partition_ground(vertices, quads, base, names, footprints, *, breaks=()):
    yield from partition_triangles(
        vertices,
        terrain_triangles(quads),
        np.repeat(base, 2),
        names,
        footprints,
        breaks=breaks,
    )


def partition_triangles(vertices, triangles, base, names, footprints, *, breaks=()):
    xyz = vertices[triangles]
    areas = shapely.polygons(xyz[:, :, :2])
    boundary = shapely.boundary(list(footprints.values()))
    edges = shapely.union_all([*boundary, *breaks])
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

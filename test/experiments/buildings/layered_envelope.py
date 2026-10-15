"""Direct complete ENU boundary of nested metrical prism layers."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import shapely
from outshine_ref.buildings.modelling.mesh_fans import connected_fans
from outshine_ref.buildings.modelling.prism_union import PrismVolume, frozen
from shapely.affinity import translate


@dataclass(frozen=True)
class LayeredEnvelope:
    mesh: object
    vertex_roots: object
    source_ids: tuple
    source_indices: object
    surface_roles: object
    volume_m3: float
    area_m2: float
    base_footprint_wkb: bytes


def regions(sources, origin):
    local = [
        translate(source.polygon, xoff=-origin[0], yoff=-origin[1])
        for source in sources
    ]
    for index, (a, b) in enumerate(zip(sources, sources[1:], strict=False)):
        if a.high_m != b.low_m:
            raise ValueError(
                "Nested envelope layers must meet at an exact shared height"
            )
        if local[index + 1].difference(local[index]).area != 0.0:
            raise ValueError("Nested envelope upper footprint escapes its lower layer")
    exposed = [a.difference(b) for a, b in zip(local, local[1:], strict=False)] + [
        local[-1]
    ]
    triangles = []
    owners = []
    for index, region in enumerate(exposed):
        if region.is_empty:
            continue
        for triangle in shapely.get_parts(
            shapely.constrained_delaunay_triangles(region)
        ):
            xy = np.asarray(triangle.exterior.coords)[:3]
            if np.cross(np.r_[xy[1] - xy[0], 0.0], np.r_[xy[2] - xy[0], 0.0])[2] < 0:
                xy = xy[::-1]
            triangles.append(xy)
            owners.append(index)
    if not triangles:
        raise ValueError("Nested envelope has no resolved upper domain")
    triangles = np.asarray(triangles)
    owners = np.asarray(owners, dtype=np.int32)
    domain = shapely.union_all([shapely.Polygon(xy) for xy in triangles])
    if domain.symmetric_difference(local[0]).area > 1e-8:
        raise ValueError("Nested exposed facets do not cover the source base domain")
    return local, triangles, owners


def layer_edges(triangles, owners, index):
    edges = {}
    for triangle in triangles[owners >= index]:
        for a, b in zip(triangle, np.roll(triangle, -1, axis=0), strict=True):
            key = tuple(sorted((tuple(a), tuple(b))))
            edges.setdefault(key, []).append((a, b))
    boundary = []
    for values in edges.values():
        if len(values) == 1:
            boundary.append(values[0])
        elif (
            len(values) != 2
            or not np.array_equal(values[0][0], values[1][1])
            or not np.array_equal(values[0][1], values[1][0])
        ):
            raise ValueError("Nested triangulation has an ambiguous boundary edge")
    return boundary


def validate(sources, roof_material, base_material, max_triangles):
    if not sources or any(
        not isinstance(source, PrismVolume) or source.polygon.is_empty
        for source in sources
    ):
        raise ValueError("Nested envelope requires nonempty PrismVolume layers")
    if len({s.identifier for s in sources}) != len(sources):
        raise ValueError("Duplicate nested layer identity")
    if any(
        type(value) is not int or not 0 <= value <= 65535
        for value in (roof_material, base_material)
    ):
        raise ValueError("Nested cap materials require uint16 identifiers")
    if type(max_triangles) is not int or max_triangles < 1:
        raise ValueError("Nested triangle capacity must be positive")
    if (
        sum(len(shapely.get_coordinates(s.polygon)) for s in sources) * 4
        > max_triangles
    ):
        raise ValueError("Nested input boundary capacity exceeded")


def layered_envelope(sources, roof_material=9, base_material=0, max_triangles=262144):
    """Experimental complete nested envelope; no arbitrary CGA/resource inheritance.

    Original IDs, layer/role, metric UV0 and geometric normals are explicit.
    Input/output capacity is not a hard GEOS time/memory bound. The existing
    prism union is the measured production choice; this strict alternative
    stays outside the installed library.
    """
    sources = tuple(sources)
    validate(sources, roof_material, base_material, max_triangles)
    bounds = sources[0].polygon.bounds
    origin = np.array(
        [
            bounds[0] + (bounds[2] - bounds[0]) / 2,
            bounds[1] + (bounds[3] - bounds[1]) / 2,
        ]
    )
    local, triangles, owners = regions(sources, origin)
    points = []
    materials = []
    indices = []
    roles = []
    uv = []

    def append(xyz, material, index, role, coordinates):
        points.extend(xyz)
        materials.append(material)
        indices.append(index)
        roles.append(role)
        uv.extend(coordinates)

    for xy, index in zip(triangles, owners, strict=True):
        append(
            np.c_[xy, np.full(3, sources[index].high_m)], roof_material, index, 0, xy
        )
        append(
            np.c_[xy[::-1], np.full(3, sources[0].low_m)], base_material, 0, 1, xy[::-1]
        )
    for index, source in enumerate(sources):
        boundary = layer_edges(triangles, owners, index)
        region = shapely.union_all([shapely.LineString([a, b]) for a, b in boundary])
        if region.symmetric_difference(local[index].boundary).length > 1e-8:
            raise ValueError(
                "Nested wall boundary lost a source edge or contains a T junction"
            )
        for a, b in boundary:
            length = np.linalg.norm(b - a)
            quad = np.array(
                [
                    (*a, source.low_m),
                    (*b, source.low_m),
                    (*b, source.high_m),
                    (*a, source.high_m),
                ]
            )
            coordinates = np.array(
                [
                    [0.0, source.low_m],
                    [length, source.low_m],
                    [length, source.high_m],
                    [0.0, source.high_m],
                ]
            )
            for order in ((0, 1, 2), (0, 2, 3)):
                append(
                    quad[list(order)],
                    source.material,
                    index,
                    2,
                    coordinates[list(order)],
                )
    if len(materials) > max_triangles:
        raise ValueError("Nested output triangle capacity exceeded")
    vertices = np.asarray(points)
    faces = np.arange(len(vertices), dtype=np.uint32).reshape(-1, 3)
    coordinates = np.asarray(uv)
    mesh = dict(vertices=vertices, faces=faces, uv=coordinates)
    _, _, duplicates, targets = connected_fans(mesh, np.arange(len(faces)))
    roots = np.arange(len(vertices), dtype=np.uint64)
    roots[duplicates] = targets
    logical = roots[faces]
    edge = np.concatenate([logical[:, [0, 1]], logical[:, [1, 2]], logical[:, [2, 0]]])
    _, inverse, count = np.unique(
        np.sort(edge, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    if np.any(count != 2) or np.any(
        np.bincount(inverse, weights=np.where(edge[:, 0] < edge[:, 1], 1, -1))
    ):
        raise ValueError(
            "Nested envelope has an open or inconsistent directed boundary"
        )
    xyz = vertices[faces]
    normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    length = np.linalg.norm(normals, axis=1)
    if np.any(length == 0) or not np.isfinite(length).all():
        raise ValueError("Nested envelope has an unresolved normal")
    mesh.update(
        vertices=vertices + [*origin, 0.0],
        materials=np.asarray(materials, dtype=np.uint16),
        normals=np.repeat(normals / length[:, None], 3, axis=0),
    )
    volume = sum(
        source.polygon.area * (source.high_m - source.low_m) for source in sources
    )
    return LayeredEnvelope(
        MappingProxyType({k: frozen(v) for k, v in mesh.items()}),
        frozen(roots),
        tuple(s.identifier for s in sources),
        frozen(np.asarray(indices, dtype=np.int32)),
        frozen(np.asarray(roles, dtype=np.uint8)),
        volume,
        float(length.sum() / 2),
        sources[0].polygon.wkb,
    )

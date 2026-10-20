"""Functional water-mask queries before render cuts (#963).

Queries use cell-relative Mercator parameters and return the canonical P1 ECEF
position/normal. They are not vertical height-at-longitude/latitude queries.
The caller qualifies immutable root facets exactly as for the render cooker.
"""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.geometry.sampling import owned
from outshine_ref.places.sources import tile_tuple
from water_cell_mask import local_sources, root_normal
from water_nodal_height import cell_faces, parameter_triangle, sample_face


@dataclass(frozen=True)
class CellField:
    cell: tuple
    context: str
    source_keys: tuple
    polygons: tuple
    mask: object
    roots: np.ndarray
    world: np.ndarray
    normals: np.ndarray
    report: dict


@dataclass(frozen=True)
class FieldSamples:
    states: tuple
    world: np.ndarray
    normals: np.ndarray
    parents: np.ndarray
    source_offsets: np.ndarray
    source_members: np.ndarray
    report: dict


def field_facet(field, keys):
    """Reuse an admitted cached root when later render detail is requested."""
    faces = cell_faces(field.cell)
    for at, root in enumerate(field.roots):
        if faces[int(root)] == keys:
            return "admitted", field.world[at]
    return "unresolved_facet", None


def prepare_field(cell, polygons, source_keys, facet, *, context):
    cell = tile_tuple(cell)
    if not isinstance(context, str) or not context:
        raise ValueError("immutable field context required")
    local, mask = local_sources(cell, polygons, source_keys, 32768)
    roots, vertices, normals = [], [], []
    work = {"root_intersections": 0, "root_requests": 0, "render_triangles": 0}
    for index, keys in enumerate(cell_faces(cell)):
        triangle = shapely.Polygon(parameter_triangle(keys) - cell[1:])
        work["root_intersections"] += 1
        if mask.intersection(triangle).area <= 0:
            continue
        work["root_requests"] += 1
        state, world = facet(keys)
        if state != "admitted":
            return None, dict(work, state=state, refused_root=keys)
        world, normal = root_normal(world)
        roots.append(index)
        vertices.append(world)
        normals.append(normal)
    if not roots:
        raise ValueError("positive covered water area required")
    roots, world, normals = (
        owned(roots, np.uint8),
        owned(vertices, float),
        owned(normals, float),
    )
    report = dict(
        work,
        state="functional_finite_cell",
        sources=len(local),
        source_coordinates=int(shapely.get_num_coordinates(local).sum()),
        numerical_array_bytes=sum(a.nbytes for a in (roots, world, normals)),
        geometry_workspace_bytes=None,
    )
    return CellField(
        cell,
        context,
        tuple(source_keys),
        tuple(local),
        mask,
        roots,
        world,
        normals,
        report,
    ), report


def sample_parameters(field, points, *, member_limit=100000):
    """Classify the precise source mask, then sample only its qualified P1 roots.

    Outside this cell is not called dry. Missing fields cannot be sampled. Source
    membership is pointwise coverage, not a render face's overlap-candidate CSR.
    Work is bounded by4096 queries,8 root tests and1024 source tests per query.
    A member-budget refusal publishes no partial source list for that point.
    """
    points = np.asarray(points, dtype=float)
    if (
        not isinstance(field, CellField)
        or points.ndim != 2
        or points.shape[1] != 2
        or not 0 < len(points) <= 4096
        or not np.isfinite(points).all()
        or type(member_limit) is not int
        or not 0 <= member_limit <= 100000
    ):
        raise ValueError(
            "qualified field and bounded finite parameter queries required"
        )
    world, normals = (
        np.full((len(points), 3), np.nan),
        np.full((len(points), 3), np.nan),
    )
    parents, offsets = (
        np.full(len(points), -1, dtype=np.int16),
        np.zeros(len(points) + 1, dtype=np.uint32),
    )
    states, members = [], []
    work = {"mask_tests": 0, "root_tests": 0, "source_tests": 0, "sampled_points": 0}
    faces = cell_faces(field.cell)
    root_shapes = [
        shapely.Polygon(parameter_triangle(faces[r]) - field.cell[1:])
        for r in field.roots
    ]
    for i, xy in enumerate(points):
        offsets[i + 1] = len(members)
        if np.any(xy < 0) or np.any(xy > 1):
            states.append("outside_cell")
            continue
        point = shapely.Point(xy)
        work["mask_tests"] += 1
        if not field.mask.covers(point):
            states.append("dry")
            continue
        covering = shapely.covers(root_shapes, point)
        work["root_tests"] += len(root_shapes)
        selected = np.flatnonzero(covering)
        if not len(selected):
            states.append("unresolved_boundary")
            continue
        at = int(selected[0])
        sources = np.flatnonzero(shapely.covers(field.polygons, point))
        work["source_tests"] += len(field.polygons)
        if not len(sources):
            states.append("unresolved_source")
            continue
        if len(members) + len(sources) > member_limit:
            states.append("membership_budget_exhausted")
            continue
        parent = int(field.roots[at])
        world[i] = sample_face(
            faces[parent], field.world[at], xy.reshape(1, 2), origin=field.cell[1:]
        )[0]
        normals[i], parents[i] = field.normals[at], parent
        members.extend(sources)
        offsets[i + 1] = len(members)
        states.append("water")
        work["sampled_points"] += 1
    arrays = (world, normals, parents, offsets, np.asarray(members, dtype=np.uint32))
    report = dict(
        work,
        committed_source_members=len(members),
        output_array_bytes=sum(a.nbytes for a in arrays),
    )
    return FieldSamples(tuple(states), *(owned(a, a.dtype) for a in arrays), report)

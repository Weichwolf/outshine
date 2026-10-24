"""Conform one supplied terrain patch to the pool's two declared contact roles."""

from dataclasses import dataclass
from fractions import Fraction

import numpy as np
import shapely
from conforming_body_contact import arrange, contact_error
from earthwork_transition import smoothstep
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from pool_body import positive_faces
from pool_cell_partition import partition


@dataclass(frozen=True)
class ContactSurface:
    sampler: SurfaceSampler


def _sample(sampler, xy):
    result = sampler.sample(xy)
    if np.any(result.face_indices < 0):
        raise ValueError("pool contact sampling is unresolved")
    return result.points_enu_m[:, 2]


def _clipped(source, domain):
    polygons = source.sampler._polygons
    selected = source.sampler._tree.query(domain, predicate="intersects")
    intersections = shapely.intersection(polygons[selected], domain)
    pieces = shapely.get_parts(intersections)
    pieces = pieces[shapely.area(pieces) > 0]
    if not len(pieces):
        raise ValueError("terrain patch does not meet this pool body")
    if len(pieces) > 4096:
        raise ValueError("pool contact clipping exceeds piece budget")
    polygons = shapely.get_parts(shapely.constrained_delaunay_triangles(pieces))
    coordinates = shapely.get_coordinates(polygons).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    faces = positive_faces(xy, inverse.reshape(-1, 3))
    vertices = np.column_stack((xy, _sample(source.sampler, xy)))
    return ContactSurface(SurfaceSampler(Mesh(vertices, faces)))


def _parameters(points, used, source, faces):
    edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    result = []
    for node in used:
        found = False
        for first, second in edges[counts == 1]:
            a, b = [
                tuple(Fraction.from_float(float(v)) for v in source[i, :2])
                for i in (first, second)
            ]
            p = points[node]
            if (b[0] - a[0]) * (p[1] - a[1]) != (b[1] - a[1]) * (p[0] - a[0]):
                continue
            axis = 0 if a[0] != b[0] else 1
            t = (p[axis] - a[axis]) / (b[axis] - a[axis])
            if 0 <= t <= 1:
                result.append(
                    {
                        "output_vertex": int(node),
                        "input_edge": [int(first), int(second)],
                        "parameter": str(t),
                    }
                )
                found = True
                break
        if not found:
            raise ValueError("exact boundary node lost original source-edge binding")
    return result


def contact_patch(
    body,
    terrain_vertices,
    terrain_faces,
    *,
    apron_m=3.0,
    method="exact",
    boundary_requests=(),
):
    """Return a local mesh plus explicit changed outer-edge obligations, never a world claim."""
    if not np.isfinite(apron_m) or not 0 < apron_m <= 10:
        raise ValueError("bounded explicit outer-contact apron required")
    if len(terrain_faces) > 1024 or len(terrain_vertices) > 4096:
        raise ValueError("bounded supplied original terrain patch required")
    prior = SurfaceSampler(Mesh(terrain_vertices, terrain_faces))
    source = ContactSurface(
        SurfaceSampler(Mesh(body["contact_vertices"], body["contact_faces"]))
    )
    domain = shapely.coverage_union_all(prior._polygons)
    if method == "exact":
        xy, faces, exact_points, work = partition(
            body, terrain_vertices, terrain_faces, boundary_requests=boundary_requests
        )
    elif method == "geos":
        if boundary_requests:
            raise ValueError("exact boundary requests require the exact partition path")
        clipped = _clipped(source, domain)
        xy, faces, work = arrange(clipped, terrain_vertices[:, :2], terrain_faces)
        exact_points = None
    else:
        raise ValueError("known exact or declining-GEOS control required")
    samples = source.sampler.sample(xy)
    inside = samples.face_indices >= 0
    z = _sample(prior, xy).copy()
    z[inside] = samples.points_enu_m[inside, 2]
    if np.any(~inside):
        outer = shapely.polygons(body["plan_xy"][body["outer_ring"]]).boundary
        lines = shapely.shortest_line(shapely.points(xy[~inside]), outer)
        foot = shapely.get_coordinates(lines).reshape(-1, 2, 2)[:, 1]
        rim = _sample(source.sampler, foot)
        distance = np.linalg.norm(foot - xy[~inside], axis=1)
        fade = smoothstep(np.minimum(distance / apron_m, 1))
        z[~inside] += (rim - z[~inside]) * (1 - fade)
    vertices = np.column_stack((xy, z))
    errors = contact_error(source, vertices, faces)
    expected_area = shapely.area(
        shapely.intersection(
            shapely.coverage_union_all(source.sampler._polygons), domain
        )
    )
    if (
        max(
            errors["maximum_above_underside_m"], errors["maximum_gap_below_underside_m"]
        )
        > 1e-7
    ):
        raise ValueError("local mesh failed its complete contact-surface comparison")
    errors["maximum_above_contact_surface_m"] = errors.pop("maximum_above_underside_m")
    errors["maximum_below_contact_surface_m"] = errors.pop(
        "maximum_gap_below_underside_m"
    )
    edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    boundary = edges[counts == 1]
    used = np.unique(boundary)
    previous = _sample(prior, xy[used])
    obligations = np.column_stack((xy[used], previous, z[used]))
    if exact_points is not None:
        work["exact_boundary_parameters"] = _parameters(
            exact_points, used, terrain_vertices, terrain_faces
        )
        new_boundary = sum(
            port["parameter"] not in ("0", "1")
            for port in work["exact_boundary_parameters"]
        )
    else:
        original_xy = set(map(tuple, np.asarray(terrain_vertices)[:, :2]))
        new_boundary = sum(tuple(xy[i]) not in original_xy for i in used)
    neighbor_update = bool(new_boundary or np.any(abs(z[used] - previous) > 1e-7))
    return (
        vertices,
        faces,
        obligations,
        {
            "work": work,
            "contact": errors,
            "expected_overlap_area_m2": float(expected_area),
            "maximum_outer_boundary_height_change_m": float(
                abs(z[used] - previous).max()
            ),
            "new_outer_boundary_vertices": new_boundary,
            "neighbor_update_required": neighbor_update,
            "admission": "candidate_pending_neighbor_updates"
            if neighbor_update
            else "local_contact_only",
            "scope": "Two-role contact surface, not body underside. Changed patch boundary is an explicit adjacent-mesh obligation; no independent native page/world admission.",
        },
    )

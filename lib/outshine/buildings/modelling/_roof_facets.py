"""Source-owned swept facets and bounded incident-plane interface refinement."""

import numpy as np
import shapely

from ...geometry.welding import weld_vertices
from ._roof_contours import shared_points, subdivide_ring
from .skeleton_plan import SkeletonBudgetExceeded, face_boundaries


def refine_interfaces(points, face_owners, owners, fixed, tolerance, report):
    _, labels, note = weld_vertices(points, 5e-5, max_vertices=16384, max_pairs=262144)
    result = points.copy()
    maximum = 0.0
    for label in np.unique(labels):
        group = np.flatnonzero(labels == label)
        if len(group) < 2 or np.any(group < fixed):
            continue
        incident = np.unique(face_owners[group])
        if len(incident) < 2:
            continue
        normals = np.array([owners[i][1] for i in incident])
        intercepts = np.array([owners[i][2] for i in incident])
        matrix = np.c_[normals, -np.ones(len(normals))]
        center = points[group].mean(axis=0)
        current = np.r_[center, np.mean(normals @ center + intercepts)]
        residual = -intercepts - matrix @ current
        delta, _, rank, _ = np.linalg.lstsq(matrix, residual, rcond=None)
        solution = current + delta
        if np.max(abs(residual)) <= tolerance:
            solution = current
        if rank == 3:
            candidate, height = solution[:2], solution[2]
        elif rank == 2:
            differences = normals - normals[0]
            index = int(np.argmax(np.linalg.norm(differences, axis=1)))
            axis = differences[index]
            if axis @ axis <= 1e-24:
                continue
            candidate = points[group].mean(axis=0)
            candidate -= (
                axis
                * (axis @ candidate + intercepts[index] - intercepts[0])
                / (axis @ axis)
            )
            height = normals[0] @ candidate + intercepts[0]
        else:
            continue
        correction = float(np.linalg.norm(points[group] - candidate, axis=1).max())
        residual = float(np.max(abs(normals @ candidate + intercepts - height)))
        if correction <= 5e-5 and residual <= tolerance * 8:
            result[group] = candidate
            maximum = max(maximum, correction)
    if report is not None:
        report.update(interface_candidates=note, maximum_interface_refinement_m=maximum)
    return result


def assemble_patches(patches, rings, boundaries, tolerance, limits, report):
    original = np.concatenate(rings)
    polygons = []
    for pieces, (_, normal, intercept) in zip(
        patches, boundaries.values(), strict=True
    ):
        polygon = shapely.union_all(pieces)
        if not isinstance(polygon, shapely.Polygon) or polygon.interiors:
            raise ValueError("Swept source facet is disconnected or has holes")
        polygon = shapely.simplify(polygon, tolerance)
        station_time = np.array(polygon.exterior.coords)[:-1]
        tangent = np.array((normal[1], -normal[0]))
        points = (
            station_time[:, :1] * tangent + (station_time[:, 1:] - intercept) * normal
        )
        points = subdivide_ring(points, original, tolerance)
        polygons.append(points)
    lengths = [len(p) for p in polygons]
    combined = np.vstack([original, *polygons])
    source_ids = np.r_[
        np.full(len(original), -1), np.repeat(np.arange(len(polygons)), lengths)
    ]
    combined = refine_interfaces(
        combined,
        source_ids,
        list(boundaries.values()),
        len(original),
        tolerance,
        report,
    )
    combined = shared_points(combined, tolerance, fixed=len(original))
    coordinates = np.unique(combined, axis=0)
    if len(coordinates) > limits.nodes:
        raise SkeletonBudgetExceeded("Swept facet node capacity exceeded")
    lookup = {tuple(p): i for i, p in enumerate(coordinates)}
    faces, offset = [], len(original)
    for length in lengths:
        points = combined[offset : offset + length]
        points = subdivide_ring(points, coordinates, tolerance)
        faces.append(tuple(lookup[tuple(p)] for p in points))
        offset += length
    ownership = face_boundaries(coordinates, faces, boundaries)
    distances = np.zeros(len(coordinates))
    for face, (_, gradient, intercept) in zip(faces, ownership, strict=True):
        distances[list(face)] = np.maximum(
            distances[list(face)], coordinates[list(face)] @ gradient + intercept
        )
    for point in original:
        distances[lookup[tuple(point)]] = 0
    return coordinates, distances, tuple(faces), ownership

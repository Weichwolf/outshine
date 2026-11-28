"""Directed kinetic contour contacts with source provenance, including holes."""

import numpy as np
import shapely

from ...geometry.welding import weld_vertices
from .skeleton_plan import SkeletonBudgetExceeded


def shared_points(points, tolerance, fixed=0):
    result, labels, _ = weld_vertices(
        points, tolerance, max_vertices=16384, max_pairs=262144
    )
    if len(np.unique(labels[:fixed])) != fixed:
        raise ValueError(
            "Distinct original vertices lie inside the numerical node guard"
        )
    return result[labels]


def winding(segments, point):
    a, b = segments[:, 0], segments[:, 1]
    cross = (b[:, 0] - a[:, 0]) * (point[1] - a[:, 1]) - (b[:, 1] - a[:, 1]) * (
        point[0] - a[:, 0]
    )
    up = (a[:, 1] <= point[1]) & (b[:, 1] > point[1]) & (cross > 0)
    down = (a[:, 1] > point[1]) & (b[:, 1] <= point[1]) & (cross < 0)
    return int(up.sum() - down.sum())


def subdivide_ring(points, anchors, tolerance):
    if len(points) * len(anchors) > 2_000_000:
        raise SkeletonBudgetExceeded("Contour subdivision candidate budget exceeded")
    edges = np.roll(points, -1, axis=0) - points
    lengths = np.linalg.norm(edges, axis=1)
    live = lengths > tolerance
    points, edges, lengths = points[live], edges[live], lengths[live]
    dx = anchors[None, :, 0] - points[:, None, 0]
    dy = anchors[None, :, 1] - points[:, None, 1]
    parameter = (dx * edges[:, None, 0] + dy * edges[:, None, 1]) / lengths[
        :, None
    ] ** 2
    distance = abs(dx * edges[:, None, 1] - dy * edges[:, None, 0]) / lengths[:, None]
    guard = tolerance / lengths[:, None]
    a, b = np.nonzero(
        (parameter > guard) & (parameter < 1 - guard) & (distance <= tolerance)
    )
    rows = np.r_[np.arange(len(points)), a]
    stations = np.r_[np.zeros(len(points)), parameter[a, b]]
    result = np.vstack((points, anchors[b]))
    return result[np.lexsort((stations, rows))]


def stitch_contacts(loops, tolerance):
    points = shared_points(np.concatenate([p for _, p in loops]), tolerance)
    coordinates, labels = np.unique(points, axis=0, return_inverse=True)
    starts, ends, owners, offset = [], [], [], 0
    for ids, p in loops:
        count = len(p)
        ring = labels[offset : offset + count]
        starts.extend(ring)
        ends.extend(np.roll(ring, -1))
        owners.extend(ids)
        offset += count
    starts, ends, owners = np.array(starts), np.array(ends), np.array(owners)
    incoming_count = np.bincount(ends, minlength=len(coordinates))
    outgoing_count = np.bincount(starts, minlength=len(coordinates))
    if not np.array_equal(incoming_count, outgoing_count):
        raise ValueError("Unbalanced directed contour contact")
    outgoing_edge = np.empty(len(coordinates), dtype=int)
    outgoing_edge[starts] = np.arange(len(starts))
    following = outgoing_edge[ends]
    for node in np.flatnonzero(outgoing_count > 1):
        incoming = np.flatnonzero(ends == node)
        outgoing = np.flatnonzero(starts == node)
        directions = coordinates[ends[outgoing]] - coordinates[node]
        angles = np.arctan2(directions[:, 1], directions[:, 0])
        selected = []
        for edge in incoming:
            back = coordinates[starts[edge]] - coordinates[node]
            turn = (np.arctan2(back[1], back[0]) - angles) % (2 * np.pi)
            selected.append(outgoing[int(np.argmin(turn))])
        if len(set(selected)) != len(outgoing):
            raise ValueError("Ambiguous directed contour pairing")
        following[incoming] = selected
    used, result = np.zeros(len(starts), dtype=bool), []
    for first in range(len(starts)):
        if used[first]:
            continue
        ring, edge = [], first
        while not used[edge]:
            used[edge] = True
            ring.append(edge)
            edge = following[edge]
        if edge != first:
            raise ValueError("Contour contact walk joined a different cycle")
        ids = owners[ring]
        corners = ids != np.roll(ids, 1)
        if np.count_nonzero(corners) < 3:
            raise ValueError("Contour has fewer than three source-plane corners")
        result.append((ids[corners], coordinates[starts[ring]][corners]))
    return result


def contours(loops, normals, intercepts, now, tolerance):
    segments = np.concatenate(
        [np.stack((p, np.roll(p, -1, axis=0)), axis=1) for _, p in loops]
    )
    ids = np.concatenate([ids for ids, _ in loops])
    valid = np.linalg.norm(segments[:, 1] - segments[:, 0], axis=1) > tolerance
    if not np.any(valid):
        return []
    segments, ids = segments[valid], ids[valid]
    coordinates = np.unique(segments.reshape(-1, 2), axis=0)
    anchors = shapely.multipoints(coordinates)
    lines = []
    for _, points in loops:
        points = subdivide_ring(points, coordinates, tolerance)
        if len(points) >= 2:
            lines.extend(
                shapely.linestrings(
                    np.stack((points, np.roll(points, -1, axis=0)), axis=1)
                )
            )
    network = shapely.union_all(lines)
    faces = shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    keep = [
        p
        for p in faces
        if p.area > tolerance * p.length * 8
        and winding(segments, np.array(p.representative_point().coords)[0]) > 0
    ]
    material = shapely.orient_polygons(shapely.union_all(keep))
    signed_area = 0.0
    for _, points in loops:
        relative = points - points[0]
        following = np.roll(relative, -1, axis=0)
        signed_area += float(
            np.sum(relative[:, 0] * following[:, 1] - relative[:, 1] * following[:, 0])
            * 0.5
        )
    if abs(material.area - signed_area) > max(1e-7, abs(signed_area) * 1e-10):
        raise ValueError("Contour noding lost finite wavefront area")
    result = []
    source_lines = shapely.linestrings(segments)
    source_normals = normals[ids]
    for polygon in shapely.get_parts(material):
        for ring in (polygon.exterior, *polygon.interiors):
            ring = shapely.snap(ring, anchors, tolerance)
            points = subdivide_ring(
                np.asarray(ring.coords)[:-1], coordinates, tolerance
            )
            if len(points) * len(segments) > 2_000_000:
                raise SkeletonBudgetExceeded(
                    "Contour ownership candidate budget exceeded"
                )
            following = np.roll(points, -1, axis=0)
            midpoint = shapely.points((points + following) * 0.5)
            distances = shapely.distance(midpoint[:, None], source_lines[None, :])
            start_height = points @ source_normals.T + intercepts[ids] - now
            end_height = following @ source_normals.T + intercepts[ids] - now
            match = (
                (distances <= tolerance)
                & (abs(start_height) <= tolerance * 8)
                & (abs(end_height) <= tolerance * 8)
            )
            a, b = np.nonzero(match)
            direction = np.sum(
                (following[a] - points[a]) * (segments[b, 1] - segments[b, 0]), axis=1
            )
            match[a, b] &= (
                (direction > 0)
                & (
                    shapely.distance(shapely.points(points[a]), source_lines[b])
                    <= tolerance
                )
                & (
                    shapely.distance(shapely.points(following[a]), source_lines[b])
                    <= tolerance
                )
            )
            minimum = np.min(np.where(match, ids[None, :], len(normals)), axis=1)
            maximum = np.max(np.where(match, ids[None, :], -1), axis=1)
            if np.any(minimum != maximum):
                raise ValueError("Contour edge has ambiguous source provenance")
            result.append((minimum, points))
    if result:
        result = stitch_contacts(result, tolerance)
    return result

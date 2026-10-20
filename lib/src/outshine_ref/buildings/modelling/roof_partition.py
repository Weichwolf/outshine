"""Bounded SFCGAL-free hip-roof partition reference; native ports remain separate."""

import time
from dataclasses import replace

import numpy as np
import shapely

from ._roof_contours import contours, shared_points
from ._roof_facets import assemble_patches
from .skeleton_plan import (
    SkeletonBudgetExceeded,
    SkeletonLimits,
    finish,
    input_data,
    rectangle_data,
    rectangular,
)


def velocities(ids, normals):
    left, right = normals[np.roll(ids, 1)], normals[ids]
    determinant = left[:, 0] * right[:, 1] - left[:, 1] * right[:, 0]
    parallel = abs(determinant) < 1e-12
    if np.any(parallel & (np.sum(left * right, axis=1) < 0)):
        raise ValueError("Opposite parallel fronts did not extinguish")
    velocity = right.copy()
    moving = ~parallel
    velocity[moving, 0] = (right[moving, 1] - left[moving, 1]) / determinant[moving]
    velocity[moving, 1] = (left[moving, 0] - right[moving, 0]) / determinant[moving]
    return velocity


def collision(loops, motions, normals, intercepts, now, tolerance):
    points = np.concatenate([p for _, p in loops])
    speed = np.concatenate(motions)
    ends = np.concatenate([np.roll(p, -1, axis=0) for _, p in loops])
    end_speed = np.concatenate([np.roll(v, -1, axis=0) for v in motions])
    ids = np.concatenate([ids for ids, _ in loops])
    previous = np.concatenate([np.roll(ids, 1) for ids, _ in loops])
    following = np.concatenate(
        [
            np.roll(np.arange(offset, offset + len(p)), -1)
            for offset, (_, p) in zip(
                np.cumsum([0] + [len(p) for _, p in loops[:-1]]), loops, strict=True
            )
        ]
    )
    direction = np.c_[normals[ids, 1], -normals[ids, 0]]
    length = np.sum((ends - points) * direction, axis=1)
    length_rate = np.sum((end_speed - speed) * direction, axis=1)
    edge_times = np.full(len(points), np.inf)
    np.divide(
        -length,
        length_rate,
        out=edge_times,
        where=(length > 0) & (length_rate < -1e-12),
    )
    gap = points @ normals[ids].T + intercepts[ids] - now
    relative = speed @ normals[ids].T - 1
    dt = np.full_like(gap, np.inf)
    closing = (relative < -1e-12) & (gap > tolerance)
    np.divide(-gap, relative, out=dt, where=closing)
    a, b = np.nonzero(np.isfinite(dt) & (dt > 0))
    times = dt[a, b]
    target = points[a] + speed[a] * times[:, None]
    start = points[b] + speed[b] * times[:, None]
    end = ends[b] + end_speed[b] * times[:, None]
    edge = end - start
    length2 = np.sum(edge * edge, axis=1)
    along = np.sum((target - start) * edge, axis=1)
    valid = (along >= -tolerance * np.sqrt(length2)) & (
        along <= length2 + tolerance * np.sqrt(length2)
    )
    valid &= (length2 != 0) | (np.linalg.norm(target - start, axis=1) <= tolerance)
    if not np.any(valid) and not np.any(np.isfinite(edge_times)):
        raise ValueError("No forward contour contact before extinction")
    minimum = min(
        float(times[valid].min()) if np.any(valid) else np.inf, float(edge_times.min())
    )
    simultaneous = valid & (
        abs(times - minimum) <= 64 * np.finfo(float).eps * max(1, now + minimum)
    )
    events = []
    for vertex, edge, point in zip(
        a[simultaneous], b[simultaneous], target[simultaneous], strict=True
    ):
        planes = np.array([previous[vertex], ids[vertex], ids[edge]])
        matrix = np.c_[normals[planes], -np.ones(3)]
        solution, _, rank, _ = np.linalg.lstsq(matrix, -intercepts[planes], rcond=None)
        if rank == 3:
            point = solution[:2]
        events.append(([vertex], point))
    for edge in np.flatnonzero(
        abs(edge_times - minimum) <= 64 * np.finfo(float).eps * max(1, now + minimum)
    ):
        vertex = following[edge]
        planes = np.array([previous[edge], ids[edge], ids[vertex]])
        matrix = np.c_[normals[planes], -np.ones(3)]
        solution, _, rank, _ = np.linalg.lstsq(matrix, -intercepts[planes], rcond=None)
        best = (
            edge
            if np.linalg.norm(speed[edge]) < np.linalg.norm(speed[vertex])
            else vertex
        )
        point = solution[:2] if rank == 3 else points[best] + speed[best] * minimum
        events.append(([edge, vertex], point))
    return minimum, len(points) ** 2, events


def prepare(polygon, limits=None, work=None):
    if limits is None:
        limits = SkeletonLimits()
    if work is None:
        work = {}
    began = time.perf_counter()
    origin, local, rings, boundaries = input_data(polygon, limits)
    if local.minimum_clearance < 1e-6:
        raise ValueError("Roof input has insufficient clearance")
    if rectangular(rings):
        work.update(
            steps=0, collision_pairs=0, swept_pieces=0, maximum_front_vertices=0
        )
        return finish(
            origin, local, boundaries, rectangle_data(rings), "rectangle", limits, began
        )
    owners = list(boundaries.values())
    normals = np.array([x[1] for x in owners])
    intercepts = np.array([x[2] for x in owners])
    loops, offset = [], 0
    for ring in rings:
        loops.append((np.arange(offset, offset + len(ring)), ring))
        offset += len(ring)
    tolerance = max(
        1e-12, 2048 * np.finfo(float).eps * max(1, np.max(abs(np.concatenate(rings))))
    )
    now, steps, tests, pieces = 0.0, 0, 0, 0
    patches = [[] for _ in owners]
    while loops:
        if (
            steps >= limits.python_events
            or time.perf_counter() - began > limits.seconds
        ):
            raise SkeletonBudgetExceeded("Roof contour work/deadline exceeded")
        count = sum(len(points) for _, points in loops)
        work["maximum_front_vertices"] = max(
            work.get("maximum_front_vertices", 0), count
        )
        if tests + count * count > 2_000_000:
            raise SkeletonBudgetExceeded("Roof collision pair budget exceeded")
        if pieces + count > 16384:
            raise SkeletonBudgetExceeded("Roof swept-piece budget exceeded")
        motions = [velocities(ids, normals) for ids, _ in loops]
        dt, count, events = collision(
            loops, motions, normals, intercepts, now, tolerance
        )
        work["contact_event_solves"] = work.get("contact_event_solves", 0) + len(events)
        tests += count
        before = np.concatenate([p for _, p in loops])
        after = before + np.concatenate(motions) * dt
        for participants, point in events:
            after[participants] = point
        after = shared_points(after, tolerance)
        updated, offset = [], 0
        for ids, points in loops:
            next_points = after[offset : offset + len(points)]
            updated.append((ids, next_points))
            tangent = np.c_[normals[ids, 1], -normals[ids, 0]]
            quad = np.empty((len(points), 4, 2))
            quad[:, :, 0] = np.array(
                [
                    np.sum(points * tangent, axis=1),
                    np.sum(np.roll(points, -1, axis=0) * tangent, axis=1),
                    np.sum(np.roll(next_points, -1, axis=0) * tangent, axis=1),
                    np.sum(next_points * tangent, axis=1),
                ]
            ).T
            quad[:, :, 1] = (now, now, now + dt, now + dt)
            batch = shapely.polygons(quad)
            positive = shapely.area(batch) > 0
            if not np.all(shapely.is_valid(batch[positive])):
                raise ValueError("Source sweep crossed before a registered contact")
            pieces += int(positive.sum())
            for owner, patch in zip(ids[positive], batch[positive], strict=True):
                patches[owner].append(patch)
            offset += len(points)
        now += dt
        loops = contours(updated, normals, intercepts, now, tolerance)
        steps += 1
        if sum(len(points) for _, points in loops) > limits.vertices * 4:
            raise SkeletonBudgetExceeded("Roof front vertex budget exceeded")
    nodes, distances, faces, ownership = assemble_patches(
        patches, rings, boundaries, tolerance, limits, work
    )
    if work is not None:
        work.update(
            steps=steps,
            collision_pairs=tests,
            swept_pieces=pieces,
            tolerance_m=tolerance,
            named_collision_array_payload_bytes=tests * 3 * 8,
        )
    plan = finish(
        origin,
        local,
        boundaries,
        (nodes, distances, tuple(faces), steps),
        "contour",
        limits,
        began,
        owners=ownership,
    )
    interface_correction = (
        0.0 if work is None else work["maximum_interface_refinement_m"]
    )
    total_correction = interface_correction + plan.maximum_refinement_m
    if total_correction > 5e-5:
        raise ValueError(
            "Combined roof refinement exceeds the existing metric correction bound"
        )
    return replace(plan, maximum_refinement_m=total_correction)

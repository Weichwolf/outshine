"""Whole projected profiles and opposite vertical walls of actual cached modules."""

from itertools import pairwise

import numpy as np
import shapely
from shapely.affinity import scale
from stair_cycle_contacts import profile
from stair_module_sections import profile_levels, wall_strip


def wall_patch(
    triangles, a, axis, length, low, high, xy_guard, *, expected=None, z_guard=1e-7
):
    delta = triangles[:, :, :2] - a
    station = delta @ axis
    distance = abs(delta[:, :, 0] * axis[1] - delta[:, :, 1] * axis[0])
    keep = (
        (distance.max(axis=1) <= xy_guard)
        & (station.max(axis=1) > 0)
        & (station.min(axis=1) < length)
    )
    projected = np.stack([station[keep], triangles[keep, :, 2] - low], axis=2)
    polygons = shapely.polygons(projected)
    positive = shapely.area(polygons) > 1e-14
    polygons, projected = polygons[positive], projected[positive]
    rectangle = shapely.box(0, 0, length, high - low) if expected is None else expected
    intersection = shapely.intersection(polygons, rectangle)
    used = shapely.area(intersection) > 1e-14
    if not used.any():
        raise ValueError("whole vertical module wall missing")
    q = projected[used]
    u, v = q[:, 1] - q[:, 0], q[:, 2] - q[:, 0]
    signs = np.unique(np.sign(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]))
    if len(signs) != 1 or signs[0] == 0:
        raise ValueError("inconsistent module wall orientation")
    cover = shapely.union_all(intersection[used])
    normalized = scale(cover, xfact=1 / xy_guard, yfact=1 / z_guard, origin=(0, 0))
    envelope = scale(normalized.buffer(1), xfact=xy_guard, yfact=z_guard, origin=(0, 0))
    missing = rectangle.difference(envelope).area
    if missing > 1e-7:
        raise ValueError("whole vertical module contact has an uncovered region")
    return {
        "faces": int(used.sum()),
        "orientation": int(signs[0]),
        "uncovered_area_m2": float(missing),
        "maximum_plane_distance_m": float(distance[keep][positive][used].max()),
    }


def interfaces(old_body, module, old_footprint, domains, height, *, reference_cap=None):
    desired = old_footprint.boundary.intersection(shapely.union_all(domains).boundary)
    segments = [
        np.array([a, b])
        for line in shapely.get_parts(desired)
        if line.geom_type == "LineString"
        for a, b in pairwise(line.coords)
        if np.linalg.norm(np.array(a) - b) > 1e-7
    ]
    if not 1 <= len(segments) <= 128:
        raise ValueError("bounded nonempty whole module interface required")
    inputs = [
        (old_body["body_vertices"], old_body["body_faces"]),
        (module["body_vertices"], module["body_faces"]),
    ]
    if any(len(faces) > 32768 for _vertices, faces in inputs):
        raise ValueError("module interface body face cap")
    result = []
    for cast in (False, True):
        triangles = [
            (vertices.astype("f4").astype(float) if cast else vertices)[faces]
            for vertices, faces in inputs
        ]
        errors = [
            np.linalg.norm(
                vertices.astype("f4").astype(float)[:, :2] - vertices[:, :2], axis=1
            ).max()
            for vertices, _faces in inputs
        ]
        for segment in segments:
            a, b = segment
            length = float(np.linalg.norm(b - a))
            axis = (b - a) / length
            # The canonical Double plane stays fixed. Rounding a short segment
            # into a new plane amplifies its angular error along a long old wall.
            broad = 1e-7 + sum(errors) if cast else 1e-7
            local, local_z = [], []
            for xyz, (vertices, faces) in zip(triangles, inputs, strict=True):
                delta = xyz[:, :, :2] - a
                stations = delta @ axis
                distance = abs(delta[:, :, 0] * axis[1] - delta[:, :, 1] * axis[0])
                near = (
                    (distance.max(axis=1) <= broad)
                    & (stations.max(axis=1) > 0)
                    & (stations.min(axis=1) < length)
                )
                ids = np.unique(faces[near])
                local_z.append(
                    float(
                        abs(
                            vertices[ids, 2].astype("f4").astype(float)
                            - vertices[ids, 2]
                        ).max(initial=0)
                    )
                )
                local.append(
                    float(
                        np.linalg.norm(
                            vertices[ids, :2].astype("f4").astype(float)
                            - vertices[ids, :2],
                            axis=1,
                        ).max(initial=0)
                    )
                )
            guard = 1e-7 + sum(local) if cast else 1e-7
            low, high = height - 0.3, height
            if cast:
                low, high = float(np.float32(low)), float(np.float32(high))
            expected, z_guard = None, 1e-7
            if reference_cap is not None:
                expected, low, z_guard = wall_strip(reference_cap, segment, cast, guard)
                high = low + expected.bounds[3]
                if cast:
                    z_guard += sum(local_z)
            checks = [
                wall_patch(
                    xyz,
                    a,
                    axis,
                    length,
                    low,
                    high,
                    guard,
                    expected=expected,
                    z_guard=z_guard,
                )
                for xyz in triangles
            ]
            if checks[0]["orientation"] == checks[1]["orientation"]:
                raise ValueError("module contact walls are not oppositely oriented")
            result.append(
                {
                    "float32": cast,
                    "segment_xy_m": segment.tolist(),
                    "length_m": length,
                    "height_m": high - low
                    if expected is None
                    else expected.area / length,
                    "xy_guard_m": guard,
                    "z_guard_m": z_guard,
                    "local_vertex_z_rounding_m": local_z
                    if reference_cap is not None and cast
                    else [],
                    "sides": checks,
                }
            )
    return {
        "segments": len(segments),
        "length_m": sum(np.linalg.norm(s[1] - s[0]) for s in segments),
        "checks": result,
        "face_tests": 2 * len(segments) * sum(len(f) for _v, f in inputs),
    }


def cap_cover(data, owner, mask, line, borrowed, cast, *, piecewise=False):
    vertices = (
        data["surface_vertices"].astype("f4").astype(float)
        if cast
        else data["surface_vertices"]
    )
    shapes = shapely.polygons(vertices[data["surface_faces"], :2])
    points = line.astype("f4").astype(float) if cast else line
    guard = (
        max(1e-7, 2 * float(abs(np.spacing(points.astype("f4"))).max()))
        if cast
        else 1e-7
    )
    selected = data["owners"][:, owner] & shapely.dwithin(
        mask, shapely.centroid(shapes), 1e-8
    )
    selected &= shapely.dwithin(shapely.LineString(points), shapes, guard)
    levels = vertices[data["surface_faces"][selected], 2].ravel()
    if piecewise:
        levels = profile_levels(vertices, data["surface_faces"][selected], points)
    pieces = list(shapes[selected])
    if not borrowed.is_empty:
        pieces.append(borrowed)
    if not pieces:
        raise ValueError("source lacks actual profile ownership")
    cover = shapely.union_all(pieces)
    missing = shapely.LineString(points).difference(cover.buffer(guard)).length
    if missing > 1e-7 or (len(levels) and np.ptp(levels) > 1e-7):
        raise ValueError("whole module profile is missing or not flat")
    return levels, {
        "float32": cast,
        "xy_guard_m": guard,
        "missing_length_m": float(missing),
    }


def profiles(
    data, rows, nodes, masks, original, frozen_cap, frozen_line, *, piecewise=False
):
    result, outer = [], []
    root = next(
        i for i, n in enumerate(nodes) if np.array_equal(n["xy"], frozen_line[1, :2])
    )
    for owner, row in enumerate(rows):
        for end in (0, -1):
            choices = [
                i
                for i, n in enumerate(nodes)
                if owner in n["incident"] and np.array_equal(n["xy"], row["xy"][end])
            ]
            if not choices:
                outer.append({"source_id": row["source_id"], "end": end})
                continue
            index = choices[0]
            line = profile(row, end)
            checks = []
            for cast in (False, True):
                v = (
                    frozen_cap.vertices.astype("f4").astype(float)
                    if cast
                    else frozen_cap.vertices
                )
                old = shapely.union_all(shapely.polygons(v[frozen_cap.triangles, :2]))
                owned_borrow = (
                    old.intersection(original[owner]).intersection(masks[index])
                    if index == root
                    else shapely.Polygon()
                )
                levels, note = cap_cover(
                    data,
                    owner,
                    masks[index],
                    line,
                    owned_borrow,
                    cast,
                    piecewise=piecewise,
                )
                neighbours = [i for i in nodes[index]["incident"] if i != owner]
                other = []
                for i in neighbours:
                    borrow = (
                        old.intersection(original[i]).intersection(masks[index])
                        if index == root
                        else shapely.Polygon()
                    )
                    value, _ = cap_cover(
                        data, i, masks[index], line, borrow, cast, piecewise=piecewise
                    )
                    other.extend(value)
                both = np.r_[
                    levels,
                    other,
                    float(np.float32(frozen_line[1, 2])) if cast else frozen_line[1, 2],
                ]
                if np.ptp(both) > 1e-7:
                    raise ValueError("old/new whole profile heights differ")
                checks.append(note)
            result.append({"source_id": row["source_id"], "end": end, "checks": checks})
    # The old outer profile must be supported by new logical owners as well.
    for cast in (False, True):
        for owner in nodes[root]["incident"]:
            v = (
                frozen_cap.vertices.astype("f4").astype(float)
                if cast
                else frozen_cap.vertices
            )
            old = shapely.union_all(shapely.polygons(v[frozen_cap.triangles, :2]))
            borrowed = old.intersection(original[owner]).intersection(masks[root])
            cap_cover(
                data,
                owner,
                masks[root],
                frozen_line[:, :2],
                borrowed,
                cast,
                piecewise=piecewise,
            )
    return {
        "new_internal_profiles": result,
        "old_profile_completed": True,
        "outer_profiles": outer,
        "complete_additional_profiles": len(result) + 1,
    }

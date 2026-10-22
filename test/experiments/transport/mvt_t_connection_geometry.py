"""Small Float64 construction predicates; no original-node identity inference."""

import math
from collections import defaultdict
from itertools import pairwise


def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def difference(a, b):
    return (a[0] - b[0], a[1] - b[1])


def segment_records(rows):
    segments = []
    for row in rows:
        station = 0.0
        for i, (a, b) in enumerate(pairwise(row["xy_m"])):
            length = math.dist(a, b)
            segments.append((row, i, a, b, station, length))
            station += length
    return segments


def ground_foot(row):
    return (
        row["layer"] is not None
        and row["bridge"] is False
        and row["tunnel"] is False
        and row["foot"] is True
    )


def connection_candidate(branch, end, segment, reach_m, min_sine):
    target, index, a, b, station, length = segment
    if branch["source_id"] == target["source_id"]:
        return None
    if not ground_foot(target) or branch["layer"] != target["layer"]:
        return None
    if target["kind"] not in ("path", "footway", "pedestrian"):
        return None
    point = branch["xy_m"][end]
    direction = difference(point, branch["xy_m"][1 if end == 0 else -2])
    magnitude = math.hypot(*direction)
    unit = tuple(v / magnitude for v in direction)
    axis = difference(b, a)
    delta = difference(a, point)
    denominator = cross(unit, axis)
    if not denominator:
        return None
    sine = abs(denominator) / length
    if sine < min_sine:
        return None
    advance = cross(delta, axis) / denominator
    fraction = cross(delta, unit) / denominator
    limit = min(reach_m, target["half_width_m"])
    resolutions = (branch["position_error_bound_m"], target["position_error_bound_m"])
    if all(v is not None for v in resolutions):
        limit = min(limit, sum(resolutions))
    if not 0 <= advance <= limit or not 0 <= fraction <= 1:
        return None
    target_station = station + fraction * length
    total = target["length_m"]
    if min(target_station, total - target_station) < branch["half_width_m"]:
        return None
    # Use an original target vertex verbatim when the intersection is its endpoint.
    join = (
        a
        if fraction == 0
        else b
        if fraction == 1
        else tuple(x + fraction * d for x, d in zip(a, axis, strict=True))
    )
    return {
        "construction_id": [
            "derived-t",
            branch["source_id"],
            end,
            target["source_id"],
            index,
        ],
        "source_id": branch["source_id"],
        "source_endpoint": end,
        "source_original_xy_m": list(point),
        "target_source_id": target["source_id"],
        "target_segment": index,
        "target_fraction": fraction,
        "target_station_m": target_station,
        "xy_m": list(join),
        "advance_m": advance,
        "crossing_sine": sine,
        "position_basis": "coincident_original_coordinate"
        if advance == 0
        else "bounded_forward_extension_recipe",
        "provider_position_bound_known": all(v is not None for v in resolutions),
        "source_node_identity_claimed": False,
    }


def constructed_axes(rows, connections):
    insertions, extensions = defaultdict(lambda: defaultdict(list)), defaultdict(list)
    for contact in connections:
        insertions[contact["target_source_id"]][contact["target_segment"]].append(
            contact
        )
        extensions[contact["source_id"]].append(contact)
    axes = {}
    for row in rows:
        key = row["source_id"]
        xy = [list(row["xy_m"][0])]
        for i, (_a, b) in enumerate(pairwise(row["xy_m"])):
            for contact in sorted(
                insertions[key].get(i, []), key=lambda c: c["target_fraction"]
            ):
                if 0 < contact["target_fraction"] < 1 and contact["xy_m"] != xy[-1]:
                    xy.append(contact["xy_m"])
            xy.append(list(b))
        for contact in extensions[key]:
            if contact["advance_m"] > 0:
                if contact["source_endpoint"] == 0:
                    xy.insert(0, contact["xy_m"])
                else:
                    xy.append(contact["xy_m"])
        axes[key] = xy
    return axes

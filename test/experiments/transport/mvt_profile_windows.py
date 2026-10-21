"""Bounded source-station requests around accepted local constructions; no invented ports."""

import math
from collections import defaultdict
from itertools import pairwise

import numpy as np
from mvt_t_connection_geometry import ground_foot
from mvt_t_connection_inputs import source_rows


def _node_lookup(way):
    nodes = defaultdict(set)
    for row in way.get("sampled_design_xy_height_node", []):
        if row[3]:
            nodes[tuple(row[:2])].add(row[3])

    def at(point):
        found = nodes.get(tuple(point), ())
        return next(iter(found)) if len(found) == 1 else None

    return at


def _context(case):
    ways = case["ways"]
    if (
        not 1 <= len(ways) <= 64
        or any(not 2 <= len(w["source_xy_m"]) <= 256 for w in ways)
        or sum(len(w["source_xy_m"]) for w in ways) > 4096
        or sum(len(w.get("sampled_design_xy_height_node", [])) for w in ways) > 8192
    ):
        raise ValueError("bounded original source/observation buffers required")
    sources = source_rows(ways)
    axes = {}
    groups = defaultdict(list)
    for source, way in zip(sources, ways, strict=True):
        if any(
            len(row) != 4
            or type(row[3]) is not int
            or not 0 <= row[3] < 2**64
            or any(not math.isfinite(x) or abs(x) > 1e7 for x in row[:3])
            for row in way.get("sampled_design_xy_height_node", [])
        ):
            raise ValueError(
                "finite observed XYZ and exact uint64 native-node IDs required"
            )
        key = source["source_id"]
        xy = np.array(source["xy_m"], float)
        if (
            key in axes
            or xy.shape != (len(xy), 2)
            or not np.isfinite(xy).all()
            or abs(xy).max() > 1e7
        ):
            raise ValueError("unique finite bounded original axes required")
        lengths = np.array([math.dist(a, b) for a, b in pairwise(xy)])
        if (lengths <= 0).any():
            raise ValueError("positive original source segments required")
        stations = np.r_[0, np.cumsum(lengths)]
        axes[key] = (xy, stations, source)
        node_at = _node_lookup(way)
        for index, point in enumerate(way["source_xy_m"]):
            node = node_at(point)
            groups[tuple(point)].append(
                (key, float(stations[index]), node, source["layer"])
            )
    contacts = []
    for xy, members in sorted(groups.items()):
        if len(members) < 2:
            continue
        valid = (
            len({m[0] for m in members}) == len(members)
            and len({m[2] for m in members}) == 1
            and members[0][2] is not None
            and len({m[3] for m in members}) == 1
        )
        contacts.append(
            {
                "xy_m": list(xy),
                "source_stations_m": {
                    key: station for key, station, _node, _layer in members
                },
                "status": "observed_native_contact"
                if valid
                else "unqualified_coincidence",
            }
        )
    return axes, contacts


def _station_request(key, axis, station, side):
    xy, arc, _source = axis
    index = min(
        len(xy) - 2, max(0, int(np.searchsorted(arc, station, side="right")) - 1)
    )
    fraction = (station - arc[index]) / (arc[index + 1] - arc[index])
    point = xy[index] + fraction * (xy[index + 1] - xy[index])
    tangent = (xy[index + 1] - xy[index]) / (arc[index + 1] - arc[index])
    return {
        "source_id": key,
        "source_station_m": float(station),
        "side": side,
        "source_segment": index,
        "center_xy_m": point.tolist(),
        "tangent_xy": tangent.tolist(),
        "status": "native_complete_profile_required",
        "station_scope": "Original source arclength, forward segment at a knot (last uses preceding); not a native navigation station or a free endpoint.",
    }


def _catalog(axes, interfaces):
    catalog = defaultdict(list)
    seen = set()
    for port in interfaces:
        key = port["source_id"]
        station = port["source_station_m"]
        identity = (key, station)
        if (
            key not in axes
            or not math.isfinite(station)
            or not 0 < station < axes[key][1][-1]
            or identity in seen
            or not np.array_equal(port["original_source_xy_m"], axes[key][0])
            or "profile_xyz_m" not in port
            or not port.get("native_faces")
        ):
            raise ValueError(
                "unique true interior candidate profiles must retain their original source axis"
            )
        seen.add(identity)
        catalog[key].append(float(station))
    return catalog


def _windows(axes, targets, seed_source_id, ends, padding_m, catalog):
    requests = []
    windows = [
        {
            "source_id": seed_source_id,
            "source_station_range_m": [0, float(axes[seed_source_id][1][-1])],
            "role": "whole_changed_source",
            "endpoint_roles": {
                str(end): "accepted_construction"
                if end in ends
                else "unknown_boundary_requires_contract"
                for end in (0, -1)
            },
        }
    ]
    reasons = [] if len(ends) == 2 else ["seed_endpoint_requires_boundary_contract"]
    for key, stations in sorted(targets.items()):
        length = float(axes[key][1][-1])
        low = max(0, min(stations) - padding_m)
        high = min(length, max(stations) + padding_m)
        before = [p for p in catalog[key] if p <= low]
        after = [p for p in catalog[key] if p >= high]
        pair = [max(before) if before else None, min(after) if after else None]
        interval = [
            low if pair[0] is None else pair[0],
            high if pair[1] is None else pair[1],
        ]
        status = (
            "captured_profile_candidates"
            if all(p is not None for p in pair)
            else "native_complete_profiles_required"
        )
        if status == "native_complete_profiles_required":
            reasons.append("missing_native_profile:" + key)
            requests.extend(
                _station_request(key, axes[key], station, side)
                for station, side, port in zip(
                    (low, high), ("lower", "upper"), pair, strict=True
                )
                if port is None
            )
        if low == 0 or high == length:
            reasons.append("source_end_context_required:" + key)
        windows.append(
            {
                "source_id": key,
                "source_station_range_m": interval,
                "requested_range_m": [low, high],
                "role": status,
                "captured_profile_keys": [[key, p] for p in pair if p is not None],
            }
        )
    return windows, requests, reasons


def _joint_plan_reasons(observed, connections, all_connections, ranges):
    reasons = []
    requirements = []
    for contact in observed:
        active = [
            key
            for key, s in contact["source_stations_m"].items()
            if key in ranges and ranges[key][0] <= s <= ranges[key][1]
        ]
        represented = any(
            contact["xy_m"] == c["source_original_xy_m"]
            and set(contact["source_stations_m"])
            <= {c["source_id"], c["target_source_id"]}
            for c in connections
        )
        if active and (
            not represented or contact["status"] != "observed_native_contact"
        ):
            reasons.append("observed_contact_requires_joint_plan")
            requirements.append(
                {"reason": "observed_contact_requires_joint_plan", **contact}
            )
    for connection in all_connections:
        if connection in connections:
            continue
        target = connection["target_source_id"]
        if (
            target in ranges
            and ranges[target][0] <= connection["target_station_m"] <= ranges[target][1]
        ):
            reasons.append("additional_construction_requires_joint_plan")
            requirements.append(
                {
                    "reason": "additional_construction_requires_joint_plan",
                    "source_id": connection["source_id"],
                    "target_source_id": target,
                    "target_station_m": connection["target_station_m"],
                }
            )
    return reasons, requirements


def select_window(
    case,
    proposal,
    seed_source_id,
    *,
    padding_m,
    interfaces=(),
    max_sources=8,
    max_span_m=128,
):
    """Select one seed's finite window; optional captured profiles are candidates.

    The seed is the entire changed source. Its accepted endpoint constructions
    request padded intervals on their target axes. Existing true profiles may
    enclose those requests; no fabricated cross-height replaces missing profiles.
    Other constructions/observed contacts in the selected intervals require an
    explicitly joint plan. A caller may retry a larger explicit padding after a
    failed fit, under its own finite attempt cap; this function does no fitting.
    """
    if (
        not math.isfinite(padding_m)
        or not 0 < padding_m <= 128
        or type(max_sources) is not int
        or not 1 <= max_sources <= 64
        or not math.isfinite(max_span_m)
        or not 0 < max_span_m <= 8192
    ):
        raise ValueError("explicit bounded window padding/source/span limits required")
    if len(proposal["connections"]) > 128 or len(interfaces) > 64:
        raise ValueError("bounded construction and candidate-port collections required")
    axes, observed = _context(case)
    if seed_source_id not in axes:
        raise ValueError("known changed seed source required")
    connections = [
        c for c in proposal["connections"] if c["source_id"] == seed_source_id
    ]
    if not connections:
        return {
            "status": "deferred",
            "reason": "no_accepted_seed_construction",
            "windows": [],
            "profile_requests": [],
        }
    ends = [c["source_endpoint"] for c in connections]
    if any(end not in (0, -1) for end in ends) or len(set(ends)) != len(ends):
        raise ValueError("at most one accepted construction per seed endpoint required")
    targets = defaultdict(list)
    for connection in connections:
        key = connection["target_source_id"]
        station = connection["target_station_m"]
        if (
            key not in axes
            or key == seed_source_id
            or not math.isfinite(station)
            or not 0 <= station <= axes[key][1][-1]
        ):
            raise ValueError(
                "accepted construction needs a known bounded target station"
            )
        targets[key].append(float(station))
    selected_keys = [seed_source_id, *targets]
    if (
        any(not ground_foot(axes[key][2]) for key in selected_keys)
        or len({axes[key][2]["layer"] for key in selected_keys}) != 1
    ):
        raise ValueError(
            "selected construction sources need explicit ground/access semantics"
        )
    if len(targets) + 1 > max_sources:
        return {
            "status": "deferred",
            "reason": "window_source_budget",
            "windows": [],
            "profile_requests": [],
        }
    catalog = _catalog(axes, interfaces)
    windows, requests, reasons = _windows(
        axes, targets, seed_source_id, ends, padding_m, catalog
    )
    ranges = {w["source_id"]: w["source_station_range_m"] for w in windows}
    if (
        len(windows) > max_sources
        or sum(b - a for a, b in ranges.values()) > max_span_m
    ):
        return {
            "status": "deferred",
            "reason": "window_span_budget",
            "windows": [],
            "profile_requests": [],
        }
    joint_reasons, requirements = _joint_plan_reasons(
        observed, connections, proposal["connections"], ranges
    )
    reasons.extend(joint_reasons)
    return {
        "status": "captured_candidate"
        if not reasons
        else "requires_contract_or_expansion",
        "reasons": sorted(set(reasons)),
        "seed_source_id": seed_source_id,
        "padding_m": padding_m,
        "windows": windows,
        "profile_requests": requests,
        "joint_plan_requirements": requirements,
        "unchanged_source_ids": sorted(set(axes) - set(ranges)),
        "work": {
            "input_sources": len(axes),
            "source_vertices": sum(len(a[0]) for a in axes.values()),
            "input_connections": len(proposal["connections"]),
            "candidate_profiles": len(interfaces),
            "selected_sources": len(windows),
            "selected_original_length_m": sum(b - a for a, b in ranges.values()),
        },
        "scope": "Deterministic metadata-only local candidate/request, bounded by declared source/span caps. Unknown ends are never free. No global-neighbour closure, invented boundary heights, fit/body admission, native navstation or hot-path execution.",
    }

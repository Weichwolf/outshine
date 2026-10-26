"""Selected joint regions from exact observed native nodes, with open frontiers."""

import copy
import math
from collections import defaultdict

from mvt_t_connection_geometry import ground_foot
from mvt_t_connection_inputs import source_rows
from mvt_t_connections import plan_connections


def _observations(case):
    ways = case["ways"]
    if (
        not 1 <= len(ways) <= 64
        or any(not 2 <= len(w["source_xy_m"]) <= 256 for w in ways)
        or sum(len(w["source_xy_m"]) for w in ways) > 4096
        or sum(len(w["sampled_design_xy_height_node"]) for w in ways) > 8192
    ):
        raise ValueError("observed region source/vertex/sample budget exceeded")
    groups, identifiers, samples = defaultdict(list), set(), 0
    for way, source in zip(ways, source_rows(ways), strict=True):
        key = source["source_id"]
        if key in identifiers:
            raise ValueError("unique observed source parts required")
        identifiers.add(key)
        nodes = defaultdict(set)
        for row in way["sampled_design_xy_height_node"]:
            if (
                len(row) != 4
                or any(not math.isfinite(v) or abs(v) > 1e7 for v in row[:3])
                or type(row[3]) is not int
                or not 0 <= row[3] < 2**64
            ):
                raise ValueError(
                    "finite observed profile and uint64 native node required"
                )
            if row[3]:
                nodes[tuple(row[:2])].add(row[3])
            samples += 1
        for at, xy in enumerate(way["source_xy_m"]):
            if len(xy) != 2 or any(not math.isfinite(v) or abs(v) > 1e7 for v in xy):
                raise ValueError("finite original source XY required")
            observed = nodes[tuple(xy)]
            if len(observed) > 1:
                raise ValueError(
                    "conflicting observed native nodes at one source point"
                )
            groups[tuple(xy)].append(
                {
                    "source_id": key,
                    "source_point_index": at,
                    "endpoint": 0
                    if at == 0
                    else -1
                    if at == len(way["source_xy_m"]) - 1
                    else None,
                    "native_node": next(iter(observed), None),
                    "layer": source["layer"],
                    "ground_access": ground_foot(source),
                }
            )
    return groups, identifiers, samples


def _group(xy, entries, selected):
    inside = [e for e in entries if e["source_id"] in selected]
    if not inside:
        return None
    node_ids = {e["native_node"] for e in entries}
    valid = (
        len(node_ids) == 1
        and None not in node_ids
        and len({e["source_id"] for e in entries}) == len(entries)
        and len({e["layer"] for e in entries}) == 1
        and all(e["ground_access"] for e in entries)
    )
    return {
        "xy_m": list(xy),
        "native_node": next(iter(node_ids)) if len(node_ids) == 1 else None,
        "members": copy.deepcopy(entries),
        "selected_source_ids": sorted(e["source_id"] for e in inside),
        "outside_source_ids": sorted(
            e["source_id"] for e in entries if e["source_id"] not in selected
        ),
        "status": "observed_compatible_native_contact"
        if valid
        else "unqualified_coincidence",
        "provider_node_identity_claimed": False,
    }


def _frontiers(ways, groups, selected, proposal):
    joins = {tuple(g["xy_m"]): g for g in groups}
    constructions = {
        (c["source_id"], c["source_endpoint"]): c for c in proposal["connections"]
    }
    endpoints = []
    for way in ways:
        source_id = f"native-part:{way['native_way']}"
        if source_id not in selected:
            continue
        for endpoint in (0, -1):
            xy = way["source_xy_m"][endpoint]
            group = joins.get(tuple(xy))
            if (source_id, endpoint) in constructions:
                status = "joint_construction_proposal"
            elif group is None:
                status = "unknown_neighbour_not_free_end"
            elif group["status"] != "observed_compatible_native_contact":
                status = "unqualified_coincidence"
            elif group["outside_source_ids"]:
                status = "observed_outside_contact_requires_port_or_expansion"
            else:
                status = "joint_observed_contact"
            endpoints.append(
                {
                    "source_id": source_id,
                    "endpoint": endpoint,
                    "xy_m": xy,
                    "status": status,
                }
            )
    return endpoints


def region_inputs(case, selected_source_ids):
    """Keep the full capture as evidence; only caller-selected parts change."""
    observed, identifiers, samples = _observations(case)
    selected = set(selected_source_ids)
    if (
        not selected
        or len(selected) != len(selected_source_ids)
        or not selected <= identifiers
    ):
        raise ValueError("unique nonempty observed source selection required")
    ways = [
        copy.deepcopy(w)
        for w in case["ways"]
        if f"native-part:{w['native_way']}" in selected
    ]
    for way in ways:
        way["sampled_design_xyz_m"] = [
            r[:3] for r in way["sampled_design_xy_height_node"]
        ]
    sources = source_rows(ways)
    proposal = plan_connections(sources)
    groups = [
        g
        for xy, entries in sorted(observed.items())
        if len(entries) > 1 and (g := _group(xy, entries, selected)) is not None
    ]
    junctions = []
    for group in groups:
        keys = group["selected_source_ids"]
        if group["status"] != "observed_compatible_native_contact" or len(keys) < 2:
            continue
        existing = [
            c
            for c in proposal["connections"]
            if c["xy_m"] == group["xy_m"]
            and {c["source_id"], c["target_source_id"]} <= set(keys)
        ]
        if existing:
            if len(existing) != 1:
                raise ValueError("observed multi-flight contact needs a shared recipe")
            existing[0]["source_ids"] = keys
            existing[0]["observed_native_node"] = group["native_node"]
            continue
        junctions.append(
            {
                "construction_id": ["observed-native-node", group["native_node"], keys],
                "xy_m": group["xy_m"],
                "source_ids": keys,
                "observed_native_node": group["native_node"],
            }
        )
    proposal["observed_junctions"] = junctions
    proposal["region_context"] = {
        "selected_source_ids": sorted(selected),
        "unselected_sources": [
            {"source_id": key, "reason": "outside_caller_selected_joint_region"}
            for key in sorted(identifiers - selected)
        ],
        "observed_contacts": groups,
        "endpoint_roles": _frontiers(ways, groups, selected, proposal),
        "capture": copy.deepcopy(case),
        "work": {
            "sources": len(identifiers),
            "source_vertices": sum(len(v) for v in observed.values()),
            "sampled_rows": samples,
            "coordinate_groups": len(observed),
            "contact_groups": len(groups),
        },
        "scope": "Joint local proposal. Observed outside contacts need an unchanged full port or expansion; unmatched ends remain unknown. No boundary or native admission.",
    }
    return {**copy.deepcopy(case), "ways": ways}, sources, proposal

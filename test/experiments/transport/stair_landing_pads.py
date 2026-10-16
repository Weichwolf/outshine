"""Bounded missing-profile proposals at already chosen original landing nodes."""

import numpy as np
import shapely
from outshine_ref.transport.junction_pads import profile_junction_pad
from shared_bridge_geometry import corridor
from stair_branch_inputs import shared_cycle_path
from stair_cycle_contacts import profile


def endpoint_profiles(rows, node):
    return [
        (i, end)
        for i in node["incident"]
        for end in (0, -1)
        if np.array_equal(rows[i]["xy"][end], node["xy"])
    ]


def chosen_kinds(rows, ends, node_index, stairs):
    for i, _end in ends:
        row = rows[i]
        props = row["original"]["source"]["feature"]["properties"]
        if (
            (
                props.get("subclass") not in ("cycleway", "footway", "steps")
                and not (
                    row.get("source_recipe", "").startswith("exterior_shared_cycle_")
                    and shared_cycle_path(props)
                )
            )
            or props.get("brunnel") == "tunnel"
            or props.get("indoor")
            or (
                (props.get("level", 0) != 0 or props.get("layer", 0) != 0)
                and not row["original"]["bridge"]
            )
        ):
            return "unsupported_source_kind_or_level"
        if props.get("subclass") == "steps" and not any(
            owner == i and node_index in (low, high) for owner, low, high in stairs
        ):
            return "stair_endpoint_not_a_chosen_landing"
    return ""


def complete_landings(rows, nodes, stairs, *, method="profile_hull", max_pads=16):
    if (
        type(max_pads) is not int
        or not 0 <= max_pads <= 16
        or not 1 <= len(rows) <= 64
        or len(nodes) > 64
        or method not in ("profile_hull", "profile_box")
    ):
        raise ValueError("bounded chosen-landing input and known recipe required")
    domains = [corridor(row) for row in rows]
    additions, pads, ledger = {}, [], []
    for index, node in enumerate(nodes):
        ends = endpoint_profiles(rows, node)
        if len(ends) < 2:
            continue
        gaps = []
        for i, end in ends:
            other = shapely.union_all([domains[j] for j in node["incident"] if j != i])
            shared = domains[i].intersection(other)
            gaps.append(
                shapely.LineString(profile(rows[i], end))
                .difference(shared.buffer(1e-7))
                .length
            )
        note = {
            "node_mvt_xy": node["mvt_xy"],
            "node_xy_m": list(node["xy"]),
            "profiles": [[rows[i]["source_id"], end] for i, end in ends],
            "missing_profile_lengths_before_m": gaps,
        }
        reason = chosen_kinds(rows, ends, index, stairs)
        if max(gaps) <= 1e-7:
            reason = "whole_profiles_already_supported_by_neighbours"
        elif node["mvt_xy"] is None:
            reason = "no_original_coincident_endpoint_node"
        elif len(ends) > 4:
            reason = "whole_profile_count_budget"
        elif not reason and len(pads) >= max_pads:
            reason = "chosen_landing_pad_budget"
        if reason:
            ledger.append(dict(note, status="unchanged", reason=reason))
            continue
        lines = [profile(rows[i], end) for i, end in ends]
        try:
            pad = profile_junction_pad(lines, method=method)
        except ValueError as error:
            ledger.append(dict(note, status="unchanged", reason=str(error)))
            continue
        coordinates = np.array(pad.exterior.coords[:-1])
        for i, _end in ends:
            additions.setdefault(i, []).append(coordinates.copy())
        pads.append(
            dict(
                note,
                method=method,
                pad_xy_m=coordinates.tolist(),
                pad_area_m2=pad.area,
                added_union_area_m2=pad.difference(
                    shapely.union_all([domains[j] for j in node["incident"]])
                ).area,
            )
        )
        ledger.append(
            dict(
                note,
                status="constructed",
                reason="whole_profile_pad",
                product=len(pads) - 1,
            )
        )
    for i, values in additions.items():
        rows[i].setdefault("junction_pads_xy_m", []).extend(values)
    return pads, ledger

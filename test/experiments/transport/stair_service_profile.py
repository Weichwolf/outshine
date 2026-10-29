"""Explicit single-lane service recipes, separate from source traffic/access facts."""

import numpy as np

ROLE = "oneway_service_carriageway"
SHARED_ROLE = "shared_service_carriageway"
SERVICE_ROLES = (ROLE, SHARED_ROLE)


def direction_allowed(props, shared):
    if shared and any(key.startswith("oneway:") for key in props):
        return False
    if shared and "oneway" not in props:
        return True
    value = props.get("oneway")
    return type(value) is int and (value == 0 if shared else value in (-1, 1))


def service_profile(source, profile, *, shared=False):
    props = source["properties"]
    direction = props.get("oneway")
    if (
        props.get("class") != "service"
        or props.get("subclass")
        or not direction_allowed(props, shared)
        or props.get("brunnel")
        or props.get("indoor")
        or props.get("layer", 0) != 0
        or props.get("level", 0) != 0
    ):
        raise ValueError(
            "ordinary service source matching the declared lane direction required"
        )
    if profile.get("lanes") != 1 or any(
        not np.isfinite(profile[k]) or profile[k] <= 0
        for k in ("widthM", "clearanceM", "speedMps", "maxGradient")
    ):
        raise ValueError("positive finite single-lane service recipe required")
    result = {
        "source_recipe": SHARED_ROLE if shared else ROLE,
        "profile_kind": "shared_service" if shared else "service",
        "crossfall_limit": 0.03,
        "traffic_profile": {
            "width_m": profile["widthM"],
            "lane_width_m": profile["widthM"],
            "forward_lanes": 1 if shared else int(direction == 1),
            "reverse_lanes": 1 if shared else int(direction == -1),
            "clearance_m": profile["clearanceM"],
            "speed_mps": profile["speedMps"],
            "source_oneway": direction,
            "public_routing_admitted": False,
            "origin": "Single-lane width, clearance, speed and crossfall are procedural assumptions; supplied service class, direction and original access tags remain distinct. Geometry grants no public route.",
        },
    }
    if shared:
        result["traffic_profile"].update(
            physical_lanes=1,
            lane_use="alternating_bidirectional",
            opposing_traffic_policy="exclusive_lane_occupancy",
            direction_origin="procedural_default"
            if direction is None
            else "supplied_two_way",
            origin="One physical lane is shared by two centered directions under exclusive occupancy. Width, clearance, speed and any unsupplied direction are procedural assumptions. Original service/access tags remain distinct; no public route or simultaneous opposing passage is admitted.",
        )
    return result


def service_landing(row, props):
    shared = row.get("source_recipe") == SHARED_ROLE
    return (
        row.get("source_recipe") in SERVICE_ROLES
        and row.get("profile_kind") == ("shared_service" if shared else "service")
        and props.get("class") == "service"
        and not props.get("subclass")
        and direction_allowed(props, shared)
        and row.get("traffic_profile", {}).get("source_oneway") == props.get("oneway")
    )

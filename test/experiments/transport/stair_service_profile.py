"""Explicit one-way service geometry; public routing is a separate source contract."""

import numpy as np

ROLE = "oneway_service_carriageway"


def service_profile(source, profile):
    props = source["properties"]
    direction = props.get("oneway")
    if (
        props.get("class") != "service"
        or props.get("subclass")
        or type(direction) is not int
        or direction not in (-1, 1)
        or props.get("brunnel")
        or props.get("indoor")
        or props.get("layer", 0) != 0
        or props.get("level", 0) != 0
    ):
        raise ValueError(
            "ordinary service source with supplied one-way direction required"
        )
    if profile.get("lanes") != 1 or any(
        not np.isfinite(profile[k]) or profile[k] <= 0
        for k in ("widthM", "clearanceM", "speedMps", "maxGradient")
    ):
        raise ValueError("positive finite single-lane service recipe required")
    return {
        "source_recipe": ROLE,
        "profile_kind": "service",
        "crossfall_limit": 0.03,
        "traffic_profile": {
            "width_m": profile["widthM"],
            "lane_width_m": profile["widthM"],
            "forward_lanes": int(direction == 1),
            "reverse_lanes": int(direction == -1),
            "clearance_m": profile["clearanceM"],
            "speed_mps": profile["speedMps"],
            "source_oneway": direction,
            "public_routing_admitted": False,
            "origin": "Single-lane width, clearance, speed and crossfall are procedural assumptions; supplied service class, direction and original access tags remain distinct. Geometry grants no public route.",
        },
    }


def service_landing(row, props):
    return (
        row.get("source_recipe") == ROLE
        and row.get("profile_kind") == "service"
        and props.get("class") == "service"
        and not props.get("subclass")
        and type(props.get("oneway")) is int
        and props["oneway"] in (-1, 1)
        and row.get("traffic_profile", {}).get("source_oneway") == props["oneway"]
    )

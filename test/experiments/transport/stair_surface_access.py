"""Explicit procedural pedestrian contacts; supplied source travel stays separate."""

import numpy as np
from transport_travel import source_travel


def contact_access(chosen, new, old, old_row):
    if chosen.get("modes") != ["walk"] or chosen.get("access_assumption") != (
        "procedural_walk_default"
    ):
        raise ValueError("explicit pedestrian access assumption required")
    for source in (new, old):
        p = source["properties"]
        if (
            any(k.endswith(":conditional") for k in p)
            or p.get("brunnel")
            or p.get("indoor")
            or p.get("layer", 0) != 0
            or p.get("level", 0) != 0
        ):
            raise ValueError("ordinary exterior ground contact sources required")
    np_, op = new["properties"], old["properties"]
    if np_.get("class") != "path" or np_.get("subclass") != "footway":
        raise ValueError("new pedestrian contact requires an original footway")
    kind = chosen.get("kind")
    if kind == "footway_overlap":
        valid = op.get("class") == "path" and op.get("subclass") == "footway"
    elif kind == "minor_pedestrian_crossing":
        valid = (
            op.get("class") == "minor"
            and old_row.get("source_recipe") == "minor_ground_carriageway"
            and old_row.get("profile_kind") == "unclassified"
            and old_row.get("traffic_profile", {}).get("width_m")
            == old_row.get("width_m")
        )
    else:
        valid = False
    if not valid:
        raise ValueError("contact kind differs from actual old source/profile")
    recipes = {"footway": ["walk"], "residential": ["motor", "cycle", "walk"]}
    public, restricted, notes = source_travel([new, old], recipes)
    if not np.all(public & 4) or np.any(restricted & 4):
        raise ValueError("contact needs public walk in both source directions")
    return {
        "kind": kind,
        "modes": ["walk"],
        "access_assumption": "procedural_walk_default",
        "source_public_mode_bits": public.tolist(),
        "source_restricted_mode_bits": restricted.tolist(),
        "source_notes": notes,
        "origin": "constructed_surface_contact",
        "original_node_connection_claimed": False,
        "motor_cycle_contact_arcs": 0,
        "walk_contact_directions": ["old_to_new", "new_to_old"],
        "source_along_directions_changed": False,
    }

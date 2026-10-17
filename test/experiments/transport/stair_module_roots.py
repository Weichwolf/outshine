"""Bind a module root to an actual original cached outer or interior profile."""

import numpy as np
from stair_cycle_contacts import profile
from stair_module_loops import interior_profile


def root_profile(state, recipe, sources):
    port = recipe["port"]
    key = port["source_id"]
    if "vertex" in port:
        if (
            set(port) != {"source_id", "vertex"}
            or recipe.get("contact_mode") != "profile"
        ):
            raise ValueError("explicit varying-profile interior root required")
        index = port["vertex"]
        contact = interior_profile(state, sources, key, index)
        line = contact["profile"]
    else:
        if set(port) != {"source_id", "end"} or port["end"] not in (0, -1):
            raise ValueError("explicit original outer endpoint required")
        index = port["end"]
        choices = [
            p
            for p in state["report"]["after"]["proposed_outer_profiles"]
            if p["source_id"] == key
        ]
        if len(choices) != 1:
            raise ValueError("one unambiguous cached outer endpoint required")
        line = np.array(choices[0]["proposed_vertices_enu_m"])
    row = {
        "xy": np.array(sources[key]["xy_m"]),
        "source_id": key,
        "width_m": float(np.linalg.norm(line[-1, :2] - line[0, :2])),
        "port_vertex": index,
        "root_interior": "vertex" in port,
    }
    if line.shape != (3, 3):
        raise ValueError("actual cached root requires a complete three-point profile")
    if "vertex" not in port and not np.allclose(
        profile(row, index), line[:, :2], rtol=0, atol=1e-8
    ):
        raise ValueError("cached whole outer profile is shifted or not locally flat")
    if recipe.get("contact_mode") != "profile" and np.ptp(line[:, 2]) > 1e-7:
        raise ValueError("cached whole outer profile is shifted or not locally flat")
    return row, line

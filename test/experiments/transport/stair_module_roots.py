"""Bind a module root to an actual original cached outer or interior profile."""

import numpy as np
from outshine_ref.transport.junction_pads import profile_junction_pad
from stair_cycle_contacts import profile
from stair_landing_pads import node_profiles
from stair_module_loops import actual_profile, interior_profile


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
        if state.get("resident_profile_roots"):
            vertex = 0 if index == 0 else len(sources[key]["mvt_xy"]) - 1
            line = actual_profile(state, sources, key, vertex, interior=False)[
                "profile"
            ]
        else:
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


def bind_root_pad(rows, node, line, *, include_interiors=False, cached_profiles=()):
    selected = node_profiles(rows, node, include_interiors=include_interiors)
    if not any(vertex in (0, -1) for _i, vertex, _p in selected):
        raise ValueError("new module has no original endpoint at its frozen port")
    pad = profile_junction_pad(
        [line[:, :2], *cached_profiles, *[p for _i, _v, p in selected]]
    )
    coordinates = np.array(pad.exterior.coords[:-1])
    for i, vertex, _p in selected:
        rows[i]["junction_pads_xy_m"] = [coordinates.copy()]
        if include_interiors and 0 < vertex < len(rows[i]["xy"]) - 1:
            indices = rows[i].setdefault("junction_pad_vertices", [])
            if vertex not in indices:
                indices.append(vertex)

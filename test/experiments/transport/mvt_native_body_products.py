"""All consumers use the same reduced reference field and owned rendered caps."""

import copy

import numpy as np
from mvt_t_profile_contacts import audit_fixed_contacts


def consumed_plan(plan, data):
    replaced = {"xy", "parts", "part_global_faces", "primary_source", "stair_lofts"}
    result = copy.deepcopy(
        {key: value for key, value in plan.items() if key not in replaced}
    )
    result["stair_lofts"] = [
        copy.deepcopy(
            {key: value for key, value in flight.items() if key != "stations_m"}
        )
        for flight in plan["stair_lofts"]
    ]
    result["xy"] = data["reference_vertices"][:, :2]
    result["parts"] = []
    result["part_global_faces"] = []
    primary = np.full(len(result["xy"]), len(plan["rows"]), int)
    for owner in range(len(plan["rows"])):
        selected = np.flatnonzero((data["face_sources"] & (1 << owner)) != 0)
        faces = data["reference_faces"][selected]
        result["parts"].append(faces)
        result["part_global_faces"].append(selected)
        vertices = np.unique(faces)
        primary[vertices] = np.minimum(primary[vertices], owner)
    if np.any(primary == len(plan["rows"])):
        raise ValueError("consumed field must not retain unused reference vertices")
    result["primary_source"] = primary.tolist()
    for flight in result["stair_lofts"]:
        flight["stations_m"] = (
            (result["xy"] - flight["origin_xy_m"]) @ flight["tangent"]
        ).tolist()
    result["solver_geometry"] = {
        "vertices": len(plan["xy"]),
        "source_faces": sum(map(len, plan["parts"])),
    }
    return result, data["reference_vertices"][:, 2]


def audit_rendered_contacts(plan, data, contacts):
    rendered = dict(plan)
    rendered["xy"] = data["body_vertices"][:, :2]
    rendered["parts"] = [
        data["top_faces"][(data["face_sources"] & (1 << owner)) != 0]
        for owner in range(len(plan["rows"]))
    ]
    return audit_fixed_contacts(rendered, data["body_vertices"][:, 2], contacts)

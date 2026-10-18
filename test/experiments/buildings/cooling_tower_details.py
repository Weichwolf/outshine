"""Optional radial-recipe experiment; ordinary/provider classification paths stay intact."""

import numpy as np
from cooling_tower_mesh import components, draw_parts
from cooling_tower_plan import plan
from facade_parts import append


def refine(row, source, info, error_m):
    if info["source_kind"] != "cooling_tower":
        return None
    try:
        programme = plan(source)
    except ValueError as error:
        info["cooling_intake_declined"] = str(error)
        return None
    physical = components(programme, error_m=error_m)
    chunks = [
        dict(mesh, facade_extent=np.zeros((len(mesh["vertices"]), 3)))
        for mesh in draw_parts(programme, physical).values()
    ]
    mesh = append(chunks[0], chunks[1:])
    return mesh, info | {
        "cooling_intake": {
            "origin": "procedural construction, not delivered structural engineering",
            "source_id": row["id"],
            "bays": programme.bays,
            "supports": len(programme.supports),
            "intake_height_m": programme.shell.knots[0][0] - source.knots[0][0],
            "physical_components": len(physical),
            "normal_product": "interpolated source-profile slopes, unchanged mesh positions",
            "limits": "No structural load, full controller/interior, native cache/frame or general shadow/LOD admission",
        },
        "profile_knots": source.knots,
        "logical_profile": "complete camera-independent cooling programme",
        "triangles": len(mesh["faces"]),
    }

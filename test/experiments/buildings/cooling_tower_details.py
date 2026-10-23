"""Optional radial-recipe experiment; ordinary/provider classification paths stay intact."""

import numpy as np
from cooling_service_mesh import components as service_components
from cooling_service_mesh import world_points
from cooling_service_plan import plan as service_plan
from cooling_tower_mesh import components, draw_parts
from cooling_tower_plan import plan
from facade_parts import append


def refine(row, source, info, error_m, *, service_access=False, service_bay=0):
    if info["source_kind"] != "cooling_tower":
        return None
    try:
        programme = plan(source)
    except ValueError as error:
        info["cooling_intake_declined"] = str(error)
        return None
    physical = components(programme, error_m=error_m)
    access_info = {}
    if service_access:
        try:
            access = service_plan(programme, bay=service_bay)
        except ValueError as error:
            access_info["service_access_declined"] = str(error)
        else:
            physical.update(service_components(access))
            access_info["service_access"] = {
                "source_id": row["id"],
                "origin": "procedural lower service construction, not a measured blueprint",
                "bay": access.bay,
                "floor_m": access.floor_m,
                "ground_m": access.ground_m,
                "components": len(access.parts),
                "material_binding": access.material,
                "foot_route_xyz_m": world_points(access, access.route).tolist(),
                "limits": "Geometric climb/step-off clearance only; no controller, complete tower access, structural load or native/device admission",
            }
    chunks = [
        dict(mesh, facade_extent=np.zeros((len(mesh["vertices"]), 3)))
        for mesh in draw_parts(programme, physical).values()
    ]
    mesh = append(chunks[0], chunks[1:])
    return mesh, info | access_info | {
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

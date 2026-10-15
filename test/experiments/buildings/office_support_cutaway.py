"""A declared ground-office section, retaining exact mesh below the horizontal cut."""

import numpy as np
from building_surface_roles import apply as surface_roles
from furniture_mesh import build as furnishings
from structural_shell_geometry import native
from tile_room_details import corner_buffers


def cutaway(plan, product, body, storey=0, wall_height_m=1.10):
    current = plan["storey_plans"][storey]
    cut_z = current["floor_z_m"] + wall_height_m
    vertices, faces, materials = [], [], []
    offset = 0
    selected = (body["levels"] == storey) & (body["materials"] != 2)
    for part in np.unique(body["parts"][selected]):
        rows = np.flatnonzero(selected & (body["parts"] == part))
        solid = native(body, rows=rows)
        clipped = solid.trim_by_plane([0.0, 0.0, -1.0], -cut_z).to_mesh64()
        points = np.asarray(clipped.vert_properties)[:, :3]
        triangles = np.asarray(clipped.tri_verts)
        vertices.append(points)
        faces.append(triangles + offset)
        materials.append(
            np.full(len(triangles), body["materials"][rows[0]], dtype=np.uint16)
        )
        offset += len(points)
    furniture, _ = furnishings(
        product | {"rooms": tuple(r for r in product["rooms"] if r["storey"] == storey)}
    )
    vertices.append(furniture["vertices"])
    faces.append(furniture["faces"] + offset)
    materials.append(furniture["materials"])
    section, roles = surface_roles(
        plan,
        {
            "vertices": np.concatenate(vertices),
            "faces": np.concatenate(faces),
            "materials": np.concatenate(materials),
            "levels": np.full(sum(len(rows) for rows in faces), storey, dtype=np.uint8),
        },
    )
    mesh = corner_buffers(section, 1)
    return mesh, {
        "storey": storey,
        "cut_z_m": cut_z,
        "wall_height_m": wall_height_m,
        "surface_roles": roles,
        "scope": "Ground-office body parts clipped and capped at the stated horizontal plane; upper storeys and ceiling omitted, actual furnishings retained. Full uncut mesh is checked separately.",
    }

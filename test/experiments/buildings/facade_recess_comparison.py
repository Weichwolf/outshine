"""Independent closed-solid equality and directly retained output storage."""

import numpy as np
from structural_shell_geometry import native


def solid(geometry, identifier):
    ids = [
        i
        for i, name in enumerate(geometry.owners)
        if name.startswith(identifier + "/") and "/frame/" not in name
    ]
    bodies = [
        native(
            {
                "vertices": geometry.mesh["vertices"],
                "faces": geometry.mesh["faces"][geometry.mesh["parts"] == i],
            }
        )
        for i in ids
    ]
    result = bodies[0]
    for body in bodies[1:]:
        result = result + body
    return result


def equivalent(parts, union):
    errors = []
    for cell in parts.descriptors:
        a, b = solid(parts, cell.identifier), solid(union, cell.identifier)
        error = max(
            abs((a - b).volume()),
            abs((b - a).volume()),
            abs(a.volume() - b.volume()),
            abs(a.surface_area() - b.surface_area()),
        )
        if error > 1e-7:
            raise ValueError("Boolean control changed the closed bay union")
        errors.append(error)
        for name in parts.owners:
            if name.startswith(cell.door.identifier + "/frame/"):
                p, q = parts.owners.index(name), union.owners.index(name)
                first = parts.mesh["vertices"][
                    parts.mesh["faces"][parts.mesh["parts"] == p]
                ]
                second = union.mesh["vertices"][
                    union.mesh["faces"][union.mesh["parts"] == q]
                ]
                if not np.array_equal(first, second):
                    raise ValueError("Boolean control changed an open door frame")
    return {"cells": len(errors), "maximum_solid_volume_or_area_error": max(errors)}


def storage(mesh):
    buffers = {(a.ctypes.data, a.nbytes) for a in mesh.values()}
    return {
        "numpy_bytes": sum(size for _, size in buffers),
        "numpy_buffers": len(buffers),
        "vertices": len(mesh["vertices"]),
        "triangles": len(mesh["faces"]),
    }

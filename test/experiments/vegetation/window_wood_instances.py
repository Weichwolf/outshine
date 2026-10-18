"""Four existing wood prototypes with bounded point-instance transforms."""

import numpy as np
from blender_instances import instance_group, validated_instances
from fern_material import attach_normals
from window_portrait_blender import bind_mesh
from window_portrait_source import wood_orders, wood_templates


def groups(graph, axes):
    orders = wood_orders(axes)
    templates = wood_templates(graph)
    result = []
    for order, mesh in enumerate(templates):
        ids = np.flatnonzero(orders == order)
        if not len(ids):
            continue
        origins, frames, scales = validated_instances(
            axes["origins"][ids],
            axes["rotations"][ids],
            axes["uniform_scales"][ids],
        )
        result.append((order, mesh, ids, origins, frames, scales))
    triangles = sum(len(m["faces"]) * len(ids) for _, m, ids, *_ in result)
    if not 0 < triangles <= 3000000:
        raise ValueError("unchanged full wood triangle budget required")
    return result


def bind(graph, axes, surface):
    triangles, template_bytes, writes, records = 0, 0, 0, []
    for order, mesh, ids, origins, frames, scales in groups(graph, axes):
        obj = bind_mesh(f"wood prototype order {order}", mesh, surface)
        attach_normals(obj, mesh["normals"])
        holder = instance_group(
            obj, origins, frames, scales, name=f"all cached wood order {order}"
        )
        holder["source_axis_count"] = len(ids)
        triangles += len(mesh["faces"]) * len(ids)
        size = sum(a.nbytes for a in mesh.values())
        template_bytes += size
        # Mesh positions, indices/loop ranges and corner normals; point TRS.
        writes += len(mesh["vertices"]) * 12 + len(mesh["faces"]) * 56 + len(ids) * 36
        records.append(
            {
                "order": order,
                "axes": len(ids),
                "prototype_triangles": len(mesh["faces"]),
            }
        )
    return {
        "triangles": triangles,
        "output_array_bytes": template_bytes + len(axes["origins"]) * 36,
        "wood_instance_groups": records,
        "wood_prototype_array_bytes": template_bytes,
        "wood_explicit_blender_write_bytes": writes,
    }

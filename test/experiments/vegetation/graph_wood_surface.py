"""Joined graph wood with local near collars and complete direct coarse products."""

import time

from collar_shading import blend_collar_normals, tube_reference
from collar_union import local_union
from graph_collars import graph_collars
from graph_wood_axes import wood_axes
from graph_wood_material import wood_material


def joined_wood(
    graph,
    detail="near",
    sampler="blocked",
    material_method="indexed",
    max_axes=128,
    max_triangles=1_000_000,
):
    began = time.perf_counter()
    sources = wood_axes(graph, detail, max_axes, max_triangles)
    if sources is None:
        return None
    axes, originals = sources
    if not axes:
        raise ValueError("nonempty complete wood request required")
    source_s = time.perf_counter() - began
    stages = {"source_s": source_s}
    plans = []
    collars = []
    if detail == "near":
        began = time.perf_counter()
        collars, plans = graph_collars(axes, cells=48, method=sampler)
        stages["collars_s"] = time.perf_counter() - began
        began = time.perf_counter()
        references = {
            a.path: tube_reference(m, method="embree") for a, m in zip(axes, originals)
        }
        lookup = {a.path: a for a in axes}
        for index, plan in enumerate(plans):
            child = lookup[plan["path"]]
            parent = lookup[child.path[:-1]]
            _, r, _ = parent.chart(child.pose.anchor[None])
            collars[index], work = blend_collar_normals(
                collars[index],
                [references[parent.path], references[child.path]],
                float(r[0]),
                float(child.radius(0)),
            )
            plan.update(work)
        stages["normal_blend_s"] = time.perf_counter() - began
    began = time.perf_counter()
    mesh, cleanup = local_union(originals + collars, with_work=True)
    stages["union_s"] = time.perf_counter() - began
    began = time.perf_counter()
    attributes, work = wood_material(axes, mesh["vertices"], material_method)
    stages["bark_s"] = time.perf_counter() - began
    mesh |= attributes
    return (
        mesh,
        {
            "stages": stages,
            "axes": len(axes),
            "collars": len(collars),
            "plans": plans,
            "cleanup": cleanup,
            "material": work,
            "source_triangles": sum(len(m["faces"]) for m in originals),
            "collar_triangles": sum(len(m["faces"]) for m in collars),
            "triangles": len(mesh["faces"]),
            "product_bytes": sum(a.nbytes for a in mesh.values()),
        },
        axes,
        originals,
    )

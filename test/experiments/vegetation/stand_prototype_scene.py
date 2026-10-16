"""Direct, nested and once-packed forms of the same finished plant prototype."""

import time

import bpy
import numpy as np
from blender_instances import instance_group, join_instances
from fern_material import palette, source_normal
from mathutils import Matrix
from render_fern import model_arrays
from stand_leaf_product import ready
from tree_entry import load


def world_matrix(origin, frame, scale):
    matrix = np.eye(4)
    matrix[:3, :3] = frame * scale
    matrix[:3, 3] = origin
    return Matrix(matrix.tolist())


def completed_template(metadata, prototype, leaves, rows, start, decoded):
    return (
        metadata
        | {
            "prototype_profile_s": {
                "finished_decode": decoded - start,
                "blender_template": time.perf_counter() - decoded,
            }
        },
        prototype,
        leaves,
        rows,
    )


def build_template(inputs, mode):
    start = time.perf_counter()
    metadata, data = ready(inputs) if mode == "packed" else load(inputs)
    decoded = time.perf_counter()
    mats = palette(inputs)
    bump = next(n for n in mats[0].node_tree.nodes if n.type == "BUMP")
    mats[0].node_tree.links.new(source_normal(mats[0]), bump.inputs["Normal"])
    wood = model_arrays("prototype wood", data["wood.npz"], mats)
    if mode == "packed":
        leaf = model_arrays("packed leaves", data["leaf-mesh.npz"], mats)
        return completed_template(
            metadata,
            join_instances([wood, leaf], name="packed plant"),
            None,
            None,
            start,
            decoded,
        )
    rows = data[metadata["tables"]]
    leaves = [
        model_arrays("leaf " + str(i), data[f"blade-{i}.npz"], mats) for i in range(8)
    ]
    if mode == "direct":
        wood.hide_render = True
        for leaf in leaves:
            leaf.hide_render = True
        return completed_template(metadata, wood, leaves, rows, start, decoded)
    if mode != "nested":
        raise ValueError("declared exact prototype representation required")
    components = [wood]
    for i, leaf in enumerate(leaves):
        mask = rows["leaf_variants"] == i
        if mask.any():
            components.append(
                instance_group(
                    leaf,
                    rows["leaf_origins"][mask],
                    rows["leaf_frames"][mask],
                    np.ones(int(mask.sum())),
                    name="prototype leaves " + str(i),
                )
            )
        else:
            leaf.hide_render = True
    return completed_template(
        metadata,
        join_instances(components, name="nested plant"),
        None,
        rows,
        start,
        decoded,
    )


def place_template(template, origins, frames, scales, mode):
    metadata, prototype, leaves, rows = template
    if scales.ndim != 1 or len(scales) != len(origins):
        raise ValueError("one positive uniform scale per world instance required")
    if mode != "direct":
        instance_group(prototype, origins, frames, scales, name="stand plants")
    else:
        for root, (origin, frame, scale) in enumerate(zip(origins, frames, scales)):
            wood = prototype.copy()
            wood.hide_render = False
            bpy.context.collection.objects.link(wood)
            wood.matrix_world = world_matrix(origin, frame, scale)
            for i, leaf in enumerate(leaves):
                mask = rows["leaf_variants"] == i
                if mask.any():
                    holder = instance_group(
                        leaf,
                        rows["leaf_origins"][mask],
                        rows["leaf_frames"][mask],
                        np.ones(int(mask.sum())),
                        name=f"root{root}-leaf{i}",
                    )
                    holder.matrix_world = world_matrix(origin, frame, scale)
    count = len(origins)
    point_rows = (
        count
        if mode == "packed"
        else count + len(rows["leaf_ids"])
        if mode == "nested"
        else count * len(rows["leaf_ids"])
    )
    return {
        "world_instances": count,
        "instanced_triangles": metadata["instanced_triangles"] * count,
        "uploaded_point_rows": point_rows,
        "explicit_point_attribute_bytes": point_rows * 36,
        "source_plan_identity": metadata["plan_identity"],
        "mode": mode,
        "prototype_profile_s": metadata["prototype_profile_s"],
        "meshes": len(bpy.data.meshes),
        "objects": len(bpy.data.objects),
    }

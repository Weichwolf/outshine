"""Bounded Blender point instances with standard batch rotation conversion."""

import bpy
import numpy as np
from scipy.spatial.transform import Rotation


def instance_group(prototype, origins, frames, scales, *, name):
    origins, frames, scales = map(np.asarray, (origins, frames, scales))
    count = len(origins)
    if scales.shape == (count,):
        scales = np.repeat(scales[:, None], 3, axis=1)
    if (
        not 1 <= count <= 100000
        or origins.shape != (count, 3)
        or frames.shape != (count, 3, 3)
        or scales.shape != (count, 3)
        or not all(np.isfinite(a).all() for a in (origins, frames, scales))
        or np.any(scales <= 0)
        or np.max(abs(np.einsum("nji,njk->nik", frames, frames) - np.eye(3))) > 2e-12
        or np.max(abs(np.linalg.det(frames) - 1)) > 2e-12
    ):
        raise ValueError("bounded finite rigid frames and positive scales required")
    points = bpy.data.meshes.new(name)
    points.vertices.add(count)
    points.vertices.foreach_set("co", origins.astype(np.float32).ravel())
    euler = Rotation.from_matrix(frames).as_euler("xyz")
    for key, values in (("rotation", euler), ("scale", scales)):
        attr = points.attributes.new(key, "FLOAT_VECTOR", "POINT")
        attr.data.foreach_set("vector", values.astype(np.float32).ravel())
    points.update()
    holder = bpy.data.objects.new(name, points)
    bpy.context.collection.objects.link(holder)
    group = bpy.data.node_groups.new(name, "GeometryNodeTree")
    group.interface.new_socket(
        name="Geometry", in_out="INPUT", socket_type="NodeSocketGeometry"
    )
    group.interface.new_socket(
        name="Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry"
    )
    nodes, links = group.nodes, group.links
    inputs, outputs = nodes.new("NodeGroupInput"), nodes.new("NodeGroupOutput")
    source = nodes.new("GeometryNodeObjectInfo")
    source.transform_space = "ORIGINAL"
    source.inputs["Object"].default_value = prototype
    source.inputs["As Instance"].default_value = True
    instances = nodes.new("GeometryNodeInstanceOnPoints")
    links.new(inputs.outputs["Geometry"], instances.inputs["Points"])
    links.new(source.outputs["Geometry"], instances.inputs["Instance"])
    for key, socket in (("rotation", "Rotation"), ("scale", "Scale")):
        attr = nodes.new("GeometryNodeInputNamedAttribute")
        attr.data_type = "FLOAT_VECTOR"
        attr.inputs["Name"].default_value = key
        links.new(attr.outputs["Attribute"], instances.inputs[socket])
    links.new(instances.outputs["Instances"], outputs.inputs["Geometry"])
    holder.modifiers.new(name, "NODES").node_group = group
    prototype.hide_render = True
    return holder


def join_instances(objects, *, name):
    if not 1 <= len(objects) <= 32:
        raise ValueError("bounded complete prototype component list required")
    mesh = bpy.data.meshes.new(name)
    holder = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(holder)
    group = bpy.data.node_groups.new(name, "GeometryNodeTree")
    group.interface.new_socket(
        name="Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry"
    )
    nodes, links = group.nodes, group.links
    joined, output = nodes.new("GeometryNodeJoinGeometry"), nodes.new("NodeGroupOutput")
    for obj in objects:
        source = nodes.new("GeometryNodeObjectInfo")
        source.transform_space = "ORIGINAL"
        source.inputs["Object"].default_value = obj
        source.inputs["As Instance"].default_value = True
        links.new(source.outputs["Geometry"], joined.inputs["Geometry"])
        obj.hide_render = True
    links.new(joined.outputs["Geometry"], output.inputs["Geometry"])
    holder.modifiers.new(name, "NODES").node_group = group
    return holder

"""Bake one diffuse sunlight bounce for the Vienna street lighting reference."""

from dataclasses import dataclass
from time import perf_counter

import bpy
import numpy as np
from materials import DIFFUSE_TRANSMISSION, METALLIC, SURFACES, surface_properties
from mathutils import Vector
from mathutils.bvhtree import BVHTree

RECEIVER_REACH_M = 38.0
REFLECTION_REACH_M = 100.0
SUN_OCCLUSION_REACH_M = 200.0
RECEIVER_SPACING_M = 2.0
SURFACE_BIAS_M = 0.03
ATTRIBUTE = "Reflected sunlight"
FACADES = {"wall", "street base", "foundation", "street trim", "portal stone"}
RECEIVERS = FACADES | {
    "paving",
    "sidewalk",
    "road",
    "ground",
    "grass",
    "window sill",
    "frame",
}


@dataclass(frozen=True)
class ReceiverBatch:
    mesh: object
    loops: np.ndarray
    positions: np.ndarray
    normals: np.ndarray


def mesh_points(mesh, transform):
    points = np.empty(3 * len(mesh.vertices), dtype=np.float32)
    mesh.vertices.foreach_get("co", points)
    return points.reshape(-1, 3) @ transform[:3, :3].T + transform[:3, 3]


def street_receivers(scene):
    bpy.context.view_layer.update()
    camera = scene.camera.matrix_world
    eye = np.asarray(camera.translation)
    right, up, backward = np.asarray(camera.to_3x3()).T
    film = np.max(np.abs(scene.camera.data.view_frame(scene=scene)), axis=0)
    tan_x, tan_y = film[:2] / film[2]
    batches = []
    has_facade = False
    for obj in scene.objects:
        if obj.type != "MESH" or len(obj.data.materials) != 1:
            continue
        mesh = obj.data
        material = mesh.materials[0].name.partition(" #")[0]
        if material not in RECEIVERS:
            continue
        mesh.calc_loop_triangles()
        index = np.empty(3 * len(mesh.loop_triangles), dtype=np.int32)
        mesh.loop_triangles.foreach_get("vertices", index)
        corners = mesh_points(mesh, np.asarray(obj.matrix_world))[index.reshape(-1, 3)]
        centres = corners.mean(axis=1)
        vectors = corners - centres[:, None, :]
        normal = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
        length = np.linalg.norm(normal, axis=1)
        relative = centres - eye
        depth = relative @ -backward
        margin = 1.0 + np.linalg.norm(vectors, axis=2).max(axis=1)
        keep = (
            (length > 1e-7)
            & (np.linalg.norm(relative, axis=1) < RECEIVER_REACH_M + margin)
            & (depth + margin > 0)
            & (abs(relative @ right) < 1.18 * tan_x * depth + margin)
            & (abs(relative @ up) < 1.18 * tan_y * depth + margin)
            & (np.sum(normal * relative, axis=1) < 0)
        )
        if not keep.any():
            continue
        has_facade |= material in FACADES
        loops = np.empty(3 * len(mesh.loop_triangles), dtype=np.int32)
        mesh.loop_triangles.foreach_get("loops", loops)
        positions = (centres[keep, None, :] + 0.96 * vectors[keep]).reshape(-1, 3)
        batches.append(
            ReceiverBatch(
                mesh,
                loops.reshape(-1, 3)[keep].ravel(),
                positions,
                np.repeat(normal[keep] / length[keep, None], 3, axis=0),
            )
        )
    return batches if has_facade else []


def place_street_sky(scene):
    probe = bpy.data.lightprobes.new("Street sky visibility", "VOLUME")
    probe.resolution_x = probe.resolution_y = 19
    probe.resolution_z = 9
    probe.surfel_density = 2
    probe.bake_samples = 128
    probe.capture_world = True
    probe.capture_indirect = probe.capture_emission = False
    probe.capture_distance = 64.0
    probe.normal_bias = 0.12
    probe.view_bias = 0.1
    volume = bpy.data.objects.new(probe.name, probe)
    volume.location = scene.camera.matrix_world.translation + Vector((0, 0, 8))
    volume.scale = (36, 36, 12)
    scene.collection.objects.link(volume)


def albedo(material):
    if material is None:
        return (0, 0, 0)
    name = material.name
    base = name.partition(" #")[0]
    if base not in SURFACES or base in {"glass", "water", "regional water"}:
        return (0, 0, 0)
    _, colour, _ = surface_properties(name)
    return (
        np.asarray(colour)
        * (1 - METALLIC.get(base, 0))
        * (1 - DIFFUSE_TRANSMISSION.get(base, 0))
    )


def reflection_index(graph, eye):
    lower = eye - (RECEIVER_REACH_M + REFLECTION_REACH_M)
    upper = eye + (RECEIVER_REACH_M + REFLECTION_REACH_M)
    vertices, faces, reflectance = [], [], []
    offset = 0
    for instance in graph.object_instances:
        obj = instance.object
        if obj.type != "MESH":
            continue
        transform = np.asarray(instance.matrix_world)
        box = np.asarray(obj.bound_box) @ transform[:3, :3].T + transform[:3, 3]
        if np.any(box.max(axis=0) < lower) or np.any(box.min(axis=0) > upper):
            continue
        mesh = obj.data
        mesh.calc_loop_triangles()
        indices = np.empty(3 * len(mesh.loop_triangles), dtype=np.int32)
        mesh.loop_triangles.foreach_get("vertices", indices)
        points = mesh_points(mesh, transform)
        triangles = indices.reshape(-1, 3)
        xyz = points[triangles]
        keep = np.all((xyz.min(axis=1) <= upper) & (xyz.max(axis=1) >= lower), axis=1)
        if not keep.any():
            continue
        ids = np.empty(len(triangles), dtype=np.int32)
        mesh.loop_triangles.foreach_get("material_index", ids)
        palette = np.array([albedo(m) for m in mesh.materials] or [(0, 0, 0)])
        used, packed = np.unique(triangles[keep], return_inverse=True)
        vertices.append(points[used])
        faces.append(packed.reshape(-1, 3) + offset)
        reflectance.append(palette[ids[keep]])
        offset += len(used)
    tree = BVHTree.FromPolygons(
        np.concatenate(vertices).tolist(),
        np.concatenate(faces).tolist(),
        all_triangles=True,
    )
    return tree, np.concatenate(reflectance)


def hemisphere_directions(count):
    step = np.arange(count)
    radial_square = (step + 0.5) / count
    phase = np.remainder(step * ((np.sqrt(5) - 1) / 2), 1) * (2 * np.pi)
    radius = np.sqrt(radial_square)
    return np.column_stack(
        (radius * np.cos(phase), radius * np.sin(phase), np.sqrt(1 - radial_square))
    )


def reflected_rgb(scene, graph, index, point, normal, sunlight, directions):
    tree, reflectance = index
    normal = Vector(normal).normalized()
    origin = Vector(point) + normal * SURFACE_BIAS_M
    axis = Vector((0, 0, 1)) if abs(normal.z) < 0.95 else Vector((1, 0, 0))
    tangent = normal.cross(axis).normalized()
    bitangent = normal.cross(tangent)
    radiance = np.zeros(3)
    for x, y, z in directions:
        direction = x * tangent + y * bitangent + z * normal
        hit, surface, face, _ = tree.ray_cast(origin, direction, REFLECTION_REACH_M)
        if face is None:
            continue
        cosine = surface.dot(sunlight)
        if cosine <= 0 or not reflectance[face].any():
            continue
        blocked = scene.ray_cast(
            graph,
            hit + surface * SURFACE_BIAS_M,
            sunlight,
            distance=SUN_OCCLUSION_REACH_M,
        )[0]
        if not blocked:
            radiance += cosine * reflectance[face]
    return radiance


def apply_bounce_material(material):
    tree = material.node_tree
    bsdf = tree.nodes["Principled BSDF"]
    base = bsdf.inputs["Base Color"]
    field = tree.nodes.new("ShaderNodeVertexColor")
    field.layer_name = ATTRIBUTE
    modulate = tree.nodes.new("ShaderNodeVectorMath")
    modulate.operation = "MULTIPLY"
    tree.links.new(field.outputs["Color"], modulate.inputs[0])
    if base.is_linked:
        tree.links.new(base.links[0].from_socket, modulate.inputs[1])
    else:
        modulate.inputs[1].default_value = base.default_value[:3]
    tree.links.new(modulate.outputs[0], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 1


def bake_sun_bounce(scene, sun, batches):
    start = perf_counter()
    eye = np.asarray(scene.camera.matrix_world.translation)
    graph = bpy.context.evaluated_depsgraph_get()
    index = reflection_index(graph, eye)
    sunlight = -(sun.matrix_world.to_3x3() @ Vector((0, 0, -1))).normalized()
    directions = hemisphere_directions(32)
    positions = np.concatenate([batch.positions for batch in batches])
    normals = np.concatenate([batch.normals for batch in batches])
    keys = np.c_[np.floor(positions / RECEIVER_SPACING_M), np.round(normals * 12)]
    _, first, lookup = np.unique(keys, axis=0, return_index=True, return_inverse=True)
    strength = sun.data.energy / (np.pi * len(directions))
    values = strength * np.array(
        [
            reflected_rgb(
                scene, graph, index, positions[i], normals[i], sunlight, directions
            )
            for i in first
        ]
    )
    fade = np.clip(
        (RECEIVER_REACH_M - np.linalg.norm(positions - eye, axis=1)) / 8, 0, 1
    )
    irradiance = values[lookup] * fade[:, None]
    start_loop = 0
    materials = set()
    for batch in batches:
        stop = start_loop + len(batch.loops)
        pixels = np.zeros((len(batch.mesh.loops), 4), dtype=np.float32)
        pixels[:, 3] = 1
        np.maximum.at(pixels[:, :3], batch.loops, irradiance[start_loop:stop])
        attr = batch.mesh.color_attributes.new(
            name=ATTRIBUTE, type="FLOAT_COLOR", domain="CORNER"
        )
        attr.data.foreach_set("color", pixels.ravel())
        materials.add(batch.mesh.materials[0])
        start_loop = stop
    for material in materials:
        apply_bounce_material(material)
    bpy.context.view_layer.update()
    return len(values), perf_counter() - start

"""Bake one diffuse sunlight bounce for the Vienna street lighting reference."""

from dataclasses import dataclass
from time import perf_counter

import bpy
import numpy as np
from irradiance_mesh import (
    GROUND_RECEIVERS,
    mesh_points,
    mesh_triangles,
    refine_receivers,
)
from materials import DIFFUSE_TRANSMISSION, METALLIC, SURFACES, surface_properties
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from ray_instances import InstancedRays, RayInstance

RECEIVER_REACH_M = 38.0
REFLECTION_REACH_M = 100.0
SUN_OCCLUSION_REACH_M = 200.0
RECEIVER_SPACING_M = 1.0
SURFACE_BIAS_M = 0.03
ATTRIBUTE = "Reflected sunlight"
FACADES = {"wall", "street base", "foundation", "street trim", "portal stone"}
RECEIVERS = (
    FACADES
    | GROUND_RECEIVERS
    | {
        "window sill",
        "frame",
    }
)
DECK_RECEIVERS = {
    "bridge deck",
    "retaining wall",
    "unpaved path",
    "forest floor",
    "meadow",
}


@dataclass(frozen=True)
class ReceiverBatch:
    mesh: object
    loops: np.ndarray
    positions: np.ndarray
    normals: np.ndarray


def collect_receivers(scene, *, below_deck=False):
    bpy.context.view_layer.update()
    camera = scene.camera.matrix_world
    eye = np.asarray(camera.translation)
    right, up, backward = np.asarray(camera.to_3x3()).T
    film = np.max(np.abs(scene.camera.data.view_frame(scene=scene)), axis=0)
    tan_x, tan_y = film[:2] / film[2]
    batches = []
    has_facade = False
    materials = RECEIVERS | DECK_RECEIVERS if below_deck else RECEIVERS
    for obj in scene.objects:
        if obj.type != "MESH" or len(obj.data.materials) != 1:
            continue
        mesh = obj.data
        material = mesh.materials[0].name.partition(" #")[0]
        if material not in materials:
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
        # Face-local offsets can give opposite sides of a shared edge different irradiance.
        positions = corners[keep].reshape(-1, 3)
        batches.append(
            ReceiverBatch(
                mesh,
                loops.reshape(-1, 3)[keep].ravel(),
                positions,
                np.repeat(normal[keep] / length[keep, None], 3, axis=0),
            )
        )
    return batches if has_facade or below_deck else []


def street_receivers(scene, *, below_deck=False):
    batches = collect_receivers(scene, below_deck=below_deck)
    materials = GROUND_RECEIVERS | DECK_RECEIVERS if below_deck else GROUND_RECEIVERS
    if refine_receivers(
        scene, batches, RECEIVER_REACH_M, 2 * RECEIVER_SPACING_M, materials
    ):
        return collect_receivers(scene, below_deck=below_deck)
    return batches


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


def face_albedo(mesh):
    ids = np.empty(len(mesh.loop_triangles), dtype=np.int32)
    mesh.loop_triangles.foreach_get("material_index", ids)
    palette = np.array([albedo(m) for m in mesh.materials] or [(0, 0, 0)])
    return palette[ids]


def static_ray_mesh(objects, lower, upper):
    vertices, faces, reflectance = [], [], []
    offset = 0
    for mesh, transform in objects:
        points = mesh_points(mesh, transform)
        triangles = mesh_triangles(mesh)
        if not len(triangles):
            continue
        xyz = points[triangles]
        keep = np.all((xyz.min(axis=1) <= upper) & (xyz.max(axis=1) >= lower), axis=1)
        if not keep.any():
            continue
        used, packed = np.unique(triangles[keep], return_inverse=True)
        vertices.append(points[used])
        faces.append(packed.reshape(-1, 3) + offset)
        reflectance.append(face_albedo(mesh)[keep])
        offset += len(used)
    tree = BVHTree.FromPolygons(
        np.concatenate(vertices).tolist() if vertices else [],
        np.concatenate(faces).tolist() if faces else [],
        all_triangles=True,
    )
    colours = np.concatenate(reflectance) if reflectance else np.empty((0, 3))
    return tree, colours


def reflection_index(graph, eye, sunlight, *, reflected_sky=False):
    reach = RECEIVER_REACH_M + REFLECTION_REACH_M + 2 * SURFACE_BIAS_M
    sunward = np.asarray(sunlight) * SUN_OCCLUSION_REACH_M
    lower = eye - reach + np.minimum(sunward, 0)
    upper = eye + reach + np.maximum(sunward, 0)
    if reflected_sky:
        sky_reach = reach + REFLECTION_REACH_M
        lower = np.minimum(lower, eye - sky_reach)
        upper = np.maximum(upper, eye + sky_reach)
    objects, placements = [], []
    for instance in graph.object_instances:
        obj = instance.object
        if obj.type != "MESH" or not len(obj.data.polygons):
            continue
        transform = np.asarray(instance.matrix_world)
        box = np.asarray(obj.bound_box) @ transform[:3, :3].T + transform[:3, 3]
        low, high = box.min(axis=0) - 1e-5, box.max(axis=0) + 1e-5
        if np.any(high < lower) or np.any(low > upper):
            continue
        if instance.is_instance:
            bounds = (*low.tolist(), *high.tolist())
            placements.append((obj.data, instance.matrix_world.copy(), bounds))
        else:
            objects.append((obj.data, transform))
    static, colours = static_ray_mesh(objects, lower, upper)
    if not placements:
        return static, colours

    reflectance = [colours]
    face_offset = len(colours)
    prototypes, instances = {}, []
    for mesh, transform, bounds in placements:
        key = mesh.as_pointer()
        if key not in prototypes:
            triangles = mesh_triangles(mesh)
            native = BVHTree.FromPolygons(
                mesh_points(mesh).tolist(), triangles.tolist(), all_triangles=True
            )
            prototypes[key] = native, face_offset
            reflectance.append(face_albedo(mesh))
            face_offset += len(triangles)
        native, offset = prototypes[key]
        instances.append(RayInstance.make(native, offset, transform, bounds))
    return InstancedRays(static, instances), np.concatenate(reflectance)


def hemisphere_directions(count):
    step = np.arange(count)
    radial_square = (step + 0.5) / count
    phase = np.remainder(step * ((np.sqrt(5) - 1) / 2), 1) * (2 * np.pi)
    radius = np.sqrt(radial_square)
    return np.column_stack(
        (radius * np.cos(phase), radius * np.sin(phase), np.sqrt(1 - radial_square))
    )


def hemisphere_basis(normal):
    axis = Vector((0, 0, 1)) if abs(normal.z) < 0.95 else Vector((1, 0, 0))
    tangent = normal.cross(axis).normalized()
    return tangent, normal.cross(tangent), normal


def hemisphere_rays(point, normal, directions):
    normal = Vector(normal).normalized()
    origin = Vector(point) + normal * SURFACE_BIAS_M
    return origin, directions @ np.asarray(hemisphere_basis(normal))


def reflected_rgb(index, point, normal, sunlight, directions):
    tree, reflectance = index
    origin, world_directions = hemisphere_rays(point, normal, directions)
    radiance = np.zeros(3)
    for direction in world_directions:
        hit, surface, face, _ = tree.ray_cast(origin, direction, REFLECTION_REACH_M)
        if face is None:
            continue
        cosine = surface.dot(sunlight)
        if cosine <= 0 or not reflectance[face].any():
            continue
        obstruction = tree.ray_cast(
            hit + surface * SURFACE_BIAS_M, sunlight, SUN_OCCLUSION_REACH_M
        )[2]
        if obstruction is None:
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


@dataclass(frozen=True)
class ReceiverSamples:
    positions: np.ndarray
    normals: np.ndarray
    fade: np.ndarray
    active: np.ndarray
    first: np.ndarray
    lookup: np.ndarray

    def expand(self, values):
        irradiance = np.zeros_like(self.positions)
        irradiance[self.active] = values[self.lookup] * self.fade[self.active, None]
        return irradiance


def receiver_samples(batches, eye):
    positions = np.concatenate([batch.positions for batch in batches])
    normals = np.concatenate([batch.normals for batch in batches])
    fade = np.clip(
        (RECEIVER_REACH_M - np.linalg.norm(positions - eye, axis=1)) / 8, 0, 1
    )
    active = np.flatnonzero(fade)
    keys = np.c_[
        np.floor(positions[active] / RECEIVER_SPACING_M), np.round(normals[active] * 12)
    ]
    _, first, lookup = np.unique(keys, axis=0, return_index=True, return_inverse=True)
    return ReceiverSamples(positions, normals, fade, active, active[first], lookup)


def write_irradiance(batches, irradiance, name, *, fade=None):
    start_loop = 0
    materials = set()
    for batch in batches:
        stop = start_loop + len(batch.loops)
        pixels = np.zeros((len(batch.mesh.loops), 4), dtype=np.float32)
        np.maximum.at(pixels[:, :3], batch.loops, irradiance[start_loop:stop])
        if fade is None:
            pixels[:, 3] = 1
        else:
            np.maximum.at(pixels[:, 3], batch.loops, fade[start_loop:stop])
        attr = batch.mesh.color_attributes.new(
            name=name, type="FLOAT_COLOR", domain="CORNER"
        )
        attr.data.foreach_set("color", pixels.ravel())
        materials.add(batch.mesh.materials[0])
        start_loop = stop
    return materials


def bake_sun_bounce(scene, sun, batches, *, index=None):
    start = perf_counter()
    eye = np.asarray(scene.camera.matrix_world.translation)
    samples = receiver_samples(batches, eye)
    if not len(samples.first):
        return 0, perf_counter() - start
    sunlight = -(sun.matrix_world.to_3x3() @ Vector((0, 0, -1))).normalized()
    directions = hemisphere_directions(128)
    if index is None:
        index = reflection_index(bpy.context.evaluated_depsgraph_get(), eye, sunlight)
    strength = sun.data.energy / (np.pi * len(directions))
    values = strength * np.array(
        [
            reflected_rgb(
                index, samples.positions[i], samples.normals[i], sunlight, directions
            )
            for i in samples.first
        ]
    )
    for material in write_irradiance(batches, samples.expand(values), ATTRIBUTE):
        apply_bounce_material(material)
    bpy.context.view_layer.update()
    return len(values), perf_counter() - start

"""Render selected Vienna reference views in a single Blender GPU process."""

import argparse
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "lib/src")]
sys.path.extend(
    map(str, (ROOT / "build/blender-python/lib").glob("python*/site-packages"))
)

from blender_air import configure_aerial
from catalog import load_catalog
from materials import SURFACES
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import configure_daylight
from outshine_ref.rendering.blender_render import configure_gpu, render_png
from reference_scene import ReferenceScene
from woodland import tree_batches


def upload(name, vertices, faces, *, collection):
    mesh = bpy.data.meshes.new(name)
    if isinstance(vertices, np.ndarray) and isinstance(faces, np.ndarray):
        used, indices = np.unique(faces, return_inverse=True)
        mesh.vertices.add(len(used))
        mesh.vertices.foreach_set("co", np.asarray(vertices[used], np.float32).ravel())
        mesh.loops.add(faces.size)
        mesh.loops.foreach_set("vertex_index", np.asarray(indices, np.int32).ravel())
        mesh.polygons.add(len(faces))
        mesh.polygons.foreach_set(
            "loop_start", np.arange(0, faces.size, faces.shape[1], dtype=np.int32)
        )
    else:
        mesh.from_pydata(vertices, [], faces)
    mesh.update()
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    colour, roughness = SURFACES[name]
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def plant_woodland(world, seed, geometry):
    count = 0
    for species, group, meshes, placements in tree_batches(
        world.source, 250, seed, world.focal
    ):
        collection = bpy.data.collections.new(f"{species} group {group}")
        for name, (vertices, faces) in meshes.items():
            upload(name, vertices, faces, collection=collection)
        for x, y, z, angle, scale in placements:
            instance = bpy.data.objects.new(species, None)
            instance.instance_type = "COLLECTION"
            instance.instance_collection = collection
            instance.location = (x, y, z)
            instance.rotation_euler.z = angle
            instance.scale = (scale,) * 3
            geometry.objects.link(instance)
        count += len(placements)
    return count


def render(place, config, args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    device = configure_gpu(scene, "eevee")
    configure_daylight(scene)
    world = ReferenceScene(place, config)
    sunlight = Vector(world.sun)
    sun = next(o for o in scene.objects if o.type == "LIGHT" and o.data.type == "SUN")
    sun.rotation_euler = (-sunlight).to_track_quat("-Z", "Y").to_euler()
    sky = scene.world.node_tree.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "HOSEK_WILKIE"
    sky.sun_direction = sunlight
    sky.turbidity = 2.2
    background = scene.world.node_tree.nodes["Background"]
    background.inputs["Strength"].default_value = 0.25
    scene.world.node_tree.links.new(sky.outputs["Color"], background.inputs["Color"])
    scene.cycles.seed = config["seed"]
    geometry = bpy.data.collections.new("World geometry")
    scene.collection.children.link(geometry)
    for name, verts, faces in world.surfaces():
        upload(name, verts, faces, collection=geometry)
    eye, look = Vector(world.eye), Vector(world.look)
    scene.camera = configure_camera(
        {
            "eye": eye,
            "target": eye + look,
            "projection": "perspective",
            "vertical_fov_deg": world.fov,
            "clip_near_m": 0.05,
            "clip_far_m": 240000,
        }
    )
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    trees = plant_woodland(world, config["seed"], geometry)
    scene.eevee.taa_render_samples = 16
    configure_aerial(scene, geometry, eye.z + world.source.datum)
    if args.engine == "cycles":
        device = configure_gpu(scene, "cycles")
    image = args.output / (place["id"] + ".png")
    args.output.mkdir(parents=True, exist_ok=True)
    times = [render_png(scene, image) for _ in range(2 if args.repeat else 1)]
    print(
        json.dumps(
            {
                "place": place["id"],
                "surface": world.support,
                "woodland_trees": trees,
                "eye_m": eye.z + world.source.datum,
                **device,
                "seconds_to_png": times,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--place", action="append")
    p.add_argument("--output", type=Path, default=ROOT / "build/shots/places")
    p.add_argument("--engine", choices=["eevee", "cycles"], default="eevee")
    p.add_argument("--repeat", action="store_true")
    args = p.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    config = load_catalog()
    for place in config["places"]:
        if not args.place or place["id"] in args.place:
            render(place, config, args)

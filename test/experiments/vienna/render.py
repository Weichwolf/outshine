"""Render selected Vienna reference views in a single Blender GPU process."""

import argparse
import json
import math
import sys
from datetime import datetime
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(HERE), str(ROOT / "lib/src")]
sys.path.extend(
    map(str, (ROOT / "build/blender-python/lib").glob("python*/site-packages"))
)

from astral import Observer
from astral.sun import azimuth, elevation
from catalog import load_catalog
from horizon import terrain_horizon
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import configure_daylight
from outshine_ref.rendering.blender_render import configure_gpu, render_png
from scene_mesh import assemble
from source_scene import LocalScene
from woodland import plant_woodland

# Linear reflectance, perceptual roughness.
SURFACES = {
    "bridge deck": ((0.23, 0.22, 0.20), 0.82),
    "bridge metal": ((0.085, 0.13, 0.115), 0.46),
    "bark": ((0.12, 0.075, 0.045), 0.92),
    "foliage": ((0.095, 0.205, 0.044), 0.52),
    "water": ((0.028, 0.084, 0.095), 0.09),
    "distant terrain": ((0.11, 0.16, 0.10), 0.95),
    "ground": ((0.15, 0.13, 0.095), 0.9),
    "road": ((0.075, 0.08, 0.082), 0.82),
    "sidewalk": ((0.27, 0.25, 0.22), 0.77),
    "roof": ((0.20, 0.10, 0.06), 0.77),
    "wall": ((0.48, 0.40, 0.30), 0.82),
    "frame": ((0.21, 0.18, 0.14), 0.65),
    "glass": ((0.08, 0.12, 0.15), 0.11),
}


def upload(name, vertices, faces, *, collection=None):
    mesh = bpy.data.meshes.new(name)
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
    target = bpy.context.scene.collection if collection is None else collection
    target.objects.link(obj)
    return obj


def render(place, config, args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    device = configure_gpu(scene, "eevee")
    configure_daylight(scene)
    observer = Observer(place["latitude"], place["longitude"])
    utc = datetime.fromisoformat(place.get("utc", config["utc"]))
    fov = place.get("vertical_fov_deg", config["vertical_fov_deg"])
    bearing = math.radians(azimuth(observer, utc))
    altitude = math.radians(elevation(observer, utc))
    sunlight = Vector(
        (
            math.sin(bearing) * math.cos(altitude),
            math.cos(bearing) * math.cos(altitude),
            math.sin(altitude),
        )
    )
    sun = next(o for o in scene.objects if o.type == "LIGHT" and o.data.type == "SUN")
    sun.rotation_euler = (-sunlight).to_track_quat("-Z", "Y").to_euler()
    scene.cycles.seed = config["seed"]
    source = LocalScene(
        place, config["source_snapshot"], ROOT / "build/vienna-sources", 250
    )
    far, faces, receipts = terrain_horizon(source, ROOT / "build/vienna-sources", 250)
    pins = json.loads((HERE / "sources.json").read_text())
    for row in source.receipts + receipts:
        if pins.get(row["url"]) != row["sha256"]:
            raise ValueError(f"Unpinned or changed Vienna source: {row['url']}")
    objects = {n: upload(n, *mesh) for n, mesh in assemble(source, 250).items()}
    upload("distant terrain", far, faces)
    bpy.context.view_layer.update()
    if "absolute_eye_m" in place:
        surface = "absolute station"
        eye = Vector((0, 0, place["absolute_eye_m"] - source.datum))
    else:
        candidates = (
            ("sidewalk", "road")
            if place["support"] == "street"
            else (place["support"],)
        )
        hits = []
        for name in candidates:
            if name in objects:
                hit, pos, _, _ = objects[name].ray_cast(
                    Vector((0, 0, 300)), Vector((0, 0, -1))
                )
                if hit:
                    hits.append((pos.z, name))
        if not hits:
            raise ValueError(f"{place['id']}: no walkable {candidates} at camera")
        support, surface = max(hits)
        eye = Vector((0, 0, support + config["eye_height_m"]))
    yaw, pitch = map(math.radians, (place["heading_deg"], place["pitch_deg"]))
    look = Vector(
        (
            math.sin(yaw) * math.cos(pitch),
            math.cos(yaw) * math.cos(pitch),
            math.sin(pitch),
        )
    )
    scene.camera = configure_camera(
        {
            "eye": eye,
            "target": eye + look,
            "projection": "perspective",
            "vertical_fov_deg": fov,
            "clip_near_m": 0.05,
            "clip_far_m": 240000,
        }
    )
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    focal_pixels = 360 / math.tan(math.radians(fov) / 2)
    trees = plant_woodland(source, 250, config["seed"], focal_pixels, upload)
    scene.eevee.taa_render_samples = 16
    if args.engine == "cycles":
        device = configure_gpu(scene, "cycles")
    image = args.output / (place["id"] + ".png")
    args.output.mkdir(parents=True, exist_ok=True)
    times = [render_png(scene, image) for _ in range(2 if args.repeat else 1)]
    print(
        json.dumps(
            {
                "place": place["id"],
                "surface": surface,
                "woodland_trees": trees,
                "eye_m": eye.z + source.datum,
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

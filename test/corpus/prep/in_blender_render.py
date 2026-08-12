"""Runs inside Blender: the oracle render, from a job the preparer wrote."""

import json
import math
import struct
import sys
import time

import bpy
import numpy
from mathutils import Matrix, Vector

# glTF is +Y up and Blender is +Z up, so (x, y, z) -> (x, -z, y). Every declared vector and every
# declared frame goes through this one map, in one place, and the importer applies the same one.
YUP_TO_ZUP = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))


def fail(message):
    print("in_blender_render: " + message, file=sys.stderr)
    sys.exit(9)


def job_from_argv():
    if "--" not in sys.argv:
        fail("no job file after --")
    with open(sys.argv[sys.argv.index("--") + 1], "r") as f:
        return json.load(f)


def clear_objects():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)


def observed_world(scene):
    world = scene.world
    if world is None or world.node_tree is None:
        return {"present": world is not None, "hasNodeTree": False}
    for node in world.node_tree.nodes:
        if node.type == "BACKGROUND":
            colour = list(node.inputs["Color"].default_value)[:3]
            return {"present": True, "hasNodeTree": True, "colourLinear": colour,
                    "strength": node.inputs["Strength"].default_value}
    return {"present": True, "hasNodeTree": True, "note": "no Background node"}


def apply_world(scene, declared):
    if declared["kind"] == "factory":
        return
    world = scene.world
    for node in world.node_tree.nodes:
        if node.type == "BACKGROUND":
            node.inputs["Color"].default_value = tuple(declared["colourLinear"]) + (1.0,)
            node.inputs["Strength"].default_value = declared["strength"]


def import_gltf(paths):
    try:
        bpy.ops.preferences.addon_enable(module="io_scene_gltf2")
    except Exception:
        pass
    before = set(bpy.data.objects)
    for path in paths:
        result = bpy.ops.import_scene.gltf(filepath=path)
        if "FINISHED" not in result:
            fail("glTF import of " + path + " returned " + repr(result))
    return [obj for obj in bpy.data.objects if obj not in before]


def strip_crossings(imported, camera_source):
    """Light and material never cross the glTF boundary, so whatever crossed is deleted here."""
    removed = {"lights": 0, "cameras": 0}
    for obj in list(imported):
        if obj.type == "LIGHT":
            bpy.data.objects.remove(obj, do_unlink=True)
            removed["lights"] += 1
        elif obj.type == "CAMERA" and camera_source == "manifest":
            bpy.data.objects.remove(obj, do_unlink=True)
            removed["cameras"] += 1
    return removed


def build_camera(scene, declared):
    position = Vector(declared["positionM"])
    forward = (Vector(declared["lookAtM"]) - position)
    if forward.length == 0.0:
        fail("camera position and lookAt coincide")
    forward.normalize()
    world_up = Vector((0.0, 1.0, 0.0))
    if abs(forward.dot(world_up)) > 0.9999:
        fail("camera forward is parallel to +Y; the roll would be undefined")
    right = forward.cross(world_up).normalized()
    up = right.cross(forward)
    roll = declared["rollRad"]
    rolled_right = right * math.cos(roll) + up * math.sin(roll)
    rolled_up = up * math.cos(roll) - right * math.sin(roll)

    basis = Matrix((
        (rolled_right.x, rolled_up.x, -forward.x, position.x),
        (rolled_right.y, rolled_up.y, -forward.y, position.y),
        (rolled_right.z, rolled_up.z, -forward.z, position.z),
        (0.0, 0.0, 0.0, 1.0),
    ))

    data = bpy.data.cameras.new("OracleCamera")
    data.sensor_fit = "VERTICAL"
    data.sensor_height = declared["sensorHeightMm"]
    data.lens = declared["sensorHeightMm"] / (2.0 * math.tan(declared["yfovRad"] / 2.0))
    data.clip_start = declared["clipStartM"]
    data.clip_end = declared["clipEndM"]
    obj = bpy.data.objects.new("OracleCamera", data)
    scene.collection.objects.link(obj)
    obj.matrix_world = YUP_TO_ZUP @ basis
    scene.camera = obj
    return {"lensMm": data.lens, "sensorHeightMm": data.sensor_height, "sensorFit": data.sensor_fit,
            "matrixWorld": [list(row) for row in obj.matrix_world]}


def adopt_camera(scene, imported):
    cameras = [obj for obj in imported if obj.type == "CAMERA"]
    if len(cameras) != 1:
        fail("camera.source is gltf and the file carries %d cameras" % len(cameras))
    scene.camera = cameras[0]
    return {"lensMm": cameras[0].data.lens, "sensorFit": cameras[0].data.sensor_fit,
            "matrixWorld": [list(row) for row in cameras[0].matrix_world]}


def build_light(scene, declared):
    if declared["kind"] == "none":
        return {"kind": "none"}
    if declared["kind"] == "sun":
        data = bpy.data.lights.new("OracleLight", type="SUN")
        data.energy = declared["irradianceWPerM2"]
        data.angle = declared["angleRad"]
        data.color = tuple(declared["colourLinear"])
        obj = bpy.data.objects.new("OracleLight", data)
        scene.collection.objects.link(obj)
        direction = (YUP_TO_ZUP.to_3x3() @ Vector(declared["directionM"])).normalized()
        obj.rotation_mode = "QUATERNION"
        obj.rotation_quaternion = Vector((0.0, 0.0, -1.0)).rotation_difference(direction)
        return {"kind": "sun", "energyWPerM2": data.energy, "angleRad": data.angle,
                "directionBlender": list(direction)}
    data = bpy.data.lights.new("OracleLight", type="POINT")
    data.energy = declared["powerW"]
    data.shadow_soft_size = declared["radiusM"]
    data.color = tuple(declared["colourLinear"])
    obj = bpy.data.objects.new("OracleLight", data)
    scene.collection.objects.link(obj)
    obj.location = (YUP_TO_ZUP @ Vector(declared["positionM"]).to_4d()).to_3d()
    return {"kind": "point", "powerW": data.energy, "radiusM": data.shadow_soft_size,
            "locationBlender": list(obj.location)}


def lower_to_base_colour(imported):
    """Keeps the file's own base-colour image and its uv set; drops the closure around it.

    The importer builds a Principled BSDF whose specular lobe survives metallic 0 at IOR 1.5, so the
    render has an integral left and no closed form. Rewiring the same Image Texture node into a
    Diffuse BSDF at roughness 0 removes it and leaves rho(u,v)*L per texel. The image datablock is
    untouched, so its sRGB decode still happens where Blender does it -- at the texel, before the
    interpolation -- which is the convention under test.
    """
    rewired = []
    for material in bpy.data.materials:
        if material.node_tree is None:
            continue
        image_node = None
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image is not None:
                image_node = node
                break
        if image_node is None:
            continue
        tree = material.node_tree
        # Removing a node invalidates every other Python handle into the collection, so the node is
        # named before the removal and looked up again after it.
        kept = image_node.name
        for node in list(tree.nodes):
            if node.name != kept and node.type not in ("UVMAP", "MAPPING", "TEX_COORD"):
                tree.nodes.remove(node)
        image_node = tree.nodes[kept]
        output = tree.nodes.new("ShaderNodeOutputMaterial")
        shader = tree.nodes.new("ShaderNodeBsdfDiffuse")
        shader.inputs["Roughness"].default_value = 0.0
        tree.links.new(image_node.outputs["Color"], shader.inputs["Color"])
        tree.links.new(shader.outputs[0], output.inputs["Surface"])
        rewired.append({"material": material.name, "image": image_node.image.name,
                        "colourspace": image_node.image.colorspace_settings.name,
                        "interpolation": image_node.interpolation,
                        "extension": image_node.extension,
                        "size": list(image_node.image.size)})
    if not rewired:
        fail("material.source is gltf-base-colour and no imported material carries an image texture")
    meshes = sum(1 for obj in imported if obj.type == "MESH")
    return {"source": "gltf-base-colour", "meshes": meshes, "rewired": rewired}


def apply_material(imported, declared):
    if declared["source"] == "gltf":
        return {"source": "gltf"}
    if declared["source"] == "gltf-base-colour":
        return lower_to_base_colour(imported)
    material = bpy.data.materials.new("OracleMaterial")
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new("ShaderNodeOutputMaterial")
    if declared["kind"] == "diffuse":
        # Never Principled: at metallic 0 it still carries a specular lobe at IOR 1.5.
        shader = tree.nodes.new("ShaderNodeBsdfDiffuse")
        shader.inputs["Color"].default_value = tuple(declared["colourLinear"]) + (1.0,)
        # The node's Roughness switches the closure to Oren-Nayar above zero; the Cycles closure at
        # zero is exactly max(dot(N,w),0)/pi.
        shader.inputs["Roughness"].default_value = 0.0
    else:
        shader = tree.nodes.new("ShaderNodeEmission")
        shader.inputs["Color"].default_value = tuple(declared["colourLinear"]) + (1.0,)
        shader.inputs["Strength"].default_value = 1.0
    tree.links.new(shader.outputs[0], output.inputs["Surface"])
    meshes = 0
    for obj in imported:
        if obj.type != "MESH":
            continue
        obj.data.materials.clear()
        obj.data.materials.append(material)
        meshes += 1
    return {"source": "manifest", "kind": declared["kind"], "meshes": meshes}


def enable_devices(recipe):
    if recipe["device"] == "CPU":
        return {"device": "CPU", "backend": "NONE", "names": []}
    preferences = bpy.context.preferences.addons["cycles"].preferences
    preferences.compute_device_type = "METAL"
    preferences.get_devices()
    names = []
    for device in preferences.devices:
        device.use = device.type != "CPU"
        if device.use:
            names.append(device.name)
    return {"device": "GPU", "backend": preferences.compute_device_type, "names": names}


def apply_recipe(scene, recipe):
    scene.render.engine = "CYCLES"
    devices = enable_devices(recipe)
    scene.cycles.device = devices["device"]
    scene.render.resolution_x = recipe["resolutionX"]
    scene.render.resolution_y = recipe["resolutionY"]
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = recipe["filmTransparent"]
    scene.cycles.samples = recipe["samples"]
    scene.cycles.use_adaptive_sampling = recipe["adaptiveSampling"]
    scene.cycles.use_denoising = recipe["denoise"]
    scene.cycles.seed = recipe["seed"]
    scene.cycles.pixel_filter_type = recipe["pixelFilter"]["type"]
    scene.cycles.filter_width = recipe["pixelFilter"]["widthPx"]
    scene.cycles.film_exposure = recipe["filmExposure"]
    scene.unit_settings.scale_length = recipe["scaleLength"]
    bounces = recipe["bounces"]
    scene.cycles.max_bounces = bounces["max"]
    scene.cycles.diffuse_bounces = bounces["diffuse"]
    scene.cycles.glossy_bounces = bounces["glossy"]
    scene.cycles.transmission_bounces = bounces["transmission"]
    scene.cycles.volume_bounces = bounces["volume"]
    scene.cycles.transparent_max_bounces = bounces["transparentMax"]
    colour = recipe["colourManagement"]
    scene.display_settings.display_device = colour["displayDevice"]
    scene.view_settings.view_transform = colour["viewTransform"]
    scene.view_settings.look = colour["look"]
    scene.view_settings.exposure = colour["exposure"]
    scene.view_settings.gamma = colour["gamma"]
    return devices


RAW_MAGIC = b"OSRAWF32"
RAW_VERSION = 1
RAW_BYTE_ORDER = 0x01020304
RAW_CHANNELS = ("R", "G", "B", "A")
RAW_TOP_ROW_FIRST = 0


def save_products(scene, recipe, exr_path, raw_path):
    image = bpy.data.images["Render Result"]
    settings = scene.render.image_settings
    settings.file_format = "OPEN_EXR"
    settings.color_mode = "RGBA"
    settings.color_depth = "32"
    settings.exr_codec = recipe["exrCodec"]
    image.save_render(filepath=exr_path, scene=scene)
    return write_raw(exr_path, raw_path)


def write_raw(exr_path, raw_path):
    """The oracle's pixels in a form C++ can read: SDL3 has no EXR reader and none is worth vendoring.

    The samples come back through the EXR rather than through Render Result, which refuses pixel
    access in background mode.
    """
    loaded = bpy.data.images.load(exr_path)
    try:
        width, height = loaded.size
        samples = numpy.empty(width * height * len(RAW_CHANNELS), dtype=numpy.float32)
        loaded.pixels.foreach_get(samples)
    finally:
        bpy.data.images.remove(loaded)
    # Blender hands back the bottom row first; the header declares top row first because that is
    # what a raster readback on our side produces, and one of the two had to be named.
    samples = samples.reshape(height, width, len(RAW_CHANNELS))[::-1]

    names = b"".join(channel.encode("ascii") + b"\0" for channel in RAW_CHANNELS)
    header_bytes = (36 + len(names) + 3) // 4 * 4
    header = RAW_MAGIC
    header += struct.pack("=IIIIIII", RAW_BYTE_ORDER, RAW_VERSION, width, height,
                          len(RAW_CHANNELS), header_bytes, RAW_TOP_ROW_FIRST)
    header += names
    header += b"\0" * (header_bytes - len(header))
    with open(raw_path, "wb") as out:
        out.write(header)
        out.write(numpy.ascontiguousarray(samples).tobytes())
    return {"width": width, "height": height, "channels": list(RAW_CHANNELS),
            "headerBytes": header_bytes, "rowOrder": "top-first"}


def main():
    job = job_from_argv()
    scene = bpy.context.scene
    factory = {
        "world": observed_world(scene),
        "objects": sorted(obj.name for obj in bpy.data.objects),
        "engine": scene.render.engine,
        "viewTransform": scene.view_settings.view_transform,
    }
    clear_objects()
    apply_world(scene, job["scene"]["world"])
    imported = import_gltf(job["gltfPaths"])
    removed = strip_crossings(imported, job["scene"]["camera"]["source"])
    imported = [obj for obj in imported if obj.name in bpy.data.objects]
    if job["scene"]["camera"]["source"] == "manifest":
        camera = build_camera(scene, job["scene"]["camera"])
    else:
        camera = adopt_camera(scene, imported)
    light = build_light(scene, job["scene"]["light"])
    material = apply_material(imported, job["scene"]["material"])
    devices = apply_recipe(scene, job["recipe"])

    started = time.time()
    result = bpy.ops.render.render(write_still=False)
    seconds = time.time() - started
    if "FINISHED" not in result:
        fail("render returned " + repr(result))
    raw = save_products(scene, job["recipe"], job["exrPath"], job["rawPath"])

    provenance = {
        "raw": raw,
        "blenderVersion": bpy.app.version_string,
        "blenderBuildHash": bpy.app.build_hash.decode() if isinstance(bpy.app.build_hash, bytes) else str(bpy.app.build_hash),
        "factoryStartup": factory,
        "worldAtRender": observed_world(scene),
        "importedObjects": sorted(obj.name for obj in imported),
        "removedAtBoundary": removed,
        "camera": camera,
        "light": light,
        "material": material,
        "devices": devices,
        "renderSeconds": seconds,
        "polygons": sum(len(obj.data.polygons) for obj in imported if obj.type == "MESH"),
    }
    print(job["provenanceOpen"] + json.dumps(provenance) + job["provenanceClose"])


main()

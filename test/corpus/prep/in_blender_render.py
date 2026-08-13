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


def import_gltf(paths, lighting_mode):
    """`lighting_mode` is how the importer turns glTF's photometric units into Blender's, and it is
    the manifest's declaration rather than a default: RAW carries `intensity` across unchanged, which
    is one-to-one for a directional light's lux and Blender's Sun Strength, while COMPAT multiplies a
    point light's candela by 4pi, which is what makes Cycles radiate `intensity` per steradian."""
    try:
        bpy.ops.preferences.addon_enable(module="io_scene_gltf2")
    except Exception:
        pass
    before = set(bpy.data.objects)
    kept = []
    for path in paths:
        collections = set(bpy.data.collections)
        result = bpy.ops.import_scene.gltf(
            filepath=path, export_import_convert_lighting_mode=lighting_mode)
        if "FINISHED" not in result:
            fail("glTF import of " + path + " returned " + repr(result))
        kept.append(keep_default_scene(path, set(bpy.data.collections) - collections))
    return [obj for obj in bpy.data.objects if obj not in before], kept


def document_json(path):
    """The subject's own JSON, whichever of the format's two containers carries it. A GLB's first
    chunk is the JSON one by the format's rule, at byte 12 with an 8-byte chunk header; refusing
    anything else stops a reader that would otherwise decode a buffer as text."""
    with open(path, "rb") as f:
        payload = f.read()
    if payload[:4] != b"glTF":
        return json.loads(payload.decode("utf-8"))
    length, kind = struct.unpack_from("<II", payload, 12)
    if kind != 0x4E4F534A:
        fail("%s is a GLB whose first chunk is %#x and the format's first chunk is JSON" % (path, kind))
    return json.loads(payload[20:20 + length].decode("utf-8"))


def keep_default_scene(path, new_collections):
    """MEASURED, 5.2.0: the importer does not honour glTF's `scene` property -- it imports EVERY
    scene's nodes into the active Blender scene, one child collection per glTF scene, in document
    order. `MultipleScenes` then renders its triangle and its square at once, and the oracle would
    be showing a picture Khronos says is wrong.

    So the document's own statement is applied here, read out of the file rather than chosen: keep
    the collection at index `scene` and delete the objects of the others. The structural assumption
    -- one child collection per scene, in order -- is CHECKED against the document's scene count
    rather than trusted, because a Blender that changed it would otherwise silently keep the wrong
    geometry."""
    scenes = document_json(path).get("scenes") or []
    if len(scenes) <= 1:
        return {"scenes": len(scenes), "honoured": "single-scene"}
    default = document.get("scene", 0)
    # The importer REUSES an existing empty collection as the file's root, so the root is not
    # necessarily new; what is new is the per-scene collections under it.
    roots = [c for c in [bpy.context.scene.collection] + list(bpy.data.collections)
             if len(c.children) == len(scenes)
             and all(child in new_collections for child in c.children)]
    if len(roots) != 1:
        fail("the file declares %d scenes and %d collections hold exactly that many freshly "
             "imported children, so which collection is which scene cannot be decided"
             % (len(scenes), len(roots)))
    children = list(roots[0].children)
    if not 0 <= default < len(children):
        fail("the file's default scene is %r and the importer made %d scene collections"
             % (default, len(children)))
    dropped = []
    for index, collection in enumerate(children):
        if index == default:
            continue
        for obj in list(collection.all_objects):
            dropped.append(obj.name)
            bpy.data.objects.remove(obj, do_unlink=True)
    return {"scenes": len(scenes), "honoured": "default-scene", "defaultScene": default,
            "keptCollection": children[default].name, "droppedObjects": sorted(dropped)}


def strip_crossings(imported, camera_source, keep_lights):
    """What crossed the glTF boundary and must not have. Light and material are declared beside the
    asset, so whatever the file carried is deleted -- EXCEPT where the case declares the light as the
    file's, which is the one narrow arm doc/requirements.md I.26.12 opens and which exists because
    two Khronos assets state their criteria in terms of the light they carry."""
    removed = {"lights": 0, "cameras": 0}
    for obj in list(imported):
        if obj.type == "LIGHT" and not keep_lights:
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
    described = {}
    if declared.get("projection") == "orthographic":
        # `ortho_scale` IS THE EXTENT OF THE FITTED SENSOR AXIS, and the fit is VERTICAL here, so the
        # number is the vertical extent in metres -- twice glTF's `ymag`, which is a half-extent.
        data.type = "ORTHO"
        data.ortho_scale = 2.0 * declared["yMagM"]
        described = {"projection": "orthographic", "orthoScaleM": data.ortho_scale}
    else:
        data.sensor_height = declared["sensorHeightMm"]
        data.lens = declared["sensorHeightMm"] / (2.0 * math.tan(declared["yfovRad"] / 2.0))
        described = {"projection": "perspective", "lensMm": data.lens,
                     "sensorHeightMm": data.sensor_height}
    data.clip_start = declared["clipStartM"]
    data.clip_end = declared["clipEndM"]
    obj = bpy.data.objects.new("OracleCamera", data)
    scene.collection.objects.link(obj)
    obj.matrix_world = YUP_TO_ZUP @ basis
    scene.camera = obj
    described.update({"sensorFit": data.sensor_fit,
                      "matrixWorld": [list(row) for row in obj.matrix_world]})
    return described


def adopt_camera(scene, imported, declared, paths):
    """The camera the manifest names by its index into the file's own `cameras`, resolved WITHOUT
    relying on the order Blender happens to hand its objects back in.

    MEASURED, 5.2.0: the importer keeps no node index and no camera index on what it builds, and
    `bpy.data.objects` is ordered by name, so the nth imported camera is not the nth camera of the
    file. What the importer does keep is the camera's own declaration -- projection, clip range and
    either the vertical field of view or the orthographic scale -- so the declared camera is matched
    on those, and an asset whose cameras are indistinguishable under them is a REFUSAL naming the
    count rather than a pick."""
    documents = [(path, document_json(path)) for path in paths]
    carriers = [(path, document) for path, document in documents if document.get("cameras")]
    if len(carriers) != 1:
        fail("camera.source is gltf and %d of the %d subject files declare cameras, so the index "
             "names no one file" % (len(carriers), len(documents)))
    declarations = carriers[0][1]["cameras"]
    index = declared["index"]
    if not 0 <= index < len(declarations):
        fail("camera.index is %d and %s declares %d cameras" % (index, carriers[0][0],
                                                                len(declarations)))
    wanted = importer_camera(declarations[index])
    matched = [obj for obj in imported
               if obj.type == "CAMERA" and same_camera(importer_camera_of(obj.data), wanted)]
    if len(matched) != 1:
        fail("camera.index is %d, whose declaration imports as %r, and %d of the file's imported "
             "cameras carry that declaration" % (index, wanted, len(matched)))
    scene.camera = matched[0]
    return {"index": index, "lensMm": matched[0].data.lens, "camType": matched[0].data.type,
            "sensorFit": matched[0].data.sensor_fit, "matchedOn": list(wanted),
            "matrixWorld": [list(row) for row in matched[0].matrix_world]}


def importer_camera(declared):
    """What Blender's importer builds from one glTF camera declaration -- its own arithmetic,
    restated here because that is the only handle on which imported object came from which
    declaration. `blender/imp/camera.py`, 5.2.0."""
    if declared["type"] == "orthographic":
        lens = declared["orthographic"]
        return ("ORTHO", 2.0 * max(lens["xmag"], lens["ymag"]), lens["znear"], lens["zfar"])
    lens = declared["perspective"]
    # An absent `zfar` is an infinite frustum, which the importer spells as a big number.
    return ("PERSP", lens["yfov"], lens["znear"], lens.get("zfar", 1e12))


def importer_camera_of(data):
    if data.type == "ORTHO":
        return ("ORTHO", data.ortho_scale, data.clip_start, data.clip_end)
    return ("PERSP", data.angle_y, data.clip_start, data.clip_end)


def same_camera(observed, declared):
    """Blender's camera properties are single precision and its field of view round-trips through a
    focal length, so the comparison is to single-precision resolution and never exact. A slack that
    let two of a file's cameras match is not a wrong pick here: `adopt_camera` refuses on any count
    but one."""
    if observed[0] != declared[0]:
        return False
    return all(abs(a - b) <= 1e-6 * max(1.0, abs(b)) for a, b in zip(observed[1:], declared[1:]))


def observed_lights(imported):
    """Every light the import left standing, as it stands: what the manifest claims about the file's
    lights is then checkable against what Blender actually built, rather than asserted."""
    out = []
    for obj in imported:
        if obj.type != "LIGHT":
            continue
        entry = {"name": obj.name, "type": obj.data.type, "energy": obj.data.energy,
                 "color": list(obj.data.color), "locationBlender": list(obj.location),
                 "matrixWorld": [list(row) for row in obj.matrix_world]}
        if hasattr(obj.data, "shadow_soft_size"):
            # A DELTA SOURCE, AND IT IS WHAT MAKES THE TWO-SEED CHECK PASSABLE: a lamp with a radius
            # is an area light and Cycles samples its solid angle, which is an estimator with
            # variance. At radius zero there is one shadow ray and no integral left.
            obj.data.shadow_soft_size = 0.0
            entry["radiusM"] = obj.data.shadow_soft_size
        if obj.data.type == "SUN":
            obj.data.angle = 0.0
            entry["angleRad"] = obj.data.angle
        out.append(entry)
    return out


def build_light(scene, declared, imported):
    if declared["kind"] == "gltf":
        return {"kind": "gltf", "lightingMode": declared["lightingMode"],
                "lights": observed_lights(imported)}
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


def subject_materials(imported):
    """The materials the SUBJECT wears, in slot order, and never `bpy.data.materials`: the factory
    file ships its own ("Dots Stroke", "Material"), and rewriting those would be rewriting the
    startup file."""
    materials = []
    for obj in imported:
        if obj.type != "MESH":
            continue
        for slot in obj.material_slots:
            if slot.material is not None and slot.material not in materials:
                materials.append(slot.material)
    return materials


def surface_shader(tree):
    """The node the Material Output takes its Surface from, which is the closure to be replaced."""
    for node in tree.nodes:
        if node.type != "OUTPUT_MATERIAL":
            continue
        surface = node.inputs["Surface"]
        if surface.is_linked:
            return node, surface.links[0].from_node
    return None, None


def images_feeding(socket):
    """Which image datablocks actually reach this socket. `SciFiHelmet` carries four in one material
    and only one of them is its base colour, so a provenance line that listed every image node in
    the tree would report three textures the render never sampled."""
    found, seen, edge = [], set(), [socket]
    while edge:
        current = edge.pop()
        if not current.is_linked:
            continue
        node = current.links[0].from_node
        if node.name in seen:
            continue
        seen.add(node.name)
        if node.type == "TEX_IMAGE" and node.image is not None:
            found.append({"image": node.image.name,
                          "colourspace": node.image.colorspace_settings.name,
                          "interpolation": node.interpolation,
                          "extension": node.extension,
                          "size": list(node.image.size)})
        edge.extend(node.inputs)
    return found


def cull_back_faces(tree, shader):
    """THE ORACLE IS LOWERED, NOT THE TOLERANCE: Cycles has no back-face culling for camera rays.

    MEASURED on `TextureSettingsTest` at 5.2.0: the importer carries glTF's `doubleSided` into
    `Material.use_backface_culling`, which is a viewport and EEVEE setting, so the path tracer shades
    a single-sided triangle from behind exactly as from the front. The reference then showed a red X
    where Khronos's own criterion says a green checkmark belongs, and covered 482 pixels our render
    does not -- the back plate of a two-quad panel, poking one to two pixels past the front one at a
    grazing angle. Both are the same defect in the oracle and neither is a threshold to widen.

    A Transparent BSDF selected by `Geometry.Backfacing` is what expresses the format's own rule
    inside Cycles. It is deterministic and adds no integral: Cycles mixes closure WEIGHTS rather than
    choosing a branch at random, and `Backfacing` is exactly 0 or 1, so one closure carries weight 1
    and the other 0. The two-seed check is what holds that claim -- an emission case must come back
    bit-identical at another seed, and a stochastic mix would not."""
    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    transparent = tree.nodes.new("ShaderNodeBsdfTransparent")
    mix = tree.nodes.new("ShaderNodeMixShader")
    tree.links.new(geometry.outputs["Backfacing"], mix.inputs["Fac"])
    tree.links.new(shader.outputs[0], mix.inputs[1])
    tree.links.new(transparent.outputs[0], mix.inputs[2])
    return mix


def image_nodes_feeding(socket):
    """The TEX_IMAGE nodes that actually reach this socket, as nodes rather than as a report."""
    found, seen, edge = [], set(), [socket]
    while edge:
        current = edge.pop()
        if not current.is_linked:
            continue
        node = current.links[0].from_node
        if node.name in seen:
            continue
        seen.add(node.name)
        if node.type == "TEX_IMAGE" and node.image is not None:
            found.append(node)
        edge.extend(node.inputs)
    return found


def srgb_to_linear(encoded):
    """The transfer function glTF declares its colour images in, on a numpy array, verbatim."""
    return numpy.where(encoded <= 0.04045, encoded / 12.92,
                       numpy.power((encoded + 0.055) / 1.055, 2.4))


def linear_float_image(image, made):
    """CYCLES DECODES sRGB AFTER FILTERING FOR AN 8-BIT IMAGE, and that is exactly the defect
    `TextureLinearInterpolationTest` exists to detect. MEASURED at 5.2.0 on that asset: its second
    sphere samples a 2x1 image whose texels are sRGB 0 and sRGB 255 at the midpoint, and Cycles
    returned 0.21404117345809937 green, which is `srgb_to_linear(0.5)` to the last bit -- the two
    encoded codes averaged and then decoded. Decoding first gives exactly 0.5, which is what glTF
    requires and what the asset's own README calls the passing result.

    So the ORACLE IS LOWERED, NOT THE TOLERANCE, the same move `cull_back_faces` makes one function
    below for the back-face rule Cycles also does not implement: the image is handed to Cycles as a
    FLOAT buffer already holding linear values, so the filter has nothing left to decode and runs on
    the numbers the format says it must. No texel changes -- only the order of the decode and the
    filter, which is the whole of what the specification fixes.

    THE DECODE IS DONE HERE AND NOT TAKEN FROM BLENDER, because Blender does not have it to give.
    MEASURED: `Image.pixels` on an 8-bit sRGB image returns 0.19215688 for a texel of byte 49, which
    is 49/255 and not `srgb_to_linear(49/255) = 0.03071344` -- the float buffer is the ENCODED byte
    run and the transform lives in the sampler. A first version of this function trusted that buffer
    and was checked on a 2x1 image whose texels are 0 and 255, where encoded and decoded agree by
    construction: the check had no power to separate the two answers and passed a conversion that had
    deleted the decode instead of moving it. The label plate's grey 49 came back as 120.

    THE ASSET KEEPS ALL OF ITS DISCRIMINATING POWER, because it was never pointed at the reference:
    an engine of ours that decoded after filtering would now produce the 128 the reference used to
    produce, and disagree.

    Only CHANNEL_PACKED sources are accepted, which is what the glTF importer produces for every
    image in this corpus (MEASURED across all eight subjects): under any other alpha mode Blender's
    float buffer is associated and copying it into an unassociated one would premultiply the colour.
    """
    if image.name in made:
        return made[image.name]
    if image.alpha_mode != "CHANNEL_PACKED":
        fail("image %r has alpha mode %r and this conversion copies Blender's own float buffer, "
             "which is associated under any other mode" % (image.name, image.alpha_mode))
    if image.colorspace_settings.name not in ("sRGB", "Non-Color"):
        fail("image %r declares the colour space %r, and this conversion knows the sRGB transfer "
             "and the absence of one" % (image.name, image.colorspace_settings.name))
    width, height = image.size
    samples = numpy.empty(width * height * image.channels, dtype=numpy.float32)
    image.pixels.foreach_get(samples)
    if image.colorspace_settings.name == "sRGB":
        # Three channels carry the transfer and the fourth does not: alpha is a coverage, never a
        # colour, and glTF says so in the same sentence that declares the colour encoded.
        samples = samples.reshape(-1, image.channels)
        samples[:, :3] = srgb_to_linear(samples[:, :3])
        samples = samples.reshape(-1)
    copy = bpy.data.images.new(image.name + ".linear", width, height, alpha=True,
                               float_buffer=True, is_data=True)
    copy.alpha_mode = "CHANNEL_PACKED"
    copy.pixels.foreach_set(numpy.ascontiguousarray(samples, dtype=numpy.float32))
    made[image.name] = copy
    return copy


def decode_before_filtering(socket, made):
    """Retargets every image feeding this socket onto its linear float copy."""
    swapped = []
    for node in image_nodes_feeding(socket):
        if node.image.is_float:
            continue
        was = node.image.name
        node.image = linear_float_image(node.image, made)
        swapped.append({"was": was, "now": node.image.name})
    return swapped


def keep_alpha(tree, shader, alpha):
    """THE FILE'S OWN `alphaMode`, KEPT RATHER THAN RE-DECLARED.

    The Principled BSDF the importer builds carries glTF's coverage on its `Alpha` socket: a constant
    1.0 under `OPAQUE`, the base-colour alpha under `BLEND`, and under `MASK` a two-node chain
    `1 - (alpha < alphaCutoff)` that is exactly 0 or 1 (MEASURED on `TextureLinearInterpolationTest`
    at 5.2.0: SUBTRACT over LESS_THAN, with the cutoff as the comparison's second operand). Lowering
    the closure and dropping that socket is what makes a masked label plate render as a solid
    rectangle -- so the socket is moved onto a Transparent mix instead, which is where Cycles
    expresses coverage.

    A constant 1.0 gets no mix at all, so an opaque material's node graph is unchanged and the
    arithmetic that already produced six byte-identical cases is not touched.
    """
    if not alpha.is_linked and alpha.default_value >= 1.0:
        return shader, None
    transparent = tree.nodes.new("ShaderNodeBsdfTransparent")
    mix = tree.nodes.new("ShaderNodeMixShader")
    if alpha.is_linked:
        tree.links.new(alpha.links[0].from_socket, mix.inputs["Fac"])
        source = alpha.links[0].from_node.name + "." + alpha.links[0].from_socket.name
    else:
        mix.inputs["Fac"].default_value = alpha.default_value
        source = "unlinked default " + repr(alpha.default_value)
    # Fac 0 takes the first shader and Fac 1 the second, so coverage 1 must select the surface.
    tree.links.new(transparent.outputs[0], mix.inputs[1])
    tree.links.new(shader.outputs[0], mix.inputs[2])
    return mix, source


def lower_to_file_colour(imported, kind, socket_name, source_name):
    """Replaces the CLOSURE and keeps whatever the importer wired into one of its colour sockets.

    The Principled BSDF carries a specular lobe at IOR 1.5 whatever the metallic factor, so a render
    through it has an integral left and no closed form to be judged against. TWO CLOSURES ARE
    OFFERED AND THE CASE DECLARES WHICH: a Diffuse BSDF at roughness 0 is exactly
    max(dot(N,w),0)/pi in Cycles and returns rho*L under a uniform environment, but only where no
    surface can see another -- where one can, the single cosine-weighted direction at 1 spp either
    escapes or does not and the pixel is a Bernoulli draw on the visible sky fraction. An Emission
    at strength 1 gathers nothing at all, so it removes the world as a light, the sun's disk, a
    light's radius and visibility together, and a subject of several plates that shade one another
    has no other honest arm (doc/requirements.md I.26.13).

    WHICH SOCKET IS THE CASE'S DECLARATION, because it is a fact about the asset rather than about
    the oracle: `AlphaBlendModeTest` states its picture in `baseColorFactor`/`baseColorTexture`, and
    `TextureLinearInterpolationTest` states its whole picture in `emissiveFactor`/`emissiveTexture`
    over a base colour of `[0,0,0,1]` -- so an arm that could only read base colour renders its two
    spheres black and measures nothing.

    WHAT FEEDS THAT SOCKET IS NOT REBUILT, IT IS MOVED. glTF's base colour is `baseColorFactor`
    TIMES `baseColorTexture`, and the importer expresses that as a Mix(MULTIPLY) whose A is the
    Image Texture and whose B is the factor -- so a lowering that reached past it to the image node
    would drop the factor, and `TextureCoordinateTest` declares four different non-white ones. An
    untextured material is the same operation with an unlinked socket, which is why it needs no arm
    of its own: its socket is the factor as a default value and it lowers to a flat closure of that
    colour instead of being left Principled.

    The image datablock is untouched, so its sRGB decode still happens where Blender does it -- at
    the texel, before the interpolation -- which is the convention `TextureLinearInterpolationTest`
    decides.
    """
    rewired = []
    floats = {}
    for material in subject_materials(imported):
        tree = material.node_tree
        if tree is None:
            fail("material %r carries no node tree, so it has no colour to lower" % material.name)
        output, closure = surface_shader(tree)
        if closure is None:
            fail("material %r has no shader on its Material Output" % material.name)
        if socket_name not in closure.inputs:
            fail("material %r is shaded by a %s, which has no %s to keep"
                 % (material.name, closure.type, socket_name))
        base = closure.inputs[socket_name]
        # `KHR_materials_emissive_strength` ARRIVES ON THE CLOSURE AND IS CARRIED, NOT DROPPED. The
        # importer multiplies nothing: it puts the extension's factor on `Emission Strength` and
        # leaves `Emission Color` at `emissiveFactor`, so the lowered node has to take both or the
        # radiance is off by the factor the asset exists to test. A LINKED strength is still a
        # refusal, because a socket driven by a node is not a scalar this can carry over.
        strength = closure.inputs.get("Emission Strength")
        emitted = 1.0
        if socket_name == "Emission Color":
            if strength.is_linked:
                fail("material %r drives its emission strength from a node, and this arm emits at a "
                     "scalar strength -- a factor dropped here is a radiance nothing can attribute"
                     % material.name)
            emitted = float(strength.default_value)
        if kind == "emission":
            shader = tree.nodes.new("ShaderNodeEmission")
            shader.inputs["Strength"].default_value = emitted
        else:
            shader = tree.nodes.new("ShaderNodeBsdfDiffuse")
            shader.inputs["Roughness"].default_value = 0.0
        if base.is_linked:
            tree.links.new(base.links[0].from_socket, shader.inputs["Color"])
            kept = base.links[0].from_node.name + "." + base.links[0].from_socket.name
        else:
            shader.inputs["Color"].default_value = tuple(base.default_value)
            kept = "unlinked default " + repr(list(base.default_value))
        alpha = closure.inputs["Alpha"]
        swapped = decode_before_filtering(shader.inputs["Color"], floats)
        swapped += decode_before_filtering(alpha, floats)
        covered, coverage = keep_alpha(tree, shader, alpha)
        culled = cull_back_faces(tree, covered) if material.use_backface_culling else None
        tree.links.new((culled or covered).outputs[0], output.inputs["Surface"])
        tree.nodes.remove(closure)
        rewired.append({"material": material.name, "colourFrom": kept, "colourSocket": socket_name,
                        "emissionStrength": emitted,
                        "coverageFrom": coverage, "decodedBeforeFiltering": swapped,
                        "backFaceCulled": material.use_backface_culling,
                        "images": images_feeding(shader.inputs["Color"])})
    if not rewired:
        fail("material.source is %s and the subject wears no material at all" % source_name)
    meshes = sum(1 for obj in imported if obj.type == "MESH")
    return {"source": source_name, "kind": kind, "meshes": meshes, "rewired": rewired}


def diffuse_material(name, colour):
    """Never Principled: at metallic 0 it still carries a specular lobe at IOR 1.5.

    The node's Roughness switches the closure to Oren-Nayar above zero; the Cycles closure at zero is
    exactly max(dot(N,w),0)/pi, so a facet under a uniform environment returns rho*L with no
    integration left to perform.
    """
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new("ShaderNodeOutputMaterial")
    shader = tree.nodes.new("ShaderNodeBsdfDiffuse")
    shader.inputs["Color"].default_value = tuple(colour) + (1.0,)
    shader.inputs["Roughness"].default_value = 0.0
    tree.links.new(shader.outputs[0], output.inputs["Surface"])
    return material


def emission_material(name, colour):
    """A surface whose radiance IS the declared colour: no incoming light, no visibility test and no
    integral of any kind. It removes the world sampled as a light, the sun's disk, a light's radius
    and visibility at once, which is every estimator this recipe still carried."""
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new("ShaderNodeOutputMaterial")
    shader = tree.nodes.new("ShaderNodeEmission")
    shader.inputs["Color"].default_value = tuple(colour) + (1.0,)
    shader.inputs["Strength"].default_value = 1.0
    tree.links.new(shader.outputs[0], output.inputs["Surface"])
    return material


def apply_emission_per_material(imported, colours):
    """One emitter per glTF MATERIAL, which is the key a multi-material asset has.

    A mesh whose primitives name different materials arrives as ONE object with several slots, so a
    per-object colour could only ever reach one of them. The importer carries the material's own name
    across unchanged, so the manifest and the renderer key on the same string, and every slot of every
    object is answered.

    The datablock is rewired rather than replaced, because replacing it would renumber the slots the
    polygons point at and silently move colours between primitives. Only the IMPORTED objects' slots
    are walked: the factory file ships materials of its own ("Dots Stroke"), and a manifest that had
    to declare a colour for those would be declaring the startup file.
    """
    assigned = {}
    subject = []
    for obj in imported:
        if obj.type != "MESH":
            continue
        for slot in obj.material_slots:
            if slot.material is not None and slot.material not in subject:
                subject.append(slot.material)
    for material in subject:
        if material.name not in colours:
            fail("the scene carries the material %r and the manifest declares colours for %s"
                 % (material.name, ", ".join(sorted(colours))))
        material.use_nodes = True
        tree = material.node_tree
        tree.nodes.clear()
        output = tree.nodes.new("ShaderNodeOutputMaterial")
        shader = tree.nodes.new("ShaderNodeEmission")
        shader.inputs["Color"].default_value = tuple(colours[material.name]) + (1.0,)
        shader.inputs["Strength"].default_value = 1.0
        tree.links.new(shader.outputs[0], output.inputs["Surface"])
        assigned[material.name] = material.name
    for name in sorted(colours):
        if name not in assigned:
            fail("the manifest declares a colour for material %r and the scene carries no such "
                 "material" % name)
    return {"source": "manifest", "kind": "emission-per-material", "assigned": assigned}


def keep_file_materials(imported):
    """THE FILE'S OWN MATERIALS, UNTOUCHED, AND WHAT THAT COSTS RECORDED RATHER THAN HIDDEN.

    The whole point of this arm is that the Principled BSDF the importer wired IS the subject, so
    nothing about the closure is rewritten. What is NOT expressed here is glTF's front-face rule:
    Cycles performs no back-face culling for camera rays, and the Transparent-BSDF-on-`Backfacing`
    expression that the other arms use does not reproduce culling over a CLOSED body.

    MEASURED at Blender 5.2.0 on an inside-out UV sphere lit by a delta sun over a black world, with
    an emissive plane behind it: under the mix the pixel at the sphere's centre comes back exactly
    0 at transparent max bounces 0, 1, 2, 4, 8, 16 and 64, while a PURE Transparent BSDF on the same
    sphere shows the plane through it from 4 upwards. So the ray is stopped by the mix and the
    surface the cull should reveal is never shaded -- which is a hole in the oracle, not a threshold.

    THAT READING DOES NOT ISOLATE THE TECHNIQUE AND THE OPEN FIXTURE THAT DOES HAS NOW RUN. On a
    closed body 0 is equally the prediction of the trick WORKING on the near back face and the far
    hemisphere being opaque, so the decisive subject is one with no far face: a single quad with its
    back face to the camera and an emissive plane of (0.25, 0.5, 0.75) behind it. Measured at 5.2.0,
    64x64, 1 spp, box filter 0.01, black world, at transparent max bounces 0, 1, 2, 4, 8 and 64, the
    centre pixel reads -- mix: (0.25, 0.5, 0.75) at every one · pure Transparent BSDF: (0.25, 0.5,
    0.75) at every one · no mix at all: (1, 0, 0), the quad's own emission, which is the third arm
    confirming Cycles shades a back face for a camera ray. THE TECHNIQUE HOLDS ON AN OPEN SURFACE,
    identically to a pure Transparent BSDF and already at zero transparent bounces. What defeats it
    is the closed body and nothing else, so the scope written on this rule is the correct one.

    It bites on `DirectionalLight`, whose three spheres are wound clockwise as seen from outside
    (0 of 10600 triangles have a counter-clockwise outward normal) with vertex normals to match and
    a material that is not `doubleSided`: a conforming rasteriser culls the outer surface and shades
    the inner one, and Khronos's own published screenshot has the lit and dark limbs on the sides
    that produces. Cycles here shades the outer surface instead. The case that carries this asset is
    a `stated-invariant` case, so the oracle decides nothing about it and the disagreement is
    reported rather than tolerated as a tolerance.
    """
    observed = []
    for material in subject_materials(imported):
        _, closure = surface_shader(material.node_tree) if material.node_tree else (None, None)
        observed.append({"material": material.name,
                         "declaresBackFaceCulling": material.use_backface_culling,
                         "backFaceCulled": False,
                         "closure": closure.type if closure is not None else None})
    if not observed:
        fail("material.source is gltf and the subject wears no material at all")
    return {"source": "gltf", "observed": observed}


def apply_material(imported, declared):
    if declared["source"] == "gltf":
        return keep_file_materials(imported)
    if declared["source"] == "gltf-base-colour":
        return lower_to_file_colour(imported, declared["kind"], "Base Color", "gltf-base-colour")
    if declared["source"] == "gltf-emissive":
        return lower_to_file_colour(imported, declared["kind"], "Emission Color", "gltf-emissive")
    meshes = [obj for obj in imported if obj.type == "MESH"]
    if declared["kind"] == "diffuse":
        # ONE FACET, ONE COLOUR. The closed form rho*L is only available where nothing in the scene
        # can be seen from anything else, and a subject of several bodies is refused this arm by the
        # manifest before a render is made.
        if len(meshes) != 1:
            fail("material.kind is diffuse over %d meshes, and the closed form holds for a single "
                 "unoccluded facet" % len(meshes))
        material = diffuse_material("OracleMaterial", declared["colourLinear"])
        meshes[0].data.materials.clear()
        meshes[0].data.materials.append(material)
        return {"source": "manifest", "kind": "diffuse", "assigned": {meshes[0].name: material.name}}

    if declared["kind"] == "emission-per-material":
        return apply_emission_per_material(imported, declared["colourLinearPerMaterial"])

    # ONE MATERIAL PER OBJECT, KEYED BY THE glTF NODE'S OWN NAME, which the importer carries into the
    # object's name. A single colour over touching bodies fuses their silhouettes and hides a
    # misplaced node inside the union; the boundary between two declared colours is exact.
    colours = declared["colourLinearPerNode"]
    assigned = {}
    for obj in meshes:
        if obj.name not in colours:
            fail("the scene carries the object %r and the manifest declares colours for %s"
                 % (obj.name, ", ".join(sorted(colours))))
        material = emission_material("OracleEmission." + obj.name, colours[obj.name])
        obj.data.materials.clear()
        obj.data.materials.append(material)
        assigned[obj.name] = material.name
    for node in sorted(colours):
        if node not in assigned:
            fail("the manifest declares a colour for node %r and the scene carries no such object"
                 % node)
    return {"source": "manifest", "kind": "emission", "assigned": assigned}


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
    imported, defaultScenes = import_gltf(job["gltfPaths"],
                                          job["scene"]["light"].get("lightingMode", "RAW"))
    removed = strip_crossings(imported, job["scene"]["camera"]["source"],
                              job["scene"]["light"]["kind"] == "gltf")
    imported = [obj for obj in imported if obj.name in bpy.data.objects]
    imported = [obj for obj in imported if obj.name in bpy.data.objects]
    if job["scene"]["camera"]["source"] == "manifest":
        camera = build_camera(scene, job["scene"]["camera"])
    else:
        camera = adopt_camera(scene, imported, job["scene"]["camera"], job["gltfPaths"])
    light = build_light(scene, job["scene"]["light"], imported)
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
        "defaultScenePerFile": defaultScenes,
        "camera": camera,
        "light": light,
        "material": material,
        "devices": devices,
        "renderSeconds": seconds,
        "polygons": sum(len(obj.data.polygons) for obj in imported if obj.type == "MESH"),
    }
    print(job["provenanceOpen"] + json.dumps(provenance) + job["provenanceClose"])


main()

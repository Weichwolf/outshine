"""Runs inside Blender: the oracle render, from a job the preparer wrote."""

import base64
import urllib.parse
import json
import math
import os
import shutil
import struct
import sys
import tempfile
import time

import bpy
import numpy
from mathutils import Matrix, Quaternion, Vector

# THE SCRIPT IS HANDED TO BLENDER BY PATH, so its own directory is not on the import path and the
# reader beside it has no spelling without this line.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exr  # noqa: E402

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


def set_frame_grid(scene, animation, frame):
    """The declared grid, set BEFORE the import and the frame set after it.

    THE ORDER IS LOAD-BEARING. Blender's glTF importer converts a sampler's SECONDS into f-curve
    frames using the scene's frame rate at import time, so a rate set afterwards would leave every
    keyframe at the frame the factory rate put it at and every product would be the pose of a
    different instant. `fps_base` is pinned to 1 for the same reason: the pair is a rational rate and
    only the pair decides what a frame is worth.
    """
    scene.render.fps = int(animation["fps"]["value"])
    scene.render.fps_base = 1.0
    scene.frame_start = 0
    scene.frame_end = max(int(animation["frames"]["value"]) - 1, 0)
    scene.frame_set(int(frame))


def _channelbag_of(obj):
    """The f-curves that drive THIS object. Blender 5's action carries several slots and two objects
    of one glTF file share one action, so the channelbag is selected by the object's own slot handle
    -- taking the first one would resample another object's curves."""
    animation = obj.animation_data
    if animation is None or animation.action is None or animation.action_slot is None:
        return None
    for layer in animation.action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                if bag.slot_handle == animation.action_slot.handle:
                    return bag
    return None


_COMPONENT = {5126: ("f", 4), 5123: ("H", 2), 5125: ("I", 4), 5122: ("h", 2), 5121: ("B", 1), 5120: ("b", 1)}
_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def _accessor(document, buffers, index):
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    letter, size = _COMPONENT[accessor["componentType"]]
    wide = _COUNT[accessor["type"]]
    blob = buffers[view.get("buffer", 0)]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [struct.unpack_from("<" + letter * wide, blob, start + at * wide * size)
            for at in range(accessor["count"])]


def _slerp(first, second, unit):
    """glTF's LINEAR interpolation OF A ROTATION, written from the specification's own formula.

    A quaternion read component-wise is a different rotation everywhere except at the keys and at the
    exact midpoint of a span. Blender's importer reads them component-wise, which is why the oracle
    had to be corrected here at all -- so a sampler that treated `rotation` like `translation` would
    put that same defect back, on the side that is supposed to BE the specification."""
    dot = sum(first[c] * second[c] for c in range(4))
    sign = -1.0 if dot < 0.0 else 1.0
    angle = math.acos(min(1.0, abs(dot)))
    if math.sin(angle) < 1e-6:
        mixed = tuple(first[c] + (sign * second[c] - first[c]) * unit for c in range(4))
    else:
        near, far = math.sin(angle * (1.0 - unit)) / math.sin(angle), math.sin(angle * unit) / math.sin(angle)
        mixed = tuple(near * first[c] + sign * far * second[c] for c in range(4))
    return _unit(mixed)


def _unit(q):
    length = math.sqrt(sum(c * c for c in q))
    if length == 0.0:
        fail("a rotation sampled to a zero-length quaternion, which names no rotation")
    return tuple(c / length for c in q)


def _sampled(times, values, how, wide, at, spherical=False):
    """glTF's three interpolations, from the specification and not from a library.

    `spherical` is set for a `rotation` channel and it changes two of the three: LINEAR becomes a slerp,
    and CUBICSPLINE is normalised after the Hermite because the specification requires the result to be
    a unit quaternion and the cubic does not preserve length. STEP returns a stored key and needs
    neither."""
    if at <= times[0][0]:
        return values[1] if how == "CUBICSPLINE" else values[0]
    if at >= times[-1][0]:
        return values[-2] if how == "CUBICSPLINE" else values[-1]
    key = 0
    while key + 1 < len(times) and times[key + 1][0] <= at:
        key += 1
    first, second = times[key][0], times[key + 1][0]
    span = second - first
    unit = (at - first) / span
    if how == "STEP":
        return values[key]
    if how == "LINEAR":
        if spherical:
            return _slerp(values[key], values[key + 1], unit)
        return tuple(values[key][c] + (values[key + 1][c] - values[key][c]) * unit for c in range(wide))
    # CUBIC HERMITE over the in-tangent, value, out-tangent triples. THE TANGENTS ARE SCALED BY THE
    # SEGMENT DURATION -- that scaling is the whole difference from a Bezier with the same handles.
    value, out = values[3 * key + 1], values[3 * key + 2]
    into, next_value = values[3 * (key + 1)], values[3 * (key + 1) + 1]
    square, cube = unit * unit, unit * unit * unit
    hermite = tuple((2 * cube - 3 * square + 1) * value[c] + (cube - 2 * square + unit) * span * out[c] +
                    (-2 * cube + 3 * square) * next_value[c] + (cube - square) * span * into[c]
                    for c in range(wide))
    return _unit(hermite) if spherical else hermite


def document_buffers(path, document):
    """The subject's own buffers, whichever container carries them: a GLB's BIN chunk, a `.bin` beside
    the JSON, or a data URI. Refuses anything else by name rather than returning a short buffer that
    would decode as plausible numbers."""
    with open(path, "rb") as f:
        payload = f.read()
    inline = None
    if payload[:4] == b"glTF":
        at = 12
        while at + 8 <= len(payload):
            length, kind = struct.unpack_from("<II", payload, at)
            if kind == 0x004E4942:
                inline = payload[at + 8:at + 8 + length]
                break
            at += 8 + length + (-length % 4)
    out = []
    for buffer in document.get("buffers", []):
        uri = buffer.get("uri")
        if uri is None:
            if inline is None:
                fail(path + ": a buffer names no uri and the container carries no BIN chunk")
            out.append(inline)
        elif uri.startswith("data:"):
            out.append(base64.b64decode(uri.split(",", 1)[1]))
        else:
            beside = os.path.join(os.path.dirname(path), urllib.parse.unquote(uri))
            if not os.path.isfile(beside):
                fail(path + ": buffer " + uri + " is not beside the document")
            with open(beside, "rb") as b:
                out.append(b.read())
    return out



_CURVE_OF = {"translation": "location", "rotation": "rotation_quaternion", "scale": "scale"}

# glTF IS Y-UP AND BLENDER IS Z-UP, AND THE IMPORTER PUTS THAT CONVERSION ON THE ROOT OBJECTS ONLY
# (MEASURED, Blender 5.2.0). A root node translating (0, 2.52, 0) in the file arrives as
# (0, 0, 2.52); a child's channel arrives unconverted, because its parent already carries the
# conversion. Writing the file's own numbers into a root's curves would therefore move it along the
# wrong axis -- a picture, not a crash. `_agrees` re-derives the importer's first key from the file
# on every channel, so this hypothesis is checked per case rather than trusted.
_CONVERSION = Matrix.Rotation(math.radians(90.0), 4, "X")


def _to_blender(road, value, rooted):
    """One sampled glTF value in the slots Blender keys it in.

    THE QUATERNION IS REORDERED AND NOT REINTERPRETED: glTF stores (x, y, z, w) and Blender (w, x, y,
    z), so a component-for-component write is a different rotation. Scale crosses unchanged even at a
    root, because C.T.R.S decomposes as T(C.t).(C.R).S and the scale factors are in the node's own
    frame either way."""
    if road == "translation":
        vector = Vector(value[:3])
        return tuple(_CONVERSION @ vector) if rooted else tuple(vector)
    if road == "rotation":
        quaternion = Quaternion((value[3], value[0], value[1], value[2]))
        return tuple(_CONVERSION.to_quaternion() @ quaternion) if rooted else tuple(quaternion)
    return tuple(value)


def _agrees(road, ours, theirs):
    """A quaternion and its negation name one rotation, and the importer negates keys to keep a curve
    continuous -- so a rotation is compared up to sign and everything else is not."""
    straight = max(abs(ours[c] - theirs[c]) for c in range(len(ours)))
    if road != "rotation":
        return straight
    return min(straight, max(abs(ours[c] + theirs[c]) for c in range(len(ours))))


def _bone_curves(name, road):
    """The armature and the f-curves driving one JOINT's channel, or (None, None) if no bone owns it.

    A joint arrives as a pose bone inside an armature, never as an object, so the curve's data path is
    `pose.bones["<name>"].<slot>` and the object that carries the action is the armature."""
    for obj in bpy.data.objects:
        if obj.type != "ARMATURE" or name not in obj.pose.bones:
            continue
        bag = _channelbag_of(obj)
        if bag is None:
            continue
        path = 'pose.bones["' + name + '"].' + _CURVE_OF[road]
        curves = {fc.array_index: fc for fc in bag.fcurves if fc.data_path == path}
        if curves:
            return obj, curves
    return None, None


def _resample_bone(name, road, how, curves, frames, wide):
    """A JOINT'S CHANNEL, RE-INTERPOLATED IN THE SPACE THE IMPORTER PUT IT IN (board:1200).

    A pose bone's transform is relative to its REST pose and expressed in the bone's own axes, and
    glTF states neither -- the importer computed both. So the file's accessor values cannot be written
    onto a bone the way they are written onto an object. What is reused here is the importer's
    CONVERSION and what is replaced is its INTERPOLATION.

    THIS ARM IS STRICTLY NARROWER THAN THE OBJECT ARM AND SAYS SO. Re-interpolating the importer's own
    keys is exact only where the importer stored them exactly, which holds for STEP and for LINEAR and
    fails for CUBICSPLINE -- imported as BEZIER, whose handles are a different function from glTF's
    Hermite with duration-scaled tangents. A CUBICSPLINE channel on a joint is REFUSED rather than
    resampled, so a future asset that needs it stops here instead of rendering a Bezier.
    """
    if how == "CUBICSPLINE":
        fail("joint " + name + "'s " + road + " is CUBICSPLINE, and a joint's keys reach this "
             "preparer already converted into bone space, where a Bezier handle cannot be turned "
             "back into glTF's Hermite -- so this channel is refused rather than approximated")
    if len(curves) != wide:
        fail("joint " + name + "'s " + road + " is " + str(len(curves)) + " curves and the file's " +
             road + " carries " + str(wide) + " components")
    at = [key.co[0] for key in curves[0].keyframe_points]
    if not at:
        fail("joint " + name + "'s " + road + " carries no keyframe to resample")
    for component in range(wide):
        if [key.co[0] for key in curves[component].keyframe_points] != at:
            fail("the components of joint " + name + "'s " + road + " carry different keyframe times")
    keys = [tuple(curves[c].keyframe_points[k].co[1] for c in range(wide)) for k in range(len(at))]
    taken = []
    for frame in frames:
        taken.append((frame, _between(at, keys, float(frame), wide, how, road == "rotation")))
    for component in range(wide):
        points = curves[component].keyframe_points
        points.clear()
        points.add(len(taken))
        for index, (frame, value) in enumerate(taken):
            points[index].co = (float(frame), value[component])
            points[index].interpolation = "LINEAR"
        curves[component].update()
    return wide


def _between(at, keys, frame, wide, how, spherical):
    """STEP and LINEAR over keys already in the target's own space, clamped at both ends as glTF
    states. A rotation is interpolated on the sphere here for the same reason it is everywhere else --
    conjugating a quaternion into bone space is a rotation of quaternion space and slerp is unchanged
    by it, so the specification's rule survives the conversion the importer applied."""
    if frame <= at[0]:
        return keys[0]
    if frame >= at[-1]:
        return keys[-1]
    key = 0
    while key + 1 < len(at) and at[key + 1] <= frame:
        key += 1
    if how == "STEP":
        return keys[key]
    unit = (frame - at[key]) / (at[key + 1] - at[key])
    if spherical:
        return _slerp(keys[key], keys[key + 1], unit)
    return tuple(keys[key][c] + (keys[key + 1][c] - keys[key][c]) * unit for c in range(wide))


def _imported_name(document, index):
    """WHAT THE IMPORTER CALLED THIS glTF NODE. [MEASURED] Blender 5.2.0 on `BoxAnimated`, whose four
    nodes are all unnamed: a node carrying a mesh takes the MESH's name -- `inner_box`, `outer_box` --
    and one carrying none becomes an empty called `Node_<index>`.

    glTF does not require a node to have a name, so this cannot be skipped and it cannot be guessed at
    the point of use. It is a hypothesis about one importer version, which is why the caller checks the
    object it reaches against the file's own first key rather than trusting the name it arrived by."""
    node = document["nodes"][index]
    if node.get("name"):
        return node["name"]
    for kind, table in (("mesh", "meshes"), ("camera", "cameras")):
        if kind in node:
            named = document.get(table, [])[node[kind]].get("name")
            if named:
                return named
    return "Node_" + str(index)


def _write_frames(name, road, taken, wide, checkAt):
    """The evaluated poses, onto this object's own curves as one exact key per frame.

    THE OBJECT IS FOUND BY THE NODE'S NAME AND A MISS IS A REFUSAL. A channel silently dropped leaves
    that node at its rest pose on the oracle's side while ours moves, which reads as a shading or a
    raster disagreement and is neither -- so the name that did not resolve is said out loud.

    EVERY WRITTEN KEY IS `CONSTANT`-FREE AND `LINEAR` BETWEEN IDENTICAL NEIGHBOURS: there is a key at
    every frame the render visits, so what lies between two of them is never sampled. The interpolation
    written here therefore states nothing and cannot reintroduce the conversion this function exists to
    remove."""
    obj = bpy.data.objects.get(name) if name else None
    if obj is None:
        fail("the glTF names an animated node " + repr(name) +
             " and no imported object carries that name, so its channel would be silently dropped")
    bag = _channelbag_of(obj)
    if bag is None:
        fail(obj.name + " is animated by the file and carries no channelbag to write into")
    road_name = _CURVE_OF[road]
    curves = {fc.array_index: fc for fc in bag.fcurves if fc.data_path == road_name}
    if len(curves) != wide:
        fail(obj.name + "'s " + road_name + " is " + str(len(curves)) + " curves and the file's " +
             road + " carries " + str(wide) + " components")
    rooted = obj.parent is None
    # THE CONVERSION IS CHECKED BEFORE IT IS RELIED ON, ON EVERY CHANNEL OF EVERY CASE. The importer
    # has already placed this channel's first key in Blender's own space; deriving that same key from
    # the file and comparing is what turns `roots are converted, children are not` from a fact about
    # one Blender version into a claim this preparer restates every time it runs. A wrong axis or a
    # component-for-component quaternion is an O(1) disagreement here and a plausible picture later.
    if all(len(curves[c].keyframe_points) for c in range(wide)):
        theirs = tuple(curves[c].keyframe_points[0].co[1] for c in range(wide))
        ours = _to_blender(road, checkAt, rooted)
        apart = _agrees(road, ours, theirs)
        if apart > 1e-4:
            fail(obj.name + "'s " + road + " at the importer's first key is " + repr(theirs) +
                 " and the same key derived from the file is " + repr(ours) + ", apart by " +
                 repr(apart) + " -- so the axis convention assumed here is not the one the importer "
                 "used, and every baked key would be in the wrong space")
    written = [(frame, _to_blender(road, value, rooted)) for frame, value in taken]
    for component in range(wide):
        points = curves[component].keyframe_points
        points.clear()
        points.add(len(written))
        for at, (frame, value) in enumerate(written):
            points[at].co = (float(frame), value[component])
            points[at].interpolation = "LINEAR"
        curves[component].update()
    return wide

def baked_channels(scene, paths, fps, declared):
    """EVERY ANIMATED CHANNEL, WRITTEN TO THE FRAME GRID AS EXACT KEYS (board:1198, board:1175).

    THE GRID IS BLENDER FRAMES AND THE SAMPLER IS IN SECONDS. `scene.frame_start` is 0 and `fps_base`
    is pinned to 1, so frame *f* is the instant *f/fps* -- and that conversion is the whole reason this
    function takes `fps` rather than reading times off the imported curves the way the slerp reduction
    it replaces did.

    The pose at each frame is evaluated from the file's own accessors against the specification, so the
    oracle is asked only to render a stated pose and never to reproduce a sampler it converted on
    import. This subsumes the LINEAR-quaternion reduction above: slerp is what the specification says a
    LINEAR rotation is, and it is evaluated here for the same reason CUBICSPLINE is.

    A NODE THE IMPORTER DID NOT NAME BACK IS A REFUSAL, not a skip: a channel silently dropped would
    leave that node at its rest pose on the oracle's side and moving on ours, which reads as a shading
    or raster disagreement and is neither."""
    baked, left = [], []
    for path in paths:
        document = document_json(path)
        if not document.get("animations"):
            continue
        # THE DECLARED SUBSET AND NOT THE FILE'S WHOLE LIST. glTF states animations are independent
        # and a client plays any subset, so which of them play is the case's declaration -- and a
        # baker that played all of them would make the picture a function of the FILE rather than of
        # the manifest, which is the one property this engine does not trade.
        for which in declared:
            if which < 0 or which >= len(document["animations"]):
                fail(path + " carries " + str(len(document["animations"])) +
                     " animations and the case declares index " + str(which))
        buffers = document_buffers(path, document)
        for index in declared:
            animation = document["animations"][index]
            for channel in animation.get("channels", []):
                target = channel.get("target", {})
                node = target.get("node")
                road = target.get("path")
                if node is None:
                    continue
                if road == "weights":
                    left.append({"animation": index, "node": node, "why": "morph weights"})
                    continue
                sampler = animation["samplers"][channel["sampler"]]
                how = sampler.get("interpolation", "LINEAR")
                times = _accessor(document, buffers, sampler["input"])
                values = _accessor(document, buffers, sampler["output"])
                wide = len(values[0])
                name = _imported_name(document, node)
                grid = list(range(scene.frame_start, scene.frame_end + 1))
                # A JOINT IS A POSE BONE AND NOT AN OBJECT, so the arms are dispatched on which of
                # them owns the name -- and they are NOT the same mechanism. The object arm evaluates
                # the FILE; the bone arm re-interpolates the IMPORTER'S keys, because a bone's
                # transform is rest-relative in axes glTF never states. Which arm ran is published
                # per channel rather than left to be inferred from the asset.
                armature, curves = _bone_curves(name, road)
                if armature is not None:
                    written = _resample_bone(name, road, how, curves, grid, wide)
                    carried = "bone:" + armature.name
                else:
                    taken = []
                    for frame in grid:
                        taken.append((frame, _sampled(times, values, how, wide, frame / float(fps),
                                                      road == "rotation")))
                    # THE IMPORTER'S OWN FIRST KEY IS THE WITNESS: the value compared against it is
                    # this sampler at the file's first key time -- the stored key for STEP and
                    # LINEAR, the middle of the triple for CUBICSPLINE, which is what the importer
                    # put there.
                    written = _write_frames(name, road, taken, wide,
                                            _sampled(times, values, how, wide, times[0][0],
                                                     road == "rotation"))
                    carried = "object"
                baked.append({"animation": index, "node": node, "name": name, "carriedBy": carried,
                              "path": road, "interpolation": how, "keyframes": len(times),
                              "frames": len(grid), "curves": written})
    return {"baked": baked, "leftAlone": left}

def evaluated_pose(imported):
    """Where the oracle actually put each object at this frame, in Blender's own +Z-up metres.

    IT IS A DIAGNOSTIC AND NOT A VERDICT: a picture disagreement over an animated case is either the
    pose or the raster, and without this the two are not separable without a second run.
    """
    depsgraph = bpy.context.evaluated_depsgraph_get()
    pose = {}
    for obj in imported:
        if obj.name not in bpy.data.objects:
            continue
        matrix = bpy.data.objects[obj.name].evaluated_get(depsgraph).matrix_world
        pose[obj.name] = [list(row) for row in matrix]
    return pose


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
    document = document_json(path)
    scenes = document.get("scenes") or []
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
    file's, which is the one narrow arm board:0085 opens and which exists because
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


def select_material_variant(imported, name):
    """THE ORACLE READS `KHR_materials_variants` ITSELF (board:1188), which is what makes it an
    independent answer to which material a primitive wears rather than a second copy of ours.

    Blender's importer stores the extension verbatim: the file's variant names land in
    `scene.gltf2_KHR_materials_variants_variants` and each primitive's mappings in the mesh's
    `gltf2_variant_mesh_data`, one entry per (slot, material, variants) the file states. Assigning
    the active variant is the importer's own operator over that data, so nothing here re-implements
    the mapping -- what is added is the DECLARATION and the refusal.

    A NAME THE FILE DOES NOT DECLARE STOPS THE RENDER AND NAMES BOTH SIDES. Falling back to the
    imported default would produce the file's own materials, which for a shoe whose default IS one of
    its variants is a picture nobody can tell from the right one.

    The slots are read back AFTER the switch and reported, because "the operator ran" and "the
    material moved" are two claims and only the second one is about the picture."""
    # THE PROPERTY IS NOT ALWAYS THERE, AND ITS ABSENCE IS AN ANSWER. The importer registers the
    # variant UI only when a file it imported declares variants, so on the 37 cases that declare none
    # this attribute does not exist -- which is exactly "the file declares no variants" and is
    # reported as that rather than crashing the render of a case this feature is not about.
    declared = list(getattr(bpy.data.scenes[0], "gltf2_KHR_materials_variants_variants", []))
    if name is None:
        return {"declared": [v.name for v in declared], "active": None}
    for index, variant in enumerate(declared):
        if variant.name != name:
            continue
        bpy.data.scenes[0].gltf2_active_variant = index
        result = bpy.ops.scene.gltf2_display_variant()
        if "FINISHED" not in result:
            fail("displaying material variant %r returned %r" % (name, result))
        return {"declared": [v.name for v in declared], "active": name, "activeIndex": index,
                "slots": [{"object": obj.name,
                           "materials": [s.material.name if s.material else None
                                         for s in obj.material_slots]}
                          for obj in imported if obj.type == "MESH"]}
    fail("the case declares material variant %r and the imported files declare %r"
         % (name, [v.name for v in declared]))


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
    has no other honest arm (board:0087).

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

    THE COLOUR IS THE MANIFEST'S AND THE FRONT-FACE RULE IS STILL THE FILE'S, which is why the
    datablock being rewired rather than replaced matters twice: `use_backface_culling` is what the
    importer put glTF's `doubleSided` into, and it survives here. MEASURED on `NegativeScaleTest`:
    its background is two single-sided quads at z = -0.10 and z = -0.15, the second facing away, and
    with no cull the reference's silhouette is the UNION of the two -- 495 pixels wide of the front
    plate's right edge, because a plate 0.05 m further from an off-axis eye projects 1.3 px to the
    side. Our silhouette reproduces the front plate alone to 1 px and the reference reproduces the
    union to 1 px, so the two masks are pictures of a different SET OF FACES rather than of a
    different placement.
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
        culled = cull_back_faces(tree, shader) if material.use_backface_culling else None
        tree.links.new((culled or shader).outputs[0], output.inputs["Surface"])
        assigned[material.name] = {"material": material.name,
                                   "backFaceCulled": material.use_backface_culling}
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
    materials = subject_materials(imported)
    observed = []
    for material in materials:
        _, closure = surface_shader(material.node_tree) if material.node_tree else (None, None)
        observed.append({"material": material.name,
                         "declaresBackFaceCulling": material.use_backface_culling,
                         "backFaceCulled": False,
                         "closure": closure.type if closure is not None else None})
    if not observed:
        fail("material.source is gltf and the subject wears no material at all")
    return {"source": "gltf", "observed": observed,
            "notALight": no_surface_of_the_subject_is_a_light(imported, materials)}


def no_surface_of_the_subject_is_a_light(imported, materials):
    """THE SUBJECT IS SEEN AND NEVER GATHERED FROM, which is what removes the estimator an emissive
    asset carries into a scene whose only declared source has no area (board:0087).

    IT TAKES BOTH HALVES AND EITHER ALONE LEAVES AN INTEGRAL, because Cycles reaches a surface's
    emission by two routes. Next-event estimation puts the emissive triangles in the light tree
    beside the declared light and picks ONE per shading event; `emission_sampling = NONE` takes them
    out of it. The integrator then still traces the one BSDF-sampled direction at zero bounces and
    adds whatever emission that direction lands on; ray visibility is what stops it, and over a black
    world it removes nothing else, since a gathering ray that meets no emitter returns zero whether
    it escapes or is stopped.

    CAMERA AND SHADOW VISIBILITY ARE UNTOUCHED, so the emissive map appears exactly where the texture
    says it does and the body still casts its own shadow -- which is the whole of what these cases
    are about, and the reason this is a reduction of the oracle rather than a tolerance on it.
    """
    for material in materials:
        material.cycles.emission_sampling = "NONE"
    gathered_from = []
    for obj in imported:
        if obj.type != "MESH":
            continue
        obj.visible_diffuse = False
        obj.visible_glossy = False
        obj.visible_transmission = False
        obj.visible_volume_scatter = False
        gathered_from.append({"object": obj.name, "camera": obj.visible_camera,
                              "shadow": obj.visible_shadow, "diffuse": obj.visible_diffuse,
                              "glossy": obj.visible_glossy,
                              "transmission": obj.visible_transmission,
                              "volumeScatter": obj.visible_volume_scatter})
    return {"emissionSampling": sorted(set(m.cycles.emission_sampling for m in materials)),
            "visibility": gathered_from}


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


def ask_for_quantities(scene, passes, work_directory, recipe):
    """Turn on the render passes the job asks for and route each to its own EXR.

    ONE FILE-OUTPUT NODE PER QUANTITY. Blender 5.2's node writes OPEN_EXR_MULTILAYER and nothing else
    -- its format enum has one member and the per-item override is accepted without effect -- so
    several slots on one node would produce one file of several layers. One node with one slot writes
    one layer, and `exr.py` reads it either way; the split is what keeps a quantity's file openable
    by a person as well as by the reader.

    THE BEAUTY PATH IS UNTOUCHED AND THAT IS LOAD-BEARING. This node group writes files and returns
    nothing -- it has no output node, so `Render Result` is the render itself and not something the
    compositor rebuilt. Blender 5.2 removed `CompositorNodeComposite`, and leaving that output absent
    is what keeps the picture out of this path. The caller PROVES it by holding the beauty dump
    against the one the corpus already carries; a single differing byte stops the round.

    EACH QUANTITY DECLARES ITS OWN SOCKET AND VIEW-LAYER FLAG (manifest.QUANTITY_PASSES), so a third
    quantity is a row there and not a branch here. Every socket is taken as RGBA whatever it carries,
    because a one-channel index and a three-channel normal in two formats would be two readers.

    MATERIAL INDICES ARE ASSIGNED HERE, in name order, because `Material Index` reports
    `material.pass_index` and Blender leaves that at 0 for everything -- an index pass over a scene
    nobody indexed is a field of zeros that looks like an answer. The mapping goes into provenance,
    because an index the runner cannot resolve to a material names nothing.
    """
    layer = scene.view_layers[0]
    indexed = {"materials": [], "objects": []}
    for at, material in enumerate(sorted(bpy.data.materials, key=lambda m: m.name)):
        material.pass_index = at + 1
        indexed["materials"].append({"name": material.name, "passIndex": at + 1})
    # THE OBJECT INDEX NEEDS THE SAME TREATMENT AND MEASURING IT IS WHY IT IS HERE: with only the
    # materials indexed, the object pass came back ZERO EVERYWHERE and a field of zeros reads as an
    # answer. That is the failure this table refuses roughness over, so it is not tolerated here.
    for at, obj in enumerate(sorted(bpy.data.objects, key=lambda o: o.name)):
        obj.pass_index = at + 1
        indexed["objects"].append({"name": obj.name, "passIndex": at + 1})
    for quantity, spec in sorted(passes.items()):
        setattr(layer, spec["viewLayerFlag"], True)

    tree = bpy.data.node_groups.new("outshine-quantities", "CompositorNodeTree")
    scene.compositing_node_group = tree
    layers = tree.nodes.new("CompositorNodeRLayers")
    layers.scene = scene
    layers.layer = layer.name

    written = {}
    for quantity, spec in sorted(passes.items()):
        socket = spec["socket"]
        if socket not in layers.outputs:
            fail("the render layer offers no socket named " + repr(socket) + "; it offers " +
                 ", ".join(repr(o.name) for o in layers.outputs))
        output = tree.nodes.new("CompositorNodeOutputFile")
        output.directory = work_directory
        output.file_name = quantity
        output.format.color_mode = "RGBA"
        output.format.color_depth = "32"
        output.format.exr_codec = recipe["exrCodec"]
        output.file_output_items.clear()
        output.file_output_items.new("RGBA", quantity)
        tree.links.new(layers.outputs[socket], output.inputs[quantity])
        written[quantity] = socket
    return {"indices": indexed, "passes": written}


def collect_quantities(work_directory, paths):
    """Move each slot's frame file to the path the job named, and dump it in the picture's layout.

    A FILE-OUTPUT SLOT NAMES ITS FILE AFTER THE FRAME and this preparer names its products after the
    quantity, so the rename is not cosmetic: without it the product a cache key was computed for and
    the file on disk have different names, and the store would publish whichever the glob found.
    """
    collected = {}
    for quantity, target in sorted(paths.items()):
        candidates = [os.path.join(work_directory, name)
                      for name in sorted(os.listdir(work_directory))
                      if name.startswith(quantity) and name.endswith(".exr")]
        if len(candidates) != 1:
            fail("the compositor wrote " + str(len(candidates)) + " files for quantity " +
                 repr(quantity) + " in " + work_directory)
        os.replace(candidates[0], target["exr"])
        collected[quantity] = write_raw(target["exr"], target["raw"])
    return collected


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

    THROUGH OUR OWN READER (`exr.py`) AND NOT THROUGH BLENDER'S. Blender's loader opens a multilayer
    EXR at `size (0, 0)` -- a file its own compositor wrote -- so every quantity dumped through it
    came out as a header with no samples. One reader for every product is also what keeps the picture
    and the quantities in one format: this function is on the BEAUTY path too, and the caller holds
    its output against the corpus's committed `oracle.raw` to prove the swap changed nothing.

    A LAYER PREFIX IS STRIPPED HERE AND NOWHERE ELSE. A single-layer file from the compositor names
    its channels `<quantity>.R`; the beauty file names them `R`. The raw layout is the same either
    way, so the prefix is dropped at the one point that writes the layout.
    """
    width, height, planes = exr.read(exr_path)
    named = {}
    for name, plane in planes.items():
        named[name.rsplit(".", 1)[-1]] = plane
    missing = [c for c in RAW_CHANNELS if c not in named]
    if missing:
        fail(exr_path + ": no channel named " + ", ".join(missing) + "; it carries " +
             ", ".join(sorted(planes)))
    samples = numpy.stack([named[c] for c in RAW_CHANNELS], axis=-1).astype(numpy.float32)

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
    animation = job["scene"].get("animation")
    if animation is not None:
        set_frame_grid(scene, animation, job["frame"])
    imported, defaultScenes = import_gltf(job["gltfPaths"],
                                          job["scene"]["light"].get("lightingMode", "RAW"))
    removed = strip_crossings(imported, job["scene"]["camera"]["source"],
                              job["scene"]["light"]["kind"] == "gltf")
    imported = [obj for obj in imported if obj.name in bpy.data.objects]
    if job["scene"]["camera"]["source"] == "manifest":
        camera = build_camera(scene, job["scene"]["camera"])
    else:
        camera = adopt_camera(scene, imported, job["scene"]["camera"], job["gltfPaths"])
    light = build_light(scene, job["scene"]["light"], imported)
    # BEFORE THE MATERIAL ARM AND AFTER THE IMPORT (board:1188): the lowering below rewires the
    # materials the subject WEARS, so a variant selected after it would swap a lowered material for
    # an untouched Principled one.
    variant = select_material_variant(imported, job["scene"].get("materialVariant"))
    material = apply_material(imported, job["scene"]["material"])
    devices = apply_recipe(scene, job["recipe"])
    channels = None
    if animation is not None:
        channels = baked_channels(scene, job["gltfPaths"],
                                  scene.render.fps / scene.render.fps_base,
                                  animation["animations"])
        scene.frame_set(int(job["frame"]))
    quantity_work = tempfile.mkdtemp(prefix="outshine-quantities-")
    quantities = ask_for_quantities(scene, job["quantityPasses"], quantity_work, job["recipe"])

    started = time.time()
    result = bpy.ops.render.render(write_still=False)
    seconds = time.time() - started
    if "FINISHED" not in result:
        fail("render returned " + repr(result))
    raw = save_products(scene, job["recipe"], job["exrPath"], job["rawPath"])
    quantities["collected"] = collect_quantities(quantity_work, job["quantityPaths"])
    shutil.rmtree(quantity_work, ignore_errors=True)

    provenance = {
        "raw": raw,
        "quantities": quantities,
        "blenderVersion": bpy.app.version_string,
        "blenderBuildHash": bpy.app.build_hash.decode() if isinstance(bpy.app.build_hash, bytes) else str(bpy.app.build_hash),
        "factoryStartup": factory,
        "worldAtRender": observed_world(scene),
        "importedObjects": sorted(obj.name for obj in imported),
        "removedAtBoundary": removed,
        "defaultScenePerFile": defaultScenes,
        "camera": camera,
        "frame": job["frame"],
        "fps": scene.render.fps / scene.render.fps_base,
        "bakedChannels": channels,
        "pose": evaluated_pose(imported),
        "light": light,
        "materialVariant": variant,
        "material": material,
        "devices": devices,
        "renderSeconds": seconds,
        "polygons": sum(len(obj.data.polygons) for obj in imported if obj.type == "MESH"),
    }
    print(job["provenanceOpen"] + json.dumps(provenance) + job["provenanceClose"])


main()

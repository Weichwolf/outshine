"""The generated subjects: geometry this repository owns, so a red rung has exactly one cause.

Every fixture is a `.glb` -- one file, one hash, no relative path to resolve -- and every one carries
POSITION and NORMAL, because a subject that made the reader derive a normal would put a smoothing
decision inside the rung whose subject is the reader.

The numbers a fixture is built from are the manifest's, not this file's: a shape reads its parameters
and refuses a key it does not know, so a parameter that silently did not apply has no spelling.

TWO KINDS OF SUBJECT ARRIVE THROUGH ONE ENTRY POINT AND THE SHAPE DECIDES WHICH. The shapes below
are the RUNG'S OWN FIXTURES -- a triangle, a quad, a cube -- written here because their whole point
is that nothing of the engine stands behind them. A shape of `grown.SHAPES` is the opposite subject:
a part the engine grew, produced by running the engine (`grown.py`), because a second implementation
of a generator in this file would score a tree the engine does not draw.
"""

import json
import math
import struct

from prep import vendor

grown = vendor.beside(__file__, "grown")
from prep.refusal import Refusal

GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

COMPONENT_TYPES = {"UNSIGNED_BYTE": (5121, "B", 1), "UNSIGNED_SHORT": (5123, "H", 2),
                   "UNSIGNED_INT": (5125, "I", 4)}
MODES = {"TRIANGLES": 4, "TRIANGLE_STRIP": 5, "TRIANGLE_FAN": 6}
FLOAT32 = 5126


def generate(where, recipe):
    """The bytes of one fixture, and what its producer reported about it -- empty for the shapes
    written here, because the whole of their report is the recipe that is already in the manifest."""
    if not isinstance(recipe, dict):
        raise Refusal(where, expected="an object", observed=type(recipe).__name__)
    if recipe.get("shape") in grown.SHAPES:
        field = _fields(where, recipe, ("shape", "parameters"))
        return grown.build(where, field["shape"], field["parameters"])
    field = _fields(where, recipe, ("shape", "parameters"), ("mode", "indexComponent", "camera"))
    shape = field["shape"]
    if shape not in _SHAPES:
        raise Refusal(where + ".shape",
                      expected="one of " + ", ".join(sorted(set(_SHAPES) | set(grown.SHAPES))),
                      observed=shape)
    mode = _one_of(where + ".mode", field.get("mode", "TRIANGLES"), sorted(MODES))
    component = _one_of(where + ".indexComponent", field.get("indexComponent", "UNSIGNED_INT"),
                        sorted(COMPONENT_TYPES))
    parts = _SHAPES[shape](where + ".parameters", field["parameters"], mode)
    camera = _camera(where + ".camera", field["camera"]) if "camera" in field else None
    return _write_glb(parts, mode, component, camera), {}


# ---------------------------------------------------------------- the shapes

class _Part:
    """One mesh at one node's placement. `run` is in the primitive's own mode, not triangles.

    EVERY NODE IS NAMED, because a declaration that says something per node -- what each part of the
    subject emits -- resolves against the name and against nothing else. A position in a list is a
    second thing to keep in step, on both sides of a comparison, and Blender's importer keys its own
    objects by the same name.
    """

    def __init__(self, positions, normals, run, name, node=None):
        self.positions = positions
        self.normals = normals
        self.run = run
        self.name = name
        self.node = node or {}


def _triangle(where, parameters, mode):
    field = _fields(where, parameters, ("legM",))
    leg = _positive(where + ".legM", field["legM"])
    _only_triangles(where, mode, "a triangle")
    # RIGHT-ISOCELES AND NOT EQUILATERAL, and the reason is arithmetic rather than taste: an
    # equilateral triangle has interior angles of 60 deg, so if one edge's raster slope is rational
    # the other two are tan(t +/- 60 deg) = (t -/+ sqrt 3) / (1 +/- t sqrt 3), irrational for every
    # rational t. No roll makes its silhouette rational, so it can never carry the exactness
    # construction of `board/`, which is written for this shape and names it.
    # Legs along +X and +Y about the bounding box's centre, in the plane z = 0, wound
    # counter-clockwise from +Z.
    half = 0.5 * leg
    positions = [(-half, -half, 0.0), (half, -half, 0.0), (-half, half, 0.0)]
    return [_Part(positions, [(0.0, 0.0, 1.0)] * 3, [0, 1, 2], "triangle")]


def _quad(where, parameters, mode):
    field = _fields(where, parameters, ("halfWidthM", "halfHeightM"))
    half_width = _positive(where + ".halfWidthM", field["halfWidthM"])
    half_height = _positive(where + ".halfHeightM", field["halfHeightM"])
    positions = [(-half_width, -half_height, 0.0), (half_width, -half_height, 0.0),
                 (-half_width, half_height, 0.0), (half_width, half_height, 0.0)]
    # One surface, three spellings of it. The strip's own flip rule makes its two triangles the same
    # two the list names; the fan's diagonal is the other one, which is why it is the same SURFACE
    # and not the same triangulation.
    runs = {"TRIANGLES": [0, 1, 2, 2, 1, 3], "TRIANGLE_STRIP": [0, 1, 2, 3], "TRIANGLE_FAN": [0, 1, 3, 2]}
    return [_Part(positions, [(0.0, 0.0, 1.0)] * 4, runs[mode], "quad")]


_CUBE_FACES = (
    ((1, 0, 0), ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1))),
    ((-1, 0, 0), ((-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1))),
    ((0, 1, 0), ((-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1))),
    ((0, -1, 0), ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1))),
    ((0, 0, 1), ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1))),
    ((0, 0, -1), ((1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1))),
)


def _cube_mesh(half_extent):
    positions, normals, run = [], [], []
    for normal, corners in _CUBE_FACES:
        base = len(positions)
        for corner in corners:
            positions.append(tuple(axis * half_extent for axis in corner))
            normals.append(tuple(float(axis) for axis in normal))
        run.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return positions, normals, run


def _cube(where, parameters, mode):
    field = _fields(where, parameters, ("halfExtentM",))
    half_extent = _positive(where + ".halfExtentM", field["halfExtentM"])
    _only_triangles(where, mode, "a cube")
    positions, normals, run = _cube_mesh(half_extent)
    return [_Part(positions, normals, run, "cube")]


def _uv_sphere(where, parameters, mode):
    field = _fields(where, parameters, ("radiusM", "segments", "rings"))
    radius = _positive(where + ".radiusM", field["radiusM"])
    segments = _count(where + ".segments", field["segments"], 3)
    rings = _count(where + ".rings", field["rings"], 2)
    _only_triangles(where, mode, "a sphere")

    positions, normals = [], []
    for ring in range(rings + 1):
        polar = math.pi * ring / rings
        for segment in range(segments + 1):
            azimuth = 2.0 * math.pi * segment / segments
            unit = (math.sin(polar) * math.cos(azimuth), math.cos(polar),
                    math.sin(polar) * math.sin(azimuth))
            normals.append(unit)
            positions.append(tuple(axis * radius for axis in unit))

    run = []
    for ring in range(rings):
        for segment in range(segments):
            here = ring * (segments + 1) + segment
            below = here + segments + 1
            # Counter-clockwise seen from OUTSIDE, which is glTF's front face and the direction the
            # NORMAL attribute already points. Wound the other way round the shape is unchanged and
            # every visible face is a back face: Cycles then rejects each sampled direction against
            # the inward geometric normal and the whole sphere renders black -- measured, 46 101 of
            # 46 151 covered pixels at exactly 0.0, which is what this order costs when it is wrong.
            #
            # The pole rows would give a triangle of zero area on one side of every quad, and a
            # degenerate triangle is a face the oracle's importer and our rasteriser may disagree
            # about for a reason that has nothing to do with the silhouette.
            if ring != 0:
                run.extend([here, here + 1, below])
            if ring != rings - 1:
                run.extend([here + 1, below + 1, below])
    return [_Part(positions, normals, run, "sphere")]


def _nested_cubes(where, parameters, mode):
    field = _fields(where, parameters, ("halfExtentM", "placement", "chain"))
    half_extent = _positive(where + ".halfExtentM", field["halfExtentM"])
    placement = _one_of(where + ".placement", field["placement"], ("trs", "matrix"))
    _only_triangles(where, mode, "a node chain")
    if not isinstance(field["chain"], list) or not field["chain"]:
        raise Refusal(where + ".chain", expected="a non-empty list of placements", observed=repr(field["chain"]))

    positions, normals, run = _cube_mesh(half_extent)
    parts = []
    for depth, level in enumerate(field["chain"]):
        step = _fields(where + ".chain[%d]" % depth, level, ("translationM", "rotationXyzw", "scale"))
        translation = _vector(where + ".chain[%d].translationM" % depth, step["translationM"], 3)
        rotation = _vector(where + ".chain[%d].rotationXyzw" % depth, step["rotationXyzw"], 4)
        scale = _vector(where + ".chain[%d].scale" % depth, step["scale"], 3)
        node = {"matrix": _trs(translation, rotation, scale)} if placement == "matrix" else {
            "translation": translation, "rotation": rotation, "scale": scale}
        parts.append(_Part(positions, normals, run, "level%d" % depth, node))
    return parts


_SHAPES = {"triangle": _triangle, "quad": _quad, "cube": _cube, "uv-sphere": _uv_sphere,
           "nested-cubes": _nested_cubes}


# ---------------------------------------------------------------- the camera

def _camera(where, declared):
    if not isinstance(declared, dict):
        raise Refusal(where, expected="an object", observed=type(declared).__name__)
    kind = _one_of(where + ".kind", declared.get("kind"), ("perspective", "orthographic"))
    common = ("kind", "znearM", "zfarM", "positionM", "lookAtM", "rollRad")
    lens = ("yfovRad",) if kind == "perspective" else ("xmagM", "ymagM")
    field = _fields(where, declared, common + lens, ("note",))
    position = _vector(where + ".positionM", field["positionM"], 3)
    aim = _vector(where + ".lookAtM", field["lookAtM"], 3)
    matrix = _look_at(where, position, aim, _number(where + ".rollRad", field["rollRad"]))
    if kind == "perspective":
        block = {"type": "perspective",
                 "perspective": {"yfov": _positive(where + ".yfovRad", field["yfovRad"]),
                                 "znear": _positive(where + ".znearM", field["znearM"]),
                                 "zfar": _positive(where + ".zfarM", field["zfarM"])}}
    else:
        block = {"type": "orthographic",
                 "orthographic": {"xmag": _positive(where + ".xmagM", field["xmagM"]),
                                  "ymag": _positive(where + ".ymagM", field["ymagM"]),
                                  "znear": _positive(where + ".znearM", field["znearM"]),
                                  "zfar": _positive(where + ".zfarM", field["zfarM"])}}
    return {"camera": block, "matrix": matrix}


def _look_at(where, position, aim, roll):
    """glTF's camera looks down -Z in its node's space, so the node's columns are right, up, -forward."""
    forward = _normalised(where, [aim[axis] - position[axis] for axis in range(3)])
    right = _normalised(where, _cross(forward, [0.0, 1.0, 0.0]))
    up = _cross(right, forward)
    turn, lean = math.cos(roll), math.sin(roll)
    rolled_right = [right[axis] * turn + up[axis] * lean for axis in range(3)]
    rolled_up = [up[axis] * turn - right[axis] * lean for axis in range(3)]
    return (rolled_right + [0.0] + rolled_up + [0.0] +
            [-forward[axis] for axis in range(3)] + [0.0] + list(position) + [1.0])


# ---------------------------------------------------------------- the container

def _write_glb(parts, mode, component, camera):
    document = {"asset": {"version": "2.0", "generator": "outshine test/harness/shared/corpus"},
                "scene": 0, "scenes": [{"nodes": []}], "nodes": [], "meshes": [],
                "accessors": [], "bufferViews": [], "buffers": []}
    binary = bytearray()

    for part in parts:
        position = _accessor(document, binary, _floats(part.positions), FLOAT32, "VEC3",
                             len(part.positions), bounds=part.positions)
        normal = _accessor(document, binary, _floats(part.normals), FLOAT32, "VEC3", len(part.normals))
        code, pack, width = COMPONENT_TYPES[component]
        ceiling = (1 << (8 * width)) - 1
        for index in part.run:
            if index > ceiling:
                raise Refusal("index " + str(index), expected="at most " + str(ceiling) + " under " + component,
                              observed=str(index), why="an index width is a declaration about the mesh it carries")
        indices = _accessor(document, binary, struct.pack("<%d%s" % (len(part.run), pack), *part.run),
                            code, "SCALAR", len(part.run), target=34963)
        document["meshes"].append({"primitives": [
            {"attributes": {"POSITION": position, "NORMAL": normal}, "indices": indices,
             "mode": MODES[mode]}]})
        node = dict(part.node)
        node["name"] = part.name
        node["mesh"] = len(document["meshes"]) - 1
        document["nodes"].append(node)

    # The chain is nested: node n is node n-1's child, and only the first is a root of the scene.
    for at in range(len(parts) - 1):
        document["nodes"][at]["children"] = [at + 1]
    document["scenes"][0]["nodes"] = [0] if parts else []

    if camera is not None:
        document["cameras"] = [camera["camera"]]
        document["nodes"].append({"camera": 0, "matrix": camera["matrix"]})
        document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)

    document["buffers"] = [{"byteLength": len(binary)}]
    text = json.dumps(document, separators=(",", ":"), sort_keys=True).encode("utf-8")
    text += b" " * (-len(text) % 4)
    binary += b"\0" * (-len(binary) % 4)
    total = 12 + 8 + len(text) + 8 + len(binary)
    out = struct.pack("<III", GLB_MAGIC, GLB_VERSION, total)
    out += struct.pack("<II", len(text), CHUNK_JSON) + text
    out += struct.pack("<II", len(binary), CHUNK_BIN) + bytes(binary)
    return out


def _accessor(document, binary, payload, component, element, count, bounds=None, target=None):
    binary += b"\0" * (-len(binary) % 4)
    view = {"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)}
    if target is not None:
        view["target"] = target
    binary += payload
    document["bufferViews"].append(view)
    accessor = {"bufferView": len(document["bufferViews"]) - 1, "componentType": component,
                "count": count, "type": element}
    if bounds is not None:
        accessor["min"] = [min(point[axis] for point in bounds) for axis in range(3)]
        accessor["max"] = [max(point[axis] for point in bounds) for axis in range(3)]
    document["accessors"].append(accessor)
    return len(document["accessors"]) - 1


def _floats(points):
    flat = [component for point in points for component in point]
    return struct.pack("<%df" % len(flat), *flat)


# ---------------------------------------------------------------- arithmetic and refusals

def _trs(translation, rotation, scale):
    """Column-major T * R * S, the format's own composition order."""
    x, y, z, w = rotation
    basis = ((1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w)),
             (2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w)),
             (2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)))
    out = []
    for column in range(3):
        out.extend([basis[column][row] * scale[column] for row in range(3)] + [0.0])
    return out + list(translation) + [1.0]


def _cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def _normalised(where, vector):
    length = math.sqrt(sum(component * component for component in vector))
    if length == 0.0:
        raise Refusal(where, expected="a direction", observed="a zero-length vector")
    return [component / length for component in vector]


def _fields(where, value, required, optional=()):
    if not isinstance(value, dict):
        raise Refusal(where, expected="an object", observed=type(value).__name__)
    known = set(required) | set(optional)
    for key in sorted(value):
        if key not in known:
            raise Refusal(where + "." + key, expected="one of " + ", ".join(sorted(known)), observed=key,
                          why="a parameter nobody reads is a shape that silently did not change")
    for key in required:
        if key not in value:
            raise Refusal(where, expected="the field " + repr(key), observed="absent")
    return value


def _one_of(where, value, allowed):
    if value not in allowed:
        raise Refusal(where, expected="one of " + ", ".join(allowed), observed=repr(value))
    return value


def _number(where, value):
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise Refusal(where, expected="a number", observed=repr(value))
    return float(value)


def _positive(where, value):
    number = _number(where, value)
    if not number > 0.0:
        raise Refusal(where, expected="a positive number", observed=repr(value))
    return number


def _count(where, value, floor):
    if not isinstance(value, int) or isinstance(value, bool) or value < floor:
        raise Refusal(where, expected="an integer of at least %d" % floor, observed=repr(value))
    return value


def _vector(where, value, length):
    if not isinstance(value, list) or len(value) != length:
        raise Refusal(where, expected="%d numbers" % length, observed=repr(value))
    return [_number(where, component) for component in value]


def _only_triangles(where, mode, subject):
    if mode != "TRIANGLES":
        raise Refusal(where, expected="TRIANGLES", observed=mode,
                      why=subject + " has no strip or fan spelling that describes the same surface")

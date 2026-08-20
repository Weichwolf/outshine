"""The declared corrections applied to a fetched subject, between the fetch and the oracle.

WHY A PATCH EXISTS AT ALL. An upstream asset may be malformed relative to its own published
appearance -- `DirectionalLight` is wound clockwise from outside with every vertex normal pointing
inward, and reproduces Khronos's own screenshot only on a renderer that does not cull. Correcting our
RENDERER would make us non-conforming, because glTF requires back-face culling for a material that
declares no `doubleSided`. Correcting the ASSET produces a file a conforming renderer draws correctly,
and both sides of the comparison then render the same corrected bytes.

IT IS A DELTA AND NEVER A REPLACEMENT. The manifest keeps upstream's sha256, the fetch still verifies
it, and the patch is stated on top of the verified bytes -- so a change upstream is still a refusal
and nothing in the tree pretends to be somebody else's file.

THE DECLARED MEASUREMENT IS RECOMPUTED AND NOT TRUSTED, which is what makes it load-bearing: a
transform whose declared `observedBefore` does not match what is on disk refuses, so a patch applied
twice is a refusal rather than a silent return to the state it corrected.
"""

import array
import json
import os

from .refusal import Refusal

_COMPONENT = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
_COMPONENTS_OF = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
_TRIANGLES = 4

def apply(subject, destination):
    """Every declared correction of one subject, in the order the manifest states them."""
    report = []
    for position, transform in enumerate(subject.patch["transforms"]):
        where = "manifest subject %s patch.transforms[%d]" % (subject.id, position)
        correction = _CORRECTIONS.get(transform["kind"])
        if correction is None:
            raise Refusal(where + ".kind", expected="one of " + ", ".join(sorted(_CORRECTIONS)),
                          observed=transform["kind"])
        report.append(correction(where, subject, destination, transform))
    return report

class _Buffers:
    """The glTF and its external buffers, decoded far enough to address one accessor's elements.

    NARROW ON PURPOSE. It reads what this transform writes and refuses everything else by name: an
    interleaved accessor, a sparse one, an embedded data URI, a `.glb`. A patch that quietly did the
    wrong thing to a layout it did not understand is worse than one that does not run.
    """

    def __init__(self, where, directory, entry):
        self.where = where
        self.directory = directory
        if entry.lower().endswith(".glb"):
            raise Refusal(where, expected="a .gltf with its buffers beside it", observed=entry,
                          why="this transform rewrites a buffer in place and a GLB carries its "
                              "buffer inside the file it would have to re-lay-out")
        with open(os.path.join(directory, entry), "r") as f:
            self.document = json.load(f)
        self.buffers = []
        self.names = []
        for index, buffer in enumerate(self.document.get("buffers", [])):
            uri = buffer.get("uri")
            if not uri or uri.startswith("data:"):
                raise Refusal(where + ".buffers[%d]" % index, expected="a file beside the glTF",
                              observed=repr(uri))
            self.names.append(uri)
            with open(os.path.join(directory, uri), "rb") as f:
                self.buffers.append(bytearray(f.read()))

    def read(self, index):
        """One accessor's elements as a flat `array`, plus what is needed to write them back."""
        accessor = self.document["accessors"][index]
        if "sparse" in accessor:
            raise Refusal(self.where + ".accessors[%d]" % index, expected="a dense accessor",
                          observed="sparse")
        view = self.document["bufferViews"][accessor["bufferView"]]
        if view.get("byteStride", 0) not in (0, _element_bytes(accessor)):
            raise Refusal(self.where + ".accessors[%d]" % index, expected="tightly packed elements",
                          observed="byteStride %d" % view["byteStride"])
        code, width = _COMPONENT[accessor["componentType"]]
        count = accessor["count"] * _COMPONENTS_OF[accessor["type"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        values = array.array(code)
        values.frombytes(bytes(self.buffers[view["buffer"]][start:start + count * width]))
        return values, view["buffer"], start

    def write(self, values, buffer, start):
        raw = values.tobytes()
        self.buffers[buffer][start:start + len(raw)] = raw

    def flush(self):
        for name, content in zip(self.names, self.buffers):
            with open(os.path.join(self.directory, name), "wb") as f:
                f.write(content)

def _element_bytes(accessor):
    return _COMPONENT[accessor["componentType"]][1] * _COMPONENTS_OF[accessor["type"]]

def _reverse_winding(where, subject, destination, transform):
    """Every triangle that faces inward, wound the other way. No vertex moves."""
    files = _Buffers(where, destination, subject.entry)
    primitives = _triangle_primitives(where, files.document)
    before = _agreed(where, transform, files, primitives, _WINDING)

    flipped = set()
    for primitive in primitives:
        positions, _, _ = files.read(primitive["attributes"]["POSITION"])
        centre = _centre(positions)
        indices, buffer, at = files.read(primitive["indices"])
        outward, _ = _outward_triangles(positions, indices, centre)
        if outward * 2 >= len(indices) // 3 or primitive["indices"] in flipped:
            continue
        for corner in range(0, len(indices) - 2, 3):
            indices[corner + 1], indices[corner + 2] = indices[corner + 2], indices[corner + 1]
        files.write(indices, buffer, at)
        flipped.add(primitive["indices"])
    files.flush()

    after = _took(where, destination, subject, primitives, _WINDING)
    return {"kind": transform["kind"], "subject": subject.id, "observedBefore": before,
            "observedAfter": after, "buffersRewritten": sorted(files.names),
            "indexAccessorsReversed": sorted(flipped)}

def _flip_normals(where, subject, destination, transform):
    """Every vertex normal that points inward, negated. No position and no index is touched."""
    files = _Buffers(where, destination, subject.entry)
    primitives = _triangle_primitives(where, files.document)
    before = _agreed(where, transform, files, primitives, _NORMALS)

    flipped = set()
    for primitive in primitives:
        positions, _, _ = files.read(primitive["attributes"]["POSITION"])
        centre = _centre(positions)
        normals, buffer, at = files.read(primitive["attributes"]["NORMAL"])
        if (_outward_normals(positions, normals, centre) * 2 >= len(normals) // 3 or
                primitive["attributes"]["NORMAL"] in flipped):
            continue
        for component in range(len(normals)):
            normals[component] = -normals[component]
        files.write(normals, buffer, at)
        flipped.add(primitive["attributes"]["NORMAL"])
    files.flush()

    after = _took(where, destination, subject, primitives, _NORMALS)
    return {"kind": transform["kind"], "subject": subject.id, "observedBefore": before,
            "observedAfter": after, "buffersRewritten": sorted(files.names),
            "normalAccessorsFlipped": sorted(flipped)}

_CORRECTIONS = {"reverse-winding": _reverse_winding, "flip-normals": _flip_normals}

def _agreed(where, transform, files, primitives, owned):
    """What is on disk before the correction, recomputed and held against what was declared."""
    before = {name: value for name, value in _count(files, primitives).items() if name in owned}
    _agrees(where, transform["observedBefore"], before)
    return before

def _took(where, destination, subject, primitives, owned):
    """THE CORRECTION IS RE-MEASURED FROM THE FILE ON DISK and not from the buffers in hand, so a
    write that never reached the file is a refusal rather than a report of what was intended."""
    after = {name: value for name, value in
             _count(_Buffers(where, destination, subject.entry), primitives).items()
             if name in owned}
    if owned is _WINDING:
        owed = after["triangles"] - after["trianglesOfZeroArea"]
        if after["trianglesCounterClockwiseFromOutside"] != owed:
            raise Refusal(
                where,
                expected="every triangle that HAS a facing wound counter-clockwise from outside",
                observed="%d of %d" % (after["trianglesCounterClockwiseFromOutside"], owed),
                why="this correction answers a consistently inside-out body; a mesh it leaves mixed "
                    "is one it does not understand, and a half-corrected asset is worse than the "
                    "original",
            )
    elif after["vertexNormalsOutward"] != after["vertexNormals"]:
        raise Refusal(
            where,
            expected="every vertex normal pointing outward",
            observed="%d of %d" % (after["vertexNormalsOutward"], after["vertexNormals"]),
            why="this correction answers a consistently inside-out body, and a mesh it leaves mixed "
                "is one it does not understand",
        )
    return after

_WINDING = ("triangles", "trianglesCounterClockwiseFromOutside", "trianglesOfZeroArea")
_NORMALS = ("vertexNormals", "vertexNormalsOutward")

def _triangle_primitives(where, document):
    """Every drawn primitive of the document. Outward is measured from each primitive's OWN vertex
    mean, so a subject of several bodies is corrected body by body rather than about one shared
    point that lies inside none of them."""
    found = []
    for mesh in document.get("meshes", []):
        for primitive in mesh["primitives"]:
            if primitive.get("mode", _TRIANGLES) != _TRIANGLES:
                raise Refusal(where, expected="triangles", observed="mode %d in mesh %s" % (
                    primitive.get("mode"), mesh.get("name", "")))
            for owed in ("POSITION", "NORMAL"):
                if owed not in primitive["attributes"]:
                    raise Refusal(where, expected=owed, observed="mesh %s carries none" % mesh.get("name", ""))
            if "indices" not in primitive:
                raise Refusal(where, expected="an index accessor",
                              observed="mesh %s draws unindexed" % mesh.get("name", ""))
            found.append(primitive)
    if not found:
        raise Refusal(where, expected="a triangle primitive", observed="none")
    return found

def _centre(positions):
    total = [0.0, 0.0, 0.0]
    vertices = len(positions) // 3
    for vertex in range(vertices):
        for axis in range(3):
            total[axis] += positions[vertex * 3 + axis]
    return [component / vertices for component in total]

def _outward_triangles(positions, indices, centre):
    """How many triangles face outward, and how many have no facing to ask about.

    A TRIANGLE OF EXACTLY ZERO AREA IS NOT MIS-WOUND, IT IS UNORIENTED. Its cross product is the zero
    vector, so no winding makes it point anywhere, and counting it as inward would make a corrected
    asset fail its own verification for something a flip cannot change. It is counted separately and
    published rather than absorbed into a tolerance -- this asset carries six, two at each sphere's
    pole seam.
    """
    outward = degenerate = 0
    for corner in range(0, len(indices) - 2, 3):
        a, b, c = (indices[corner], indices[corner + 1], indices[corner + 2])
        v0 = positions[a * 3:a * 3 + 3]
        v1 = positions[b * 3:b * 3 + 3]
        v2 = positions[c * 3:c * 3 + 3]
        e1 = [v1[k] - v0[k] for k in range(3)]
        e2 = [v2[k] - v0[k] for k in range(3)]
        normal = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                  e1[0] * e2[1] - e1[1] * e2[0]]
        if normal == [0.0, 0.0, 0.0]:
            degenerate += 1
            continue
        away = [(v0[k] + v1[k] + v2[k]) / 3.0 - centre[k] for k in range(3)]
        if sum(normal[k] * away[k] for k in range(3)) > 0.0:
            outward += 1
    return outward, degenerate

def _outward_normals(positions, normals, centre):
    outward = 0
    for vertex in range(len(normals) // 3):
        along = sum(normals[vertex * 3 + k] * (positions[vertex * 3 + k] - centre[k])
                    for k in range(3))
        if along > 0.0:
            outward += 1
    return outward

def _count(files, primitives):
    triangles = ccw = flat = vertices = outward = 0
    for primitive in primitives:
        positions, _, _ = files.read(primitive["attributes"]["POSITION"])
        centre = _centre(positions)
        normals, _, _ = files.read(primitive["attributes"]["NORMAL"])
        indices, _, _ = files.read(primitive["indices"])
        faces, unoriented = _outward_triangles(positions, indices, centre)
        triangles += len(indices) // 3
        ccw += faces
        flat += unoriented
        vertices += len(normals) // 3
        outward += _outward_normals(positions, normals, centre)
    return {"triangles": triangles, "trianglesCounterClockwiseFromOutside": ccw,
            "trianglesOfZeroArea": flat, "vertexNormals": vertices,
            "vertexNormalsOutward": outward}

def _agrees(where, declared, observed):
    for name in sorted(observed):
        if name not in declared:
            raise Refusal(where + ".observedBefore", expected="a declared " + name,
                          observed="absent",
                          why="a transform states the state it expects to find, and this preparer "
                              "recomputes every one of them")
    for name in sorted(declared):
        if name not in observed:
            raise Refusal(where + ".observedBefore." + name,
                          expected="one of " + ", ".join(sorted(observed)), observed=name)
        if declared[name]["value"] != observed[name]:
            raise Refusal(where + ".observedBefore." + name, expected=repr(declared[name]["value"]),
                          observed=repr(observed[name]),
                          why="the asset on disk is not the one this patch was measured against, "
                              "and a correction applied to something else is not a correction")

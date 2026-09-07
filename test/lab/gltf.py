"""THE LAB WRITES glTF, BECAUSE THAT IS WHAT THE ENGINE READS.

The twin handed Blender a hand-written binary PLY -- a format only the viewer understands, with
no materials in it, so a look had to be rebuilt on the other side. glTF 2.0 carries the geometry
AND the material, Blender imports it natively, and `src/import/` already reads it: the same file
is the reference picture's input and the C++ side's, which is what makes one a check on the other.

THE AXES ARE glTF'S, AND THAT IS THE WHOLE POINT. glTF 2.0 states +Y up, +Z toward the viewer,
right-handed, metres; the tree's own frame is East-North-Up. So the boundary converts

    (East, North, Up)  ->  (East, Up, -North)

and nothing else. `CLAUDE.md`: "a file format is somebody else's convention ... an importer
converts INTO the tree's convention", which reads the same way outward. Written the other way --
inverted against `src/import/Axes.cpp`'s `InEcef` so the C++ would read it back unchanged --
Blender loaded the same file lying on its side, because Blender follows the standard. A file
that only one reader understands is the PLY this replaces.

`check_round_trip` states the claim as a comparison, and `check_blender_agrees` in the lab's
render path is the other half: a box written here and read by Blender's own importer stands the
way it was built.

THE MATERIAL IS THE MATERIAL. `materials.Material.to_gltf()` already answers in glTF 2.0's own
fields -- `baseColorFactor`, `metallicFactor`, `roughnessFactor`, `KHR_materials_ior`,
`KHR_materials_emissive_strength`, `extras` for grain, relief, bond and unit -- so this writes
what it says instead of translating it.
"""
import json
import struct

import numpy as np

FLOAT, USHORT, UINT = 5126, 5123, 5125
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963


def to_gltf_axes(v):
    """(East, North, Up) -> glTF's (x, y, z) = (East, Up, -North). glTF 2.0 is +Y up."""
    v = np.asarray(v, dtype=np.float32).reshape(-1, 3)
    return np.column_stack((v[:, 0], v[:, 2], -v[:, 1])).astype(np.float32)


def from_gltf_axes(v):
    """And back: (x, y, z) -> (East, North, Up) = (x, -z, y)."""
    v = np.asarray(v, dtype=np.float32).reshape(-1, 3)
    return np.column_stack((v[:, 0], -v[:, 2], v[:, 1])).astype(np.float32)


def _flat_normals(v, t):
    """ONE NORMAL PER FACE, which is what a generator's surfaces are. glTF has no per-face
    attribute, so a flat-shaded mesh is written with its faces UNWELDED -- three vertices each --
    and that is the honest cost of the convention rather than a smoothing nobody asked for."""
    p = v[t.reshape(-1)].reshape(-1, 3, 3)
    n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    run = np.linalg.norm(n, axis=1)
    run[run <= 1e-12] = 1.0
    n = (n / run[:, None]).astype(np.float32)
    return p.reshape(-1, 3), np.repeat(n, 3, axis=0)


def write(path, parts, looks=None, fields=None, flat=True):
    """`parts` is {name: (vertices as (N,3) in ENU metres, triangles as (M,3))}; `looks` maps a
    name to anything with `to_gltf()`; `fields` maps a name to a per-vertex (N,3) weathering
    field. Writes a binary .glb and returns its byte count.

    THE FIELD TRAVELS AS `COLOR_0`, which is glTF's own per-vertex colour and what Blender's
    importer turns into a colour attribute. It was a separate array threaded into a generated
    script beside a mesh file that could not hold it; a format that carries the geometry, the
    material, its parameters AND this is a format the picture can be rebuilt from by anyone."""
    looks = looks or {}
    fields = fields or {}
    blob = bytearray()
    views, accessors, meshes, materials, nodes = [], [], [], [], []

    def _view(data, target):
        while len(blob) % 4:
            blob.append(0)
        at = len(blob)
        blob.extend(data)
        views.append({"buffer": 0, "byteOffset": at, "byteLength": len(data), "target": target})
        return len(views) - 1

    def _accessor(view, kind, count, ctype, lo=None, hi=None):
        got = {"bufferView": view, "componentType": ctype, "count": int(count), "type": kind}
        if lo is not None:
            got["min"], got["max"] = [float(q) for q in lo], [float(q) for q in hi]
        accessors.append(got)
        return len(accessors) - 1

    for name, (verts, tris) in parts.items():
        v = np.asarray(verts, dtype=np.float32).reshape(-1, 3)
        t = np.asarray(tris, dtype=np.uint32).reshape(-1, 3)
        if not len(t):
            continue
        worn = fields.get(name)
        if worn is not None:
            worn = np.asarray(worn, dtype=np.float32).reshape(-1, 3)
            if len(worn) != len(v):
                worn = None
        if flat:
            corner = t.reshape(-1)
            v, normals = _flat_normals(v, t)
            if worn is not None:
                worn = worn[corner]
            t = np.arange(len(v), dtype=np.uint32).reshape(-1, 3)
        else:
            normals = np.zeros_like(v)
        out = to_gltf_axes(v)
        nrm = to_gltf_axes(normals)
        pv = _view(out.tobytes(), ARRAY_BUFFER)
        nv = _view(nrm.tobytes(), ARRAY_BUFFER)
        iv = _view(t.reshape(-1).astype(np.uint32).tobytes(), ELEMENT_ARRAY_BUFFER)
        pa = _accessor(pv, "VEC3", len(out), FLOAT, out.min(axis=0), out.max(axis=0))
        na = _accessor(nv, "VEC3", len(nrm), FLOAT)
        ia = _accessor(iv, "SCALAR", t.size, UINT)
        got = looks.get(name)
        material = None
        if got is not None:
            made = got.to_gltf() if hasattr(got, "to_gltf") else {
                "pbrMetallicRoughness": {"baseColorFactor": list(map(float, tuple(got))) + [1.0],
                                         "metallicFactor": 0.0, "roughnessFactor": 0.85}}
            made = dict(made, name=str(name))
            materials.append(made)
            material = len(materials) - 1
        attributes = {"POSITION": pa, "NORMAL": na}
        if worn is not None:
            # `_WEAR` AND NOT `COLOR_0`. glTF's own vertex colour is what a viewer expects, and
            # Blender quantises it to eight bits on import -- 0.5 came back as 0.503, which over
            # a 20 m height scale is 7.8 cm of banding inside a 0.5 m band. An application
            # attribute keeps its float32 and arrives as a named attribute, which is exactly what
            # the shader already reads. Measured 2026-09-07.
            cv = _view(np.ascontiguousarray(worn, dtype=np.float32).tobytes(), ARRAY_BUFFER)
            attributes["_WEAR"] = _accessor(cv, "VEC3", len(worn), FLOAT)
        primitive = {"attributes": attributes, "indices": ia, "mode": 4}
        if material is not None:
            primitive["material"] = material
        meshes.append({"name": str(name), "primitives": [primitive]})
        nodes.append({"mesh": len(meshes) - 1, "name": str(name)})

    doc = {"asset": {"version": "2.0", "generator": "outshine-lab"},
           "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}],
           "nodes": nodes, "meshes": meshes, "accessors": accessors,
           "bufferViews": views, "buffers": [{"byteLength": len(blob)}]}
    if materials:
        doc["materials"] = materials
        used = sorted({e for m in materials for e in m.get("extensions", {})})
        if used:
            doc["extensionsUsed"] = used

    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * ((4 - len(text) % 4) % 4)
    blob.extend(b"\0" * ((4 - len(blob) % 4) % 4))
    whole = 12 + 8 + len(text) + 8 + len(blob)
    with open(path, "wb") as out:
        out.write(struct.pack("<III", 0x46546C67, 2, whole))
        out.write(struct.pack("<II", len(text), 0x4E4F534A))
        out.write(text)
        out.write(struct.pack("<II", len(blob), 0x004E4942))
        out.write(blob)
    return whole


def check_round_trip(rng=None):
    """A POINT WRITTEN AND READ BACK IS THE SAME POINT. The exporter is the exact inverse of
    `Gltf::InEcef` or the engine loads the lab's reference mirrored, and nothing downstream
    would say so. Negative control: change one sign and this goes red."""
    rng = rng or np.random.default_rng(7)
    v = rng.uniform(-500.0, 500.0, (2048, 3)).astype(np.float32)
    back = from_gltf_axes(to_gltf_axes(v))
    return float(np.abs(back - v).max())

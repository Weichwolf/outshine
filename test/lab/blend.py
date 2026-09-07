"""THE LAB'S BLENDER PATH: the same geometry, lit and shaded the way a picture has to be judged.

`render.py` is flat shading with a z-buffer, and that is right for what it is FOR -- a crack, a
fold or an inverted normal shows in it and nothing hides. It is the wrong instrument for judging
whether a place reads at AAA level, and the comparison said so: the outshine client's own OldTown
has terracotta roofs over cream walls, contact shadows down every street, aerial perspective on the
far hills and a sky with depth, while the lab twin had one brick colour, a flat green ground and no
shadow anywhere (looked at, 2026-09-06). A twin that cannot be compared is not a twin.

So the geometry goes to Blender, which is already the lab's declared tool for the look:

    MATERIALS BY ROLE, never one colour per body -- wall, roof, road, ground, water. A roof is a
        different material from the wall under it and that single split is most of what a town
        reads as
    A REAL SUN, at the place and the hour the client's own table states, casting real shadows
    A PHYSICAL SKY, so the horizon carries the aerial perspective the flat renderer cannot
    THE CLIENT'S OWN CAMERA -- the same coordinate, bearing, pitch, field of view and frame

The mesh is handed over as one PLY per material with per-face groups; Blender is driven headless
by a generated script. Nothing here is imported by the beds: this is a VIEWER, and the geometry it
draws is the geometry the checks already passed.
"""
import math
import os
import pathlib
import subprocess
import tempfile

import numpy as np

BLENDER = os.environ.get("OUTSHINE_BLENDER", "/opt/homebrew/bin/blender")

# THE EXPOSURE IS MEASURED, NOT CHOSEN. An 18 percent card lit flat by this sun must read near
# 118 of 255 through AgX -- that is what a right exposure means and it is checkable, which is why
# the three numbers below carry a measurement rather than a taste. Daylight is roughly 85 percent
# sun and 15 percent sky, so the dome is the ambient and the lamp is the sun.
# MEASURED 2026-09-06 against an 18 percent card, flat to a 72 degree sun:
#   sky 0.25 sun 4.0   card 106/120/137   grey error 3.3   blue cast +31
#   sky 0.12 sun 6.0   card 104/110/119   grey error 7.0   blue cast +16
#   sky 0.06 sun 8.0   card 106/109/113   grey error 9.0   blue cast  +7   <- taken
# The first row exposes right and is BLUE, which is what a dome-dominated scene is; the last is
# daylight, 85 percent sun and 15 percent sky, and a camera at 6500 K renders it neutral. A dark
# dome is not a stylistic choice -- it is what gives a street its contact shadows.
# ...and then the SKY ITSELF came out dark, because dimming the dome dims the picture of it as
# well as its light. The dome is left at its own physical brightness and the SUN is raised to the
# ratio that makes the illuminant daylight; the exposure carries the rest.
#   sky 1.00 sun 130 ev -5.40   card 123/126/130, cast +7.5, sky pixel 150   <- taken
SKY_GAIN = 1.0
SUN_GAIN = 130.0
EXPOSURE = -5.4
BALANCE_K = 6500.0        # [SET] daylight, which is what a camera outdoors is set to

# ROLE -> (base colour, roughness, metallic). The roof and the wall are the split that matters.
LOOKS = {
    "wall":   ((0.82, 0.79, 0.72), 0.85, 0.0),
    "roof":   ((0.55, 0.27, 0.19), 0.75, 0.0),
    "road":   ((0.19, 0.19, 0.20), 0.60, 0.0),
    "ground": ((0.33, 0.38, 0.22), 0.95, 0.0),
    "water":  ((0.10, 0.18, 0.26), 0.08, 0.0),
    "rail":   ((0.28, 0.26, 0.24), 0.55, 0.0),
    "stone":  ((0.78, 0.75, 0.68), 0.80, 0.0),
    "glass":  ((0.055, 0.075, 0.095), 0.06, 0.0),
    "metal":  ((0.38, 0.38, 0.40), 0.35, 0.9),
    "wood":   ((0.86, 0.85, 0.82), 0.65, 0.0),
}


# THE BLUE CHANNEL IS A HEIGHT OVER THE GROUND, DIVIDED BY THIS. A vertex colour is a byte,
# so the range has to be declared: 20 m covers a five-storey block and gives 8 cm of
# resolution, which is a sixth of the splash band it is read for.
HEIGHT_SCALE_M = 20.0


def write_ply(path, verts, tris, field=None):
    """A BINARY PLY, WRITTEN STRAIGHT FROM THE ARRAYS.

    An ASCII PLY of six million triangles is a hundred million `str.format` calls and a file a
    reader has to parse back into floats; the binary one is a header and two `tofile` calls.
    Measured 2026-09-07 on a town twin: the same reason the mesh itself is arrays and not tuples,
    and the same reason the C++ never had this problem -- the cost is the layout.

    `field` is the generator's per-vertex WEATHERING -- (N, 3) in [0, 1] -- and it rides on the
    vertex colour, which is the one channel every mesh format already has."""
    V = np.ascontiguousarray(np.asarray(verts, dtype=np.float32).reshape(-1, 3))
    T = np.ascontiguousarray(np.asarray(tris, dtype=np.int32).reshape(-1, 3))
    rows = [("x", "<f4"), ("y", "<f4"), ("z", "<f4")]
    if field is not None:
        rows += [("red", "u1"), ("green", "u1"), ("blue", "u1")]
    head = ["ply", "format binary_little_endian 1.0", f"element vertex {len(V)}"]
    head += [f"property float {n}" for n in ("x", "y", "z")]
    if field is not None:
        head += [f"property uchar {n}" for n in ("red", "green", "blue")]
    head += [f"element face {len(T)}", "property list uchar int vertex_index", "end_header"]
    body = np.zeros(len(V), dtype=np.dtype(rows))
    body["x"], body["y"], body["z"] = V[:, 0], V[:, 1], V[:, 2]
    if field is not None:
        F = np.clip(np.asarray(field, dtype=np.float32).reshape(-1, 3), 0.0, 1.0)
        for name, k in (("red", 0), ("green", 1), ("blue", 2)):
            body[name] = (F[:, k] * 255.0 + 0.5).astype(np.uint8)
    faces = np.zeros(len(T), dtype=np.dtype([("n", "u1"), ("a", "<i4"), ("b", "<i4"),
                                             ("c", "<i4")]))
    faces["n"] = 3
    faces["a"], faces["b"], faces["c"] = T[:, 0], T[:, 1], T[:, 2]
    with open(path, "wb") as out:
        out.write(("\n".join(head) + "\n").encode())
        body.tofile(out)
        faces.tofile(out)


def render(parts, camera, sun, out_png, samples=64, haze=0.35, engine="CYCLES",
           looks=None, sky_gain=SKY_GAIN, sun_gain=SUN_GAIN, exposure=EXPOSURE,
           balance_k=BALANCE_K, fields=None, grounds=None):
    """`parts` is {role: (vertices, tris)}, `camera` the lab's own, `sun` the ENU direction.

    `looks` overrides the role table with {role: rgb} or {role: (rgb, rough, metal)}, which is how
    a body's own palette -- its epoch's field, its trim, its roof's covering -- reaches the
    picture instead of one colour for every wall on Earth."""
    work = pathlib.Path(tempfile.mkdtemp(prefix="outshine-blend-"))
    # A LOOK IS A glTF MATERIAL, and its fields are glTF 2.0's own: baseColorFactor (linear
    # albedo), metallicFactor, roughnessFactor, ior, emissiveFactor and the alpha mode. Blender's
    # Principled BSDF IS metallic-roughness, so the mapping is a copy field for field and not a
    # translation -- which is the point of speaking the importer's vocabulary in the first place.
    table = {}
    for role, got in dict(LOOKS, **(looks or {})).items():
        if hasattr(got, "to_gltf"):
            m = got.to_gltf()
            pbr = m["pbrMetallicRoughness"]
            table[role] = dict(rgb=tuple(pbr["baseColorFactor"][:3]),
                               alpha=float(pbr["baseColorFactor"][3]),
                               metal=float(pbr["metallicFactor"]),
                               rough=float(pbr["roughnessFactor"]),
                               ior=float(m.get("extensions", {}).get(
                                   "KHR_materials_ior", {}).get("ior", 1.5)),
                               emissive=tuple(m.get("emissiveFactor", (0.0, 0.0, 0.0))),
                               strength=float(m.get("extensions", {}).get(
                                   "KHR_materials_emissive_strength", {}).get(
                                       "emissiveStrength", 1.0)),
                               mode=m.get("alphaMode", "OPAQUE"),
                               cutoff=float(m.get("alphaCutoff", 0.5)),
                               two=bool(m.get("doubleSided", False)),
                               transmission=float(m.get("extensions", {}).get(
                                   "KHR_materials_transmission", {}).get(
                                       "transmissionFactor", 0.0)),
                               **dict({"grain_m": 0.0, "relief_m": 0.0, "mottle": 0.0,
                                       "unit_m": [0.0, 0.0], "joint_m": 0.0, "bond": "",
                                       "splash_m": 0.0},
                                      **m.get("extras", {})))
        elif isinstance(got, tuple) and len(got) == 3 and isinstance(got[0], (tuple, list)):
            table[role] = dict(rgb=tuple(got[0]), alpha=1.0, metal=float(got[2]),
                               rough=float(got[1]), ior=1.5, emissive=(0.0, 0.0, 0.0),
                               strength=1.0, mode="OPAQUE", cutoff=0.5, two=False,
                               transmission=0.0, grain_m=0.0, relief_m=0.0, mottle=0.0,
                               unit_m=[0.0, 0.0], joint_m=0.0, bond="", splash_m=0.0)
        else:
            table[role] = dict(rgb=tuple(got), alpha=1.0, metal=0.0, rough=0.85, ior=1.5,
                               emissive=(0.0, 0.0, 0.0), strength=1.0, mode="OPAQUE",
                               cutoff=0.5, two=False, transmission=0.0,
                               grain_m=0.0, relief_m=0.0, mottle=0.0,
                               unit_m=[0.0, 0.0], joint_m=0.0, bond="", splash_m=0.0)
    # PARTS THAT LOOK THE SAME BECOME ONE MESH. A town twin hands over seven thousand of them --
    # a role per building per surface -- and Blender imports a PLY with one operator call each,
    # each of which updates the scene: measured 2026-09-07, fifteen minutes before a ray was
    # traced. Grouping by the LOOK is what an engine's draw call batching is, and it is only
    # possible because nothing per-PART reaches the material any more: the height over the
    # ground rides on the vertex colour instead.
    # AND THE BATCH IS BUILT IN ARRAYS. Concatenating a list of tuples per part costs twelve
    # times the memory of the arrays it ends up as and every one of those tuples is an object
    # the allocator has to make; one `concatenate` per group at the end costs one copy.
    fields = fields or {}
    group = {}
    for role, (verts, tris) in parts.items():
        if role not in table:
            # A FAILURE IS LOUD. This `continue` dropped EVERY part of every place for a day:
            # `Parts.of` names a part `<role>_<index>` while the looks were collected under the
            # caller's role, so nothing matched, no mesh file was written, and Blender rendered
            # an empty sky in 3.3 seconds with every geometric check green (2026-09-07).
            raise KeyError(f"no look for part {role!r}; the picture would silently lose it. "
                           f"Known: {sorted(table)[:8]}...")
        v = np.asarray(verts, dtype=np.float32).reshape(-1, 3)
        t = np.asarray(tris, dtype=np.int32).reshape(-1, 3)
        if not len(t):
            continue
        look = table[role]
        key = tuple(sorted((k, tuple(v_) if isinstance(v_, (list, tuple)) else v_)
                           for k, v_ in look.items()))
        got = group.setdefault(key, {"name": role.split(".")[0], "v": [], "t": [], "f": [],
                                     "look": look, "worn": False, "at": 0})
        got["v"].append(v)
        got["t"].append(t + got["at"])
        got["at"] += len(v)
        held = fields.get(role)
        if held is not None:
            got["worn"] = True
            got["f"].append(np.asarray(held, dtype=np.float32).reshape(-1, 3))
        else:
            got["f"].append(np.zeros((len(v), 3), dtype=np.float32))
    table = {}
    worn = {}
    files = {}
    for at, got in enumerate(group.values()):
        role = f"{got['name']}_{at}"
        table[role] = got["look"]
        worn[role] = got["worn"]
        files[role] = str(work / f"{role}.ply")
        write_ply(files[role], np.concatenate(got["v"]), np.concatenate(got["t"]),
                  np.concatenate(got["f"]) if got["worn"] else None)
    sun = np.asarray(sun, dtype=float)
    sun = sun / max(float(np.linalg.norm(sun)), 1e-9)
    elev = math.degrees(math.asin(max(-1.0, min(1.0, float(sun[2])))))
    azim = math.degrees(math.atan2(float(sun[0]), float(sun[1])))     # from north, clockwise
    script = work / "shot.py"
    script.write_text(_SCRIPT.format(
        files=repr(files), looks=repr({k: table[k] for k in files}),
        worn=repr({k: bool(worn.get(k)) for k in files}),
        width=camera.width, height=camera.height, fov=camera.fov_deg,
        bearing=camera.bearing_deg, pitch=camera.pitch_deg, agl=camera.agl_m,
        ortho=bool(camera.orthographic), ymag=camera.y_mag_m,
        elev=elev, azim=azim, samples=int(samples), haze=float(haze),
        sky_gain=float(sky_gain), sun_gain=float(sun_gain), exposure=float(exposure),
        balance_k=float(balance_k),
        height_scale=HEIGHT_SCALE_M,
        out=repr(str(out_png)), engine=repr(engine)))
    done = subprocess.run([BLENDER, "-b", "--factory-startup", "-P", str(script)],
                          capture_output=True, text=True, timeout=3600)
    # FRESHNESS AND NOT EXISTENCE. A render that fails leaves the PREVIOUS picture on disk, and
    # a check that only asks whether the file is there hands it back: three material changes in a
    # row were judged on one stale image before the file was deleted by hand and the renderer
    # turned out to have been failing all along (2026-09-07). The picture has to be NEWER than
    # the script that asked for it.
    got = pathlib.Path(out_png)
    if not got.exists() or got.stat().st_mtime < script.stat().st_mtime:
        raise RuntimeError(f"blender wrote no picture:\n{done.stdout[-2000:]}\n{done.stderr[-1000:]}")
    return out_png


_SCRIPT = r'''
import bpy, math, mathutils
HEIGHT_SCALE_M = {height_scale}

def _bond(nt, vec, cell, joint, stagger):
    """A COURSED BOND IN METRES, and not Blender's Brick node.

    That node's Mortar Size is a fraction of the CELL, so one number gives a bed joint and a
    perpend in the ratio of the unit's own aspect: on a 240 x 71.5 brick a 12.5 mm bed joint
    came out as a 38 mm perpend (rendered and looked at, 2026-09-06). The joint is a DIMENSION,
    the same one in both directions, so it is built here from the dimension.

    Returns (fac, unit) -- 1.0 in the joint, and a per-unit random for the mottle."""
    def math(op, a=None, b=None):
        n = nt.nodes.new("ShaderNodeMath")
        n.operation = op
        for k, v in ((0, a), (1, b)):
            if v is None:
                continue
            if hasattr(v, "is_linked") or hasattr(v, "links"):
                nt.links.new(v, n.inputs[k])
            else:
                n.inputs[k].default_value = v
        return n.outputs[0]

    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(vec, sep.inputs["Vector"])
    cx, cy = cell
    vy = math("DIVIDE", sep.outputs["Y"], cy)
    row = math("FLOOR", vy)
    fv = math("MULTIPLY", math("SUBTRACT", vy, row), cy)
    shift = math("MULTIPLY", math("FRACT", math("MULTIPLY", row, 0.5)), stagger)
    ux = math("ADD", math("DIVIDE", sep.outputs["X"], cx), shift)
    col = math("FLOOR", ux)
    fu = math("MULTIPLY", math("SUBTRACT", ux, col), cx)
    soft = max(joint * 0.30, 1e-4)
    edges = []
    for got in (fu, fv):
        rng = nt.nodes.new("ShaderNodeMapRange")
        rng.clamp = True
        nt.links.new(got, rng.inputs["Value"])
        rng.inputs["From Min"].default_value = joint
        rng.inputs["From Max"].default_value = joint + soft
        rng.inputs["To Min"].default_value = 0.0
        rng.inputs["To Max"].default_value = 1.0
        edges.append(rng.outputs["Result"])
    unitness = math("MULTIPLY", edges[0], edges[1])
    fac = math("SUBTRACT", 1.0, unitness)
    id_vec = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(col, id_vec.inputs["X"])
    nt.links.new(row, id_vec.inputs["Y"])
    rand = nt.nodes.new("ShaderNodeTexWhiteNoise")
    rand.noise_dimensions = "3D"
    nt.links.new(id_vec.outputs["Vector"], rand.inputs["Vector"])
    return fac, rand.outputs["Value"]


def _surface_uv(nt, coord):
    """THE SURFACE'S OWN FRAME, built from the geometry NORMAL, and this is what a laid material
    needs rather than a box projection.

    Triplanar puts three copies of the pattern on the three axis planes and blends them by the
    normal -- which is right on a wall and WRONG on a roof: at 45 degrees two of the weights are
    exactly equal, the two patterns interfere, and the surface reads as crumpled paper however
    sharp the blend (rendered Rothenburg's roofs and looked at, 2026-09-07; the geometry was
    measured correct to a median of 0.000 m first, which is what left the material as the only
    place it could be).

    A tangent frame is one evaluation instead of three, and it is the frame a TILER uses: the
    tangent runs along the eaves (horizontal, across the fall line) and the bitangent up the
    slope, so courses lie along the eaves on any pitch and horizontally on any wall. Where the
    normal is vertical the frame degenerates and the plane IS the ground plane, which is what a
    pavement wants anyway."""
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    n = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Normal"], n.inputs["Vector"])
    pos = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(coord.outputs["Object"], pos.inputs["Vector"])

    def math(op, a=None, b=None):
        m = nt.nodes.new("ShaderNodeMath")
        m.operation = op
        for k, v in ((0, a), (1, b)):
            if v is None:
                continue
            if hasattr(v, "is_linked"):
                nt.links.new(v, m.inputs[k])
            else:
                m.inputs[k].default_value = v
        return m.outputs[0]

    # t = normalize(cross(N, z)) = normalize((Ny, -Nx, 0)); b = cross(N, t)
    tan = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(n.outputs["Y"], tan.inputs["X"])
    nt.links.new(math("MULTIPLY", n.outputs["X"], -1.0), tan.inputs["Y"])
    flat = nt.nodes.new("ShaderNodeMath")
    flat.operation = "LESS_THAN"
    nt.links.new(math("ABSOLUTE", n.outputs["Z"]), flat.inputs[0])
    flat.inputs[1].default_value = 0.999
    # where the normal is vertical the tangent collapses, so fall back to +x
    pick = nt.nodes.new("ShaderNodeMix")
    pick.data_type = "VECTOR"
    nt.links.new(flat.outputs[0], pick.inputs[0])
    pick.inputs[4].default_value = (1.0, 0.0, 0.0)
    nt.links.new(tan.outputs["Vector"], pick.inputs[5])
    t = nt.nodes.new("ShaderNodeVectorMath")
    t.operation = "NORMALIZE"
    nt.links.new(pick.outputs[1], t.inputs[0])
    bit = nt.nodes.new("ShaderNodeVectorMath")
    bit.operation = "CROSS_PRODUCT"
    nt.links.new(geo.outputs["Normal"], bit.inputs[0])
    nt.links.new(t.outputs["Vector"], bit.inputs[1])
    du = nt.nodes.new("ShaderNodeVectorMath")
    du.operation = "DOT_PRODUCT"
    nt.links.new(coord.outputs["Object"], du.inputs[0])
    nt.links.new(t.outputs["Vector"], du.inputs[1])
    dv = nt.nodes.new("ShaderNodeVectorMath")
    dv.operation = "DOT_PRODUCT"
    nt.links.new(coord.outputs["Object"], dv.inputs[0])
    nt.links.new(bit.outputs["Vector"], dv.inputs[1])
    out = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(du.outputs["Value"], out.inputs["X"])
    nt.links.new(dv.outputs["Value"], out.inputs["Y"])
    return out.outputs["Vector"]


files = {files}
looks = {looks}
worn = {worn}
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

# ONE MATERIAL PER LOOK, not per PART.
_made = {{}}


def _material(role, ob):
    look = looks[role]
    key = (tuple(sorted((k, tuple(v) if isinstance(v, (list, tuple)) else v)
                        for k, v in look.items())), bool(worn.get(role)))
    got = _made.get(key)
    if got is not None:
        return got, False
    mat = bpy.data.materials.new(role)
    _made[key] = mat
    return mat, True


for role, path in files.items():
    try:
        bpy.ops.wm.ply_import(filepath=path, import_colors="LINEAR")
    except TypeError:
        bpy.ops.wm.ply_import(filepath=path)
    ob = bpy.context.selected_objects[0]
    ob.name = role
    look = looks[role]
    mat, fresh = _material(role, ob)
    if not fresh:
        ob.data.materials.append(mat)
        ob.data.shade_flat()
        continue
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    rgb = look["rgb"]
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], look["alpha"])
    bsdf.inputs["Roughness"].default_value = look["rough"]
    bsdf.inputs["Metallic"].default_value = look["metal"]
    for named, value in (("IOR", look["ior"]), ("Alpha", look["alpha"]),
                         ("Transmission Weight", look["transmission"]),
                         ("Emission Strength", look["strength"])):
        if named in bsdf.inputs:
            bsdf.inputs[named].default_value = value
    if "Emission Color" in bsdf.inputs:
        e = look["emissive"]
        bsdf.inputs["Emission Color"].default_value = (e[0], e[1], e[2], 1.0)
    # THE SURFACE'S OWN DIMENSIONS, from the material's `extras`: a grain in METRES, a relief
    # depth in METRES and an albedo mottle as a fraction. Nothing here is a texture and nothing
    # is decoration -- a viewer at 2 m resolves 0.5 mm, so an 11 mm asphalt aggregate standing
    # 1.5 mm proud is the whole difference between a road and grey plastic in raking light.
    unit = look.get("unit_m", [0.0, 0.0])
    if look.get("bond", "") and unit[0] > 0.0 and unit[1] > 0.0:
        # A LAID SURFACE, in its own bond. Blender's Brick node IS this pattern: a cell of
        # (brick width x row height) with a mortar band and a per-unit colour, and every number
        # below is the material's own dimension divided by that cell.
        nt = mat.node_tree
        coord = nt.nodes.new("ShaderNodeTexCoord")
        cell = (unit[0] + look["joint_m"], unit[1] + look["joint_m"])
        mo = look.get("mottle", 0.0)
        lo = (rgb[0] * (1 - mo), rgb[1] * (1 - mo), rgb[2] * (1 - mo), 1.0)
        hi = (min(1.0, rgb[0] * (1 + mo)), min(1.0, rgb[1] * (1 + mo)),
              min(1.0, rgb[2] * (1 + mo)), 1.0)
        # the joint is LIME, and it is lighter than almost anything laid in it
        joint_rgb = (min(1.0, rgb[0] * 1.8 + 0.10), min(1.0, rgb[1] * 1.8 + 0.10),
                     min(1.0, rgb[2] * 1.8 + 0.10), 1.0)

        stagger = 0.5 if look["bond"] == "stretcher" else 0.0

        def one(vec):
            fac, unit = _bond(nt, vec, cell, look["joint_m"], stagger)
            shade = nt.nodes.new("ShaderNodeMix")
            shade.data_type = "RGBA"
            shade.inputs[6].default_value = lo
            shade.inputs[7].default_value = hi
            nt.links.new(unit, shade.inputs["Factor"])
            laid = nt.nodes.new("ShaderNodeMix")
            laid.data_type = "RGBA"
            nt.links.new(fac, laid.inputs["Factor"])
            nt.links.new(shade.outputs[2], laid.inputs[6])
            laid.inputs[7].default_value = joint_rgb
            return laid.outputs[2], fac

        colour, fac = one(_surface_uv(nt, coord))
        nt.links.new(colour, bsdf.inputs["Base Color"])
        if look.get("relief_m", 0.0) > 0.0:
            # the joint is RAKED: the mortar sits back, so height is 1 minus the mortar factor
            inv = nt.nodes.new("ShaderNodeMath")
            inv.operation = "SUBTRACT"
            inv.inputs[0].default_value = 1.0
            nt.links.new(fac, inv.inputs[1])
            bump = nt.nodes.new("ShaderNodeBump")
            bump.inputs["Distance"].default_value = look["relief_m"]
            bump.inputs["Strength"].default_value = 1.0
            nt.links.new(inv.outputs[0], bump.inputs["Height"])
            nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    elif look.get("grain_m", 0.0) > 0.0 or look.get("mottle", 0.0) > 0.0:
        nt = mat.node_tree
        grain = look.get("grain_m", 0.0) or 0.05
        coord = nt.nodes.new("ShaderNodeTexCoord")
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 1.0 / grain
        noise.inputs["Detail"].default_value = 6.0
        noise.inputs["Roughness"].default_value = 0.6
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        if look.get("relief_m", 0.0) > 0.0:
            bump = nt.nodes.new("ShaderNodeBump")
            # Blender's Bump takes a DISTANCE in scene units, which is metres here
            bump.inputs["Distance"].default_value = look["relief_m"]
            bump.inputs["Strength"].default_value = 1.0
            nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
            nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        if look.get("mottle", 0.0) > 0.0:
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.inputs["Factor"].default_value = 1.0
            lo = [c * (1.0 - look["mottle"]) for c in rgb]
            hi = [min(1.0, c * (1.0 + look["mottle"])) for c in rgb]
            mix.inputs[6].default_value = (lo[0], lo[1], lo[2], look["alpha"])
            mix.inputs[7].default_value = (hi[0], hi[1], hi[2], look["alpha"])
            # the mottle is coarser than the grain: a brick differs from its neighbour, not a
            # grain of sand from the next
            wide = nt.nodes.new("ShaderNodeTexNoise")
            wide.inputs["Scale"].default_value = 1.0 / max(grain * 3.0, 0.02)
            wide.inputs["Detail"].default_value = 3.0
            nt.links.new(coord.outputs["Object"], wide.inputs["Vector"])
            nt.links.new(wide.outputs["Fac"], mix.inputs["Factor"])
            nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    # THE SPLASH BAND. Rain bounces 0.5 m off a paved surface and no higher, and what a wall
    # needs to know is its own HEIGHT OVER THE GROUND. That number is carried per VERTEX in the
    # blue channel -- linear in z, so interpolation across a fifteen-metre wall quad is EXACT --
    # and the material does the clamping, which is the non-linear half a vertex cannot hold.
    # It was a per-material constant first, and that is what stopped seven thousand parts from
    # ever being batched into one mesh.
    if look.get("splash_m", 0.0) > 0.0 and worn.get(role):
        nt = mat.node_tree
        col = nt.nodes.new("ShaderNodeAttribute")
        names = [a.name for a in getattr(ob.data, "color_attributes", [])]
        col.attribute_name = names[0] if names else "Col"
        chan = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(col.outputs["Color"], chan.inputs["Color"])
        band = nt.nodes.new("ShaderNodeMapRange")
        band.clamp = True
        nt.links.new(chan.outputs["Blue"], band.inputs["Value"])
        band.inputs["From Min"].default_value = 0.0
        band.inputs["From Max"].default_value = look["splash_m"] / HEIGHT_SCALE_M
        band.inputs["To Min"].default_value = 1.0
        band.inputs["To Max"].default_value = 0.0
        wet = nt.nodes.new("ShaderNodeMix")
        wet.data_type = "RGBA"
        base = bsdf.inputs["Base Color"]
        if base.links:
            nt.links.new(base.links[0].from_socket, wet.inputs[6])
        else:
            wet.inputs[6].default_value = (rgb[0], rgb[1], rgb[2], look["alpha"])
        # what stands in the splash is darker and greener: a plinth grows what a wall never does
        wet.inputs[7].default_value = (rgb[0] * 0.50, rgb[1] * 0.56, rgb[2] * 0.44, look["alpha"])
        nt.links.new(band.outputs["Result"], wet.inputs["Factor"])
        nt.links.new(wet.outputs[2], bsdf.inputs["Base Color"])

    # THE WEATHERING FIELD, if the generator handed one over: polish, silt, splash on the vertex
    # colour. Nothing here is a pattern -- a tyre polished the red channel, water left the green
    # one and rain bounced into the blue -- and the material only has to READ them.
    if worn.get(role) and look.get("splash_m", 0.0) <= 0.0:
        nt = mat.node_tree
        col = nt.nodes.new("ShaderNodeAttribute")
        # THE ATTRIBUTE HAS TO BE NAMED. Left to the default the node reads BLACK, and a field
        # that is right in the file and right in the mesh shows as nothing at all -- which is
        # exactly what a facade with 444 field vertices looked like (2026-09-06). PLY's colours
        # land as one POINT attribute and `ShaderNodeAttribute` is what reads a named one.
        names = [a.name for a in getattr(ob.data, "color_attributes", [])]
        col.attribute_name = names[0] if names else "Col"
        chan = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(col.outputs["Color"], chan.inputs["Color"])
        base = bsdf.inputs["Base Color"]
        got = base.links[0].from_socket if base.links else None
        step = nt.nodes.new("ShaderNodeMix")
        step.data_type = "RGBA"
        if got is not None:
            nt.links.new(got, step.inputs[6])
        else:
            step.inputs[6].default_value = (rgb[0], rgb[1], rgb[2], look["alpha"])
        # SILT IS DARK AND ROUGH and it is the strongest of the three: a gutter reads at a
        # glance, a polished wheel path only when the light is low
        step.inputs[7].default_value = (rgb[0] * 0.55, rgb[1] * 0.55, rgb[2] * 0.52, look["alpha"])
        silt = nt.nodes.new("ShaderNodeMath")
        silt.operation = "MULTIPLY"
        nt.links.new(chan.outputs["Green"], silt.inputs[0])
        silt.inputs[1].default_value = 0.85
        nt.links.new(silt.outputs[0], step.inputs["Factor"])
        wet = nt.nodes.new("ShaderNodeMix")
        wet.data_type = "RGBA"
        nt.links.new(step.outputs[2], wet.inputs[6])
        # SPLASH is the darkest and greenest: a plinth grows what a wall never does
        wet.inputs[7].default_value = (rgb[0] * 0.50, rgb[1] * 0.56, rgb[2] * 0.44, look["alpha"])
        nt.links.new(chan.outputs["Blue"], wet.inputs["Factor"])
        nt.links.new(wet.outputs[2], bsdf.inputs["Base Color"])
        # POLISH takes the roughness DOWN: what a tyre does to aggregate is make it smooth
        rough = nt.nodes.new("ShaderNodeMapRange")
        rough.clamp = True
        nt.links.new(chan.outputs["Red"], rough.inputs["Value"])
        rough.inputs["To Min"].default_value = look["rough"]
        rough.inputs["To Max"].default_value = max(0.30, look["rough"] - 0.30)
        nt.links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    mat.use_backface_culling = not look["two"]
    if look["mode"] == "MASK":
        # A LEAF CARD IS A QUAD WITH A HOLE IN IT, and without the hole it is a quad. The engine
        # will carry a leaf texture; the lab generates the mask so the SILHOUETTE is right here
        # too -- a radial falloff with noise, which is a leaf spray's own outline.
        nt = mat.node_tree
        coord = nt.nodes.new("ShaderNodeTexCoord")
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 9.0
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.42
        ramp.color_ramp.elements[1].position = 0.56
        nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
        nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Alpha"])
    if look["mode"] == "MASK":
        mat.blend_method = "CLIP" if hasattr(mat, "blend_method") else mat.blend_method
    ob.data.materials.append(mat)
    ob.data.shade_flat()

# THE SUN, where the place and the hour put it. Blender's sun points DOWN its -Z by default, so
# the rotation carries the altitude and the azimuth measured from north, clockwise.
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", type="SUN"))
bpy.context.collection.objects.link(sun)
# BLENDER'S SUN STRENGTH IS IRRADIANCE, and 1.0 is already a clear day. At 2.4 with AgX every
# wall came back paper-white and a palette of ochre, cream and grey-green was indistinguishable
# (rendered a row of four and looked at, 2026-09-06) -- which makes every material judgement
# impossible, so it is not a taste setting but a correctness one.
sun.data.energy = {sun_gain}
sun.data.angle = math.radians(0.53)          # the sun's own disc, which is what softens a shadow
sun.rotation_euler = (math.radians(90.0 - {elev}), 0.0, math.radians(-{azim}))

world = bpy.data.worlds.new("sky")
bpy.context.scene.world = world
world.use_nodes = True
nt = world.node_tree
nt.nodes.clear()
sky = nt.nodes.new("ShaderNodeTexSky")
# Blender 5's sky node calls the physical model MULTIPLE_SCATTERING; "NISHITA" was
# its 3.x name and is gone (measured 2026-09-06 against the enum the build actually offers).
sky.sky_type = "MULTIPLE_SCATTERING"
# THE SKY CARRIES ITS OWN SUN DISC, and a sun lamp beside it lights the scene TWICE. Every
# wall came back paper-white at any lamp strength and any exposure, because the dome alone was
# already a full daylight sun (rendered a row of four twice and looked at, 2026-09-06). The dome
# provides the ambient, the lamp provides the sun and its shadows, and neither doubles the other.
sky.sun_disc = False
sky.sun_elevation = math.radians({elev})
sky.sun_rotation = math.radians(-{azim})
sky.altitude = 200.0
sky.air_density = 1.0
sky.aerosol_density = {haze} * 4.0        # Blender 5 calls the haze `aerosol_density`
sky.ozone_density = 1.0
sky.ground_albedo = 0.22
bg = nt.nodes.new("ShaderNodeBackground")
bg.inputs["Strength"].default_value = {sky_gain}
out = nt.nodes.new("ShaderNodeOutputWorld")
nt.links.new(sky.outputs[0], bg.inputs[0])
nt.links.new(bg.outputs[0], out.inputs[0])

cam_data = bpy.data.cameras.new("cam")
if {ortho}:
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 2.0 * {ymag} * max(1.0, {width} / {height})
else:
    cam_data.lens_unit = "FOV"
    cam_data.angle = math.radians({fov})
cam_data.clip_start = 0.5
cam_data.clip_end = 40000.0
cam = bpy.data.objects.new("cam", cam_data)
bpy.context.collection.objects.link(cam)
cam.location = (0.0, 0.0, {agl})
# the lab frame is ENU: x east, y north, z up. Blender's camera looks down its own -Z, so a
# bearing measured clockwise from north is a rotation about Z, and the pitch tilts about X.
cam.rotation_euler = (math.radians(90.0 + {pitch}), 0.0, math.radians(-{bearing}))
bpy.context.scene.camera = cam

sc = bpy.context.scene
sc.render.engine = {engine}
# THE GPU IS THERE AND CYCLES DEFAULTS TO THE CPU. `compute_device_type` was already METAL and the
# Apple A18 Pro's five GPU cores were already enabled in the preferences, but `scene.cycles.device`
# is CPU unless it is asked -- so every render up to 2026-09-06 was traced on the cores this
# engine's frame budget is measured against, which is a waste of the one thing the machine has.
if {engine} == "CYCLES":
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for dev in prefs.devices:
            dev.use = dev.type != "CPU"
        sc.cycles.device = "GPU"
    except Exception:
        sc.cycles.device = "CPU"
sc.render.resolution_x = {width}
sc.render.resolution_y = {height}
sc.render.resolution_percentage = 100
sc.render.film_transparent = False
sc.render.filepath = {out}
sc.render.image_settings.file_format = "PNG"
if {engine} == "CYCLES":
    sc.cycles.samples = {samples}
    # THE SAME DECLARATION RENDERS THE SAME BYTES, TWICE. Cycles seeds its sampler from the frame
    # number by default and every pixel then differs a little between two runs of one scene: the
    # comparison that proves a culler loses nothing read 2.1 % of pixels moved with a worst
    # channel of 5, and a proof whose noise floor has to be argued about is not a proof. These
    # renders are also the reference the C++ will be measured against, and a reference that
    # wanders is worth nothing.
    sc.cycles.seed = 0
    sc.cycles.use_animated_seed = False
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 4
sc.view_settings.view_transform = "AgX"
# AgX ON A SUNLIT WHITE WALL BLOWS OUT. The first render came back near paper-white with the
# relief only just legible (looked at 2026-09-06); a stop and a half down puts the wall where a
# camera at f/8 would put it and the shadows carry the modelling again.
sc.view_settings.exposure = {exposure}
# AND THE CAMERA IS WHITE BALANCED, which is what a camera does. Physical daylight is a white sun
# under a blue dome, so an unbalanced render puts a +31 blue cast on an 18 percent card (measured
# 2026-09-06 against the card). A film camera carries a daylight balance and so does this one.
sc.view_settings.use_white_balance = True
sc.view_settings.white_balance_temperature = {balance_k}
sc.view_settings.look = "AgX - Base Contrast"
bpy.ops.render.render(write_still=True)
'''

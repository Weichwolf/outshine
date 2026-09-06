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


def write_ply(path, verts, tris):
    """Binary would be faster; this is a lab and a text PLY is one thing less to get wrong."""
    with open(path, "w") as out:
        out.write("ply\nformat ascii 1.0\n")
        out.write(f"element vertex {len(verts)}\nproperty float x\nproperty float y\nproperty float z\n")
        out.write(f"element face {len(tris)}\nproperty list uchar int vertex_index\nend_header\n")
        for v in verts:
            out.write(f"{v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
        for t in tris:
            out.write(f"3 {t[0]} {t[1]} {t[2]}\n")


def render(parts, camera, sun, out_png, samples=64, haze=0.35, engine="CYCLES"):
    """`parts` is {role: (vertices, tris)}, `camera` the lab's own, `sun` the ENU direction."""
    work = pathlib.Path(tempfile.mkdtemp(prefix="outshine-blend-"))
    files = {}
    for role, (verts, tris) in parts.items():
        if not tris:
            continue
        files[role] = str(work / f"{role}.ply")
        write_ply(files[role], verts, tris)
    sun = np.asarray(sun, dtype=float)
    sun = sun / max(float(np.linalg.norm(sun)), 1e-9)
    elev = math.degrees(math.asin(max(-1.0, min(1.0, float(sun[2])))))
    azim = math.degrees(math.atan2(float(sun[0]), float(sun[1])))     # from north, clockwise
    script = work / "shot.py"
    script.write_text(_SCRIPT.format(
        files=repr(files), looks=repr({k: LOOKS[k] for k in files}),
        width=camera.width, height=camera.height, fov=camera.fov_deg,
        bearing=camera.bearing_deg, pitch=camera.pitch_deg, agl=camera.agl_m,
        ortho=bool(camera.orthographic), ymag=camera.y_mag_m,
        elev=elev, azim=azim, samples=int(samples), haze=float(haze),
        out=repr(str(out_png)), engine=repr(engine)))
    done = subprocess.run([BLENDER, "-b", "--factory-startup", "-P", str(script)],
                          capture_output=True, text=True, timeout=3600)
    if not pathlib.Path(out_png).exists():
        raise RuntimeError(f"blender wrote no picture:\n{done.stdout[-2000:]}\n{done.stderr[-1000:]}")
    return out_png


_SCRIPT = r'''
import bpy, math, mathutils
files = {files}
looks = {looks}
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

for role, path in files.items():
    bpy.ops.wm.ply_import(filepath=path)
    ob = bpy.context.selected_objects[0]
    ob.name = role
    rgb, rough, metal = looks[role]
    mat = bpy.data.materials.new(role)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    ob.data.materials.append(mat)
    ob.data.shade_flat()

# THE SUN, where the place and the hour put it. Blender's sun points DOWN its -Z by default, so
# the rotation carries the altitude and the azimuth measured from north, clockwise.
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", type="SUN"))
bpy.context.collection.objects.link(sun)
sun.data.energy = 2.4
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
sky.sun_elevation = math.radians({elev})
sky.sun_rotation = math.radians(-{azim})
sky.altitude = 200.0
sky.air_density = 1.0
sky.aerosol_density = {haze} * 4.0        # Blender 5 calls the haze `aerosol_density`
sky.ozone_density = 1.0
sky.ground_albedo = 0.22
bg = nt.nodes.new("ShaderNodeBackground")
bg.inputs["Strength"].default_value = 1.0
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
sc.render.resolution_x = {width}
sc.render.resolution_y = {height}
sc.render.resolution_percentage = 100
sc.render.film_transparent = False
sc.render.filepath = {out}
sc.render.image_settings.file_format = "PNG"
if {engine} == "CYCLES":
    sc.cycles.samples = {samples}
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 4
sc.view_settings.view_transform = "AgX"
# AgX ON A SUNLIT WHITE WALL BLOWS OUT. The first render came back near paper-white with the
# relief only just legible (looked at 2026-09-06); a stop and a half down puts the wall where a
# camera at f/8 would put it and the shadows carry the modelling again.
sc.view_settings.exposure = -1.6
sc.view_settings.look = "AgX - Base Contrast"
bpy.ops.render.render(write_still=True)
'''

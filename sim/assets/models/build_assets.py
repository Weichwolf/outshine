#!/usr/bin/env python3
"""FlightBox — der Asset-Baecker. Headless Blender, ein Skript, ein .glb je Eintrag.

    /Applications/Blender.app/Contents/MacOS/Blender --background --python build_assets.py -- --out DIR

WARUM PARAMETRISCH UND NICHT MODELLIERT. Ein von Hand modelliertes Netz ist eine Zahl ohne Herkunft:
niemand kann spaeter sagen, warum eine Spannweite 9,45 m ist. Hier baut jede Form aus BENANNTEN Massen,
und die Masse stehen mit ihrer Quelle daneben. Wer eine Zahl aendern will, aendert sie an einer Stelle
und der Beleg steht dabei.

WARUM glTF. Khronos' GL Transmission Format ist das Laufzeitformat zu OpenGL/WebGPU: Dreiecke,
Normalen, Material, ein Binaerblock — und Blender exportiert es ohne Zusatzpaket. `.glb` ist die
Ein-Datei-Form, also genau ein Artefakt je Asset und nichts daneben zu verlieren.

KOORDINATEN. glTF ist +Y oben, -Z vorwaerts. Der Exporter dreht Blenders +Z-oben selbst; hier wird
deshalb in BLENDER-Achsen gebaut (+X rechts, +Y vorwaerts, +Z oben) und der Export macht den Rest.
Der Nullpunkt jedes Modells ist sein SCHWERPUNKT-NAHER Bezugspunkt, nicht die Nasenspitze: der Renderer
setzt eine Pose, und eine Pose ist eine Lage des Koerpers, nicht seiner Spitze.
"""
import argparse
import math
import os
import sys

import bpy
import bmesh

kMToBlender = 1.0   # eine Blender-Einheit ist ein Meter, damit keine zweite Skala entsteht


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def mat(name, rgb, rough=0.55, metal=0.0, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit:
        b.inputs["Emission Color"].default_value = (*rgb, 1.0)
        b.inputs["Emission Strength"].default_value = emit
    return m


def cyl(name, r, length, y0, verts=16, material=None):
    """Zylinder entlang +Y (Flugrichtung), Basis bei y0."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=length,
                                        location=(0, y0 + 0.5 * length, 0),
                                        rotation=(math.radians(90), 0, 0))
    o = bpy.context.object
    o.name = name
    if material:
        o.data.materials.append(material)
    return o


def cone(name, r, length, y0, verts=16, material=None, r2=0.0):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r, radius2=r2, depth=length,
                                    location=(0, y0 + 0.5 * length, 0),
                                    rotation=(math.radians(-90), 0, 0))
    o = bpy.context.object
    o.name = name
    if material:
        o.data.materials.append(material)
    return o


def fin(name, root, tip, span, sweep, y0, thick, material=None, roll_deg=0.0):
    """Eine Flosse in der XY-Ebene, um +Y gerollt. root/tip = Tiefe an Wurzel und Spitze."""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    v = [bm.verts.new(p) for p in [
        (0.0, y0, 0.0), (0.0, y0 + root, 0.0),
        (span, y0 + sweep + tip, 0.0), (span, y0 + sweep, 0.0)]]
    bm.faces.new(v)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    mod = o.modifiers.new("solid", 'SOLIDIFY')
    mod.thickness = thick
    mod.offset = 0.0
    o.rotation_euler = (0.0, math.radians(roll_deg), 0.0)
    if material:
        o.data.materials.append(material)
    return o


def join(name, objs):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    j = bpy.context.object
    j.name = name
    bpy.ops.object.shade_smooth()
    return j


def export(path, name):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False,
                              export_yup=True, export_apply=True)
    tris = sum(len(o.data.loop_triangles) for o in bpy.data.objects
               if o.type == 'MESH' and (o.data.calc_loop_triangles() or True))
    print("ASSET %-14s %-52s %7d Byte  %5d Dreiecke" % (name, path, os.path.getsize(path), tris))


# ---------------------------------------------------------------- die Leiter, einfach zuerst

def build_aim9(out):
    """AIM-9L/M Sidewinder. Masse [DOC doc/weapons.md / kStoreCatalogue]: Laenge 2,85 m, Rumpf 0,127 m,
    Spannweite ueber die Steuerflossen 0,63 m. Die vier Kanardflossen vorn und vier Rollerons hinten
    sind die Form, an der man diese Rakete erkennt, also sind sie drin."""
    clear()
    body_r, L = 0.0635, 2.85
    grey = mat("aim9_body", (0.78, 0.78, 0.80), rough=0.35, metal=0.6)
    dark = mat("aim9_seeker", (0.10, 0.10, 0.12), rough=0.25)
    parts = [cyl("body", body_r, L - 0.30, -0.5 * L, 24, grey),
             cone("seeker", body_r, 0.30, 0.5 * L - 0.30, 24, dark, r2=0.018)]
    for i in range(4):
        parts.append(fin("canard%d" % i, 0.22, 0.14, 0.315 - body_r, 0.06,
                         0.5 * L - 0.78, 0.010, grey, roll_deg=90 * i))
        parts.append(fin("roll%d" % i, 0.30, 0.16, 0.315 - body_r, 0.10,
                         -0.5 * L + 0.06, 0.012, grey, roll_deg=90 * i + 45))
    join("aim9", parts)
    export(os.path.join(out, "aim9.glb"), "aim9")


def build_mk82(out):
    """Mk-82, 500 lb. Masse [DOC]: Laenge 2,22 m, Durchmesser 0,273 m, vier Leitwerksflossen."""
    clear()
    r, L = 0.1365, 2.22
    olive = mat("mk82_body", (0.28, 0.30, 0.22), rough=0.75)
    parts = [cyl("body", r, L * 0.62, -0.5 * L + L * 0.14, 24, olive),
             cone("nose", r, L * 0.24, 0.5 * L - L * 0.24, 24, olive, r2=0.02),
             cone("taper", r, L * 0.14, -0.5 * L, 24, olive, r2=0.07)]
    for i in range(4):
        parts.append(fin("fin%d" % i, 0.42, 0.30, 0.28, 0.10,
                         -0.5 * L + 0.02, 0.014, olive, roll_deg=90 * i + 45))
    join("mk82", parts)
    export(os.path.join(out, "mk82.glb"), "mk82")


def build_r73(out):
    """R-73, die MiG-29s Nahbereichsrunde. Masse [DOC]: Laenge 2,90 m, Rumpf 0,170 m, Spannweite
    0,510 m. Die vorderen Destabilisatoren sind ihr Erkennungsmerkmal."""
    clear()
    r, L = 0.085, 2.90
    grey = mat("r73_body", (0.72, 0.73, 0.75), rough=0.40, metal=0.5)
    dark = mat("r73_seeker", (0.08, 0.08, 0.10), rough=0.20)
    parts = [cyl("body", r, L - 0.26, -0.5 * L, 24, grey),
             cone("seeker", r, 0.26, 0.5 * L - 0.26, 24, dark, r2=0.022)]
    for i in range(4):
        parts.append(fin("destab%d" % i, 0.16, 0.10, 0.09, 0.04,
                         0.5 * L - 0.52, 0.008, grey, roll_deg=90 * i))
        parts.append(fin("wing%d" % i, 0.34, 0.20, 0.255 - r, 0.09,
                         -0.5 * L + 0.55, 0.010, grey, roll_deg=90 * i + 45))
        parts.append(fin("tail%d" % i, 0.22, 0.12, 0.20, 0.06,
                         -0.5 * L + 0.04, 0.010, grey, roll_deg=90 * i + 45))
    join("r73", parts)
    export(os.path.join(out, "r73.glb"), "r73")


kBuilders = {"aim9": build_aim9, "mk82": build_mk82, "r73": build_r73}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--only", default="")
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    names = [a.only] if a.only else list(kBuilders)
    for n in names:
        kBuilders[n](a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

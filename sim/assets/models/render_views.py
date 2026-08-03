#!/usr/bin/env python3
"""Beweisrenderer: laedt ein .glb und rendert orthografische Risse + eine Dreiviertelansicht.

    Blender --background --python render_views.py -- --glb f16_L0.glb --out r.png [--res 900]

Die Risse sind ORTHOGRAFISCH und massstabsgleich, damit man sie ueber eine Blaupause legen kann.
"""
import bpy
import sys
import os
import math
import mathutils


def argv():
    a = sys.argv
    return a[a.index("--") + 1:] if "--" in a else []


def parse(args):
    d = {"res": "900", "shade": "solid"}
    k = None
    for t in args:
        if t.startswith("--"):
            k = t[2:]
            d[k] = "1"
        elif k:
            d[k] = t
    return d


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def scene_setup(res, transparent=True):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = res
    sc.render.resolution_y = res
    sc.render.film_transparent = transparent
    sc.render.image_settings.file_format = "PNG"
    sc.view_settings.view_transform = "Standard"
    try:
        sc.eevee.taa_render_samples = 32
    except Exception:
        pass
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.30, 0.33, 0.38, 1.0)
    w.node_tree.nodes["Background"].inputs[1].default_value = 1.1
    sc.world = w


def lights():
    for name, rot, e in (("key", (math.radians(55), 0, math.radians(35)), 4.0),
                         ("fill", (math.radians(70), 0, math.radians(-120)), 1.6),
                         ("rim", (math.radians(115), 0, math.radians(190)), 2.2)):
        d = bpy.data.lights.new(name, "SUN")
        d.energy = e
        d.angle = math.radians(6)
        o = bpy.data.objects.new(name, d)
        o.rotation_euler = rot
        bpy.context.collection.objects.link(o)


def bbox_all():
    lo = mathutils.Vector((1e9,) * 3)
    hi = mathutils.Vector((-1e9,) * 3)
    dg = bpy.context.evaluated_depsgraph_get()
    for o in bpy.context.scene.objects:
        if o.type != "MESH":
            continue
        me = o.evaluated_get(dg).to_mesh()
        for v in me.vertices:
            w = o.matrix_world @ v.co
            for i in range(3):
                lo[i] = min(lo[i], w[i])
                hi[i] = max(hi[i], w[i])
        o.evaluated_get(dg).to_mesh_clear()
    return lo, hi


def cam(name, loc, look, ortho_scale=None):
    c = bpy.data.cameras.new(name)
    if ortho_scale:
        c.type = "ORTHO"
        c.ortho_scale = ortho_scale
    else:
        c.lens = 85.0
    o = bpy.data.objects.new(name, c)
    o.location = loc
    d = mathutils.Vector(look) - mathutils.Vector(loc)
    o.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    bpy.context.collection.objects.link(o)
    return o


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    a = parse(argv())
    res = int(a["res"])
    glb = os.path.abspath(a["glb"])
    out = os.path.abspath(a["out"])
    clear()
    scene_setup(res)
    bpy.ops.import_scene.gltf(filepath=glb)
    # Bewegliche Knoten ausschlagen: --deflect "ctl.rudder=30,gear.main.l=84"
    #
    # WARUM KOMPONIEREN UND NICHT SETZEN (Runde-4-Befund 0). Die Ruhelage eines Knotens steht
    # als Rotation IM .glb: gear.nose 180 deg um X, gear.main.l 176.32 deg um (-0.962, 0.128,
    # -0.242), ctl.rudder 141.68 deg. Sie ist nicht neutral, sondern richtet die LOKALE X-Achse
    # auf das Scharnier aus. Ein Konsument dreht um genau diese lokale Achse, also
    #     M = M_ruhe * Rx(theta)
    # Wer stattdessen rotation_euler SETZT, wirft die Ruhelage weg und dreht um die WELT-X-Achse.
    # Gemessen an gear.nose=-92 deg: das Rad landete dadurch 1.16 m zu weit vorn im Einlaufmaul.
    if a.get("deflect", "1") not in ("1",):
        by = {o.name: o for o in bpy.context.scene.objects}
        for item in a["deflect"].split(","):
            if "=" not in item:
                continue
            name, deg = item.split("=")
            hit = ([o for n, o in by.items() if n.startswith(name[:-1])] if name.endswith("*")
                   else [by[name]] if name in by else [])
            if not hit:
                print("DEFLECT MISS %s" % name)
            for o in hit:
                base = o.matrix_basis.copy()
                o.matrix_basis = base @ mathutils.Matrix.Rotation(
                    math.radians(float(deg)), 4, "X")
                pw = o.parent.matrix_world if o.parent else mathutils.Matrix.Identity(4)
                w = pw @ base
                ax = (w.to_3x3() @ mathutils.Vector((1.0, 0.0, 0.0))).normalized()
                print("DEFLECT %-22s %+7.2f deg  Achse(Welt) %+.3f %+.3f %+.3f  "
                      "Drehpunkt(Welt) %+.3f %+.3f %+.3f"
                      % (o.name, float(deg), ax.x, ax.y, ax.z, *w.translation))
    # Zahlenbeweis statt Augenmass: Welt-Huellquader benannter Teilbaeume NACH dem Ausschlag.
    if a.get("probe", "1") not in ("1",):
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        for pref in a["probe"].split(","):
            lo = mathutils.Vector((1e9,) * 3)
            hi = mathutils.Vector((-1e9,) * 3)
            n = 0
            for o in bpy.context.scene.objects:
                if o.type != "MESH" or not o.name.startswith(pref):
                    continue
                n += 1
                me = o.evaluated_get(dg).to_mesh()
                for v in me.vertices:
                    w = o.matrix_world @ v.co
                    for i in range(3):
                        lo[i] = min(lo[i], w[i])
                        hi[i] = max(hi[i], w[i])
                o.evaluated_get(dg).to_mesh_clear()
            if n:
                print("PROBE %-24s n=%2d  x[%+.3f %+.3f] y[%+.3f %+.3f] z[%+.3f %+.3f]"
                      % (pref, n, lo.x, hi.x, lo.y, hi.y, lo.z, hi.z))
            else:
                print("PROBE %-24s LEER" % pref)

    lights()
    lo, hi = bbox_all()
    ctr = (lo + hi) * 0.5
    size = max(hi - lo)
    m = size * 1.12
    print("BBOX %.3f %.3f %.3f .. %.3f %.3f %.3f" % (*lo, *hi))
    print("SIZE x=%.3f y=%.3f z=%.3f" % tuple(hi - lo))
    tris = 0
    dg = bpy.context.evaluated_depsgraph_get()
    for o in bpy.context.scene.objects:
        if o.type == "MESH":
            me = o.evaluated_get(dg).to_mesh()
            me.calc_loop_triangles()
            tris += len(me.loop_triangles)
            o.evaluated_get(dg).to_mesh_clear()
    print("TRIS %d" % tris)

    # glTF-Import dreht +Y-oben nach +Z-oben zurueck: Modell steht wieder X=rechts, Y=vorn, Z=oben.
    d = size * 3.0
    views = {
        "side":  (cam("side",  (-d, ctr.y, ctr.z), (0, ctr.y, ctr.z), m)),
        "top":   (cam("top",   (ctr.x, ctr.y, d), (ctr.x, ctr.y, 0), m)),
        "front": (cam("front", (ctr.x, d, ctr.z), (ctr.x, 0, ctr.z), m)),
        "quart": (cam("quart", (ctr.x - d * 0.62, ctr.y + d * 0.60, ctr.z + d * 0.34), ctr, m * 1.05)),
        "rear":  (cam("rear",  (ctr.x + d * 0.45, ctr.y - d * 0.72, ctr.z + d * 0.22), ctr, m * 0.75)),
    }
    views["top"].rotation_euler = (0, 0, 0)
    base, ext = os.path.splitext(out)
    for k, c in views.items():
        bpy.context.scene.camera = c
        render("%s_%s%s" % (base, k, ext))
    print("OK")


main()

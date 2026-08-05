#!/usr/bin/env python3
"""Silhouetten-Gate fuer die LOD-Leiter.

    Blender --background --python check_lod.py -- --models sim/assets/models [--out /tmp/sil]
                                                  [--asset f16] [--res 1200] [--limit 2.0]

WARUM ES DIESES SKRIPT GIBT. Das Cauchy-Kriterium in build_f16.switch_table entscheidet JE
KOERPER, ob ein Wegfall unter ein Pixel faellt. Es ist damit in der SUMME blind: L2->L3 verlor
13 Fahrwerkskoerper und sechs Pylone, jeden einzeln unbedeutend, zusammen 25.8 % der
Seitensilhouette und 58.1 % der Frontsilhouette (Runde-4-Befund 3). Die Silhouette ist eine
Summe und wird deshalb als Summe GEMESSEN, nicht abgeschaetzt.

MESSUNG. Alle Stufen werden mit EINER festen orthografischen Kamera je Ansicht gerendert
(Mitte und Massstab aus der Sollgeometrie, nicht aus der jeweiligen Stufe — sonst misst man
Kameraversatz). Verglichen wird die Alpha-Maske: XOR-Flaeche geteilt durch die Flaeche der
groeberen Vergleichsstufe. Rueckgabe 0 = alle Uebergaenge unter --limit.
"""
import bpy
import sys
import os
import json
import mathutils
import numpy as np

kViews = ("side", "top", "front")


def argv():
    a = sys.argv
    return a[a.index("--") + 1:] if "--" in a else []


def parse(args):
    d = {"asset": "f16", "res": "1200", "limit": "2.0", "out": ""}
    k = None
    for t in args:
        if t.startswith("--"):
            k = t[2:]
            d[k] = "1"
        elif k:
            d[k] = t
    return d


def mask(glb, res, ctr, size, out_dir, tag):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = res
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    bpy.ops.import_scene.gltf(filepath=glb)
    # Glas zaehlt zur Silhouette: Transparenz wuerde die Haube aus der Maske loeschen.
    for m in bpy.data.materials:
        m.blend_method = "OPAQUE"
        if m.node_tree:
            for n in m.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    n.inputs["Alpha"].default_value = 1.0
    d = size * 3.0
    out = {}
    for name, loc, look in (("side", (-d, ctr[1], ctr[2]), (0, ctr[1], ctr[2])),
                            ("top", (ctr[0], ctr[1], d), (ctr[0], ctr[1], 0)),
                            ("front", (ctr[0], d, ctr[2]), (ctr[0], 0, ctr[2]))):
        c = bpy.data.cameras.new(name)
        c.type = "ORTHO"
        c.ortho_scale = size
        o = bpy.data.objects.new(name, c)
        o.location = loc
        v = mathutils.Vector(look) - mathutils.Vector(loc)
        o.rotation_euler = (0, 0, 0) if name == "top" else \
            v.to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(o)
        sc.camera = o
        p = os.path.join(out_dir, "sil_%s_%s.png" % (tag, name))
        sc.render.filepath = p
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(p)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(res, res, 4)[:, :, 3]
        bpy.data.images.remove(im)
        out[name] = a > 0.5
    return out


def main():
    a = parse(argv())
    models = os.path.abspath(a["models"])
    out_dir = os.path.abspath(a["out"]) if a["out"] else models
    os.makedirs(out_dir, exist_ok=True)
    res, limit = int(a["res"]), float(a["limit"])
    doc = json.load(open(os.path.join(models, "%s.asset.json" % a["asset"])))
    bb = doc["lods"][0]["bbox"]
    ctr = [0.5 * (bb[k][0] + bb[k][1]) for k in "xyz"]
    size = max(bb[k][1] - bb[k][0] for k in "xyz") * 1.06
    lods = [l["lod"] for l in doc["lods"]]
    m = {l: mask(os.path.join(models, "%s_%s.glb" % (a["asset"], l)),
                 res, ctr, size, out_dir, l) for l in lods}
    rc, rows = 0, []
    for i in range(len(lods) - 1):
        lo, hi = lods[i], lods[i + 1]
        for v in kViews:
            base = m[lo][v].sum()
            xor = int(np.logical_xor(m[lo][v], m[hi][v]).sum())
            pct = 100.0 * xor / max(base, 1)
            rows.append((lo, hi, v, xor, pct))
            if pct > limit:
                rc = 1
    for lo, hi, v, xor, pct in rows:
        print("SIL %s->%s %-5s XOR %6d px  %5.2f %%  %s"
              % (lo, hi, v, xor, pct, "OK" if pct <= limit else "UEBER GRENZE"))
    for v in kViews:
        pct = 100.0 * np.logical_xor(m[lods[0]][v], m[lods[-1]][v]).sum() / m[lods[0]][v].sum()
        print("SIL %s->%s %-5s gesamt        %5.2f %%" % (lods[0], lods[-1], v, pct))
    print("GRENZE %.2f %%   %s" % (limit, "BESTANDEN" if rc == 0 else "DURCHGEFALLEN"))
    return rc


sys.exit(main())

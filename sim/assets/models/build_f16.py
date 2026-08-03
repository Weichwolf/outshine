#!/usr/bin/env python3
"""FlightBox — F-16C **Block 52**. Vier LOD-Stufen aus EINER parametrischen Quelle.

    /Applications/Blender.app/Contents/MacOS/Blender --background --python build_f16.py -- \
        --out sim/assets/models [--lod 0] [--blend]

WARUM BLOCK 52. f16.xml deklariert F100-PW-229 — Pratt & Whitney. Damit gilt NSI-Einlauf und
P&W-Duese, nicht MCID und F110. Prinzip 4: Referenz ist das geflogene Modell.

WARUM GELOFTET. Ein Rumpf aus Zylindern ist ein Platzhalter. Die Spantrissen kommen aus dem
vermessenen Block-52-Riss (f16_geometry.py, jede Zahl mit Quelle), das Skript zieht die Haut.

WARUM VIER DATEIEN. Ein Asset ist eine Stufenleiter. Jede Stufe entsteht aus denselben Stationen
mit groeberer Abtastung — kein Dezimierer erfindet Kanten. Die Umschaltweiten werden aus den
tatsaechlich VERLORENEN Koerpern gerechnet (s. switch_table), nicht gesetzt.

KOORDINATEN. Blender-Achsen (+X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m), der glTF-Export
dreht auf +Y-oben/-Z-vorwaerts. Nullpunkt = der VRP aus f16.xml.
"""
import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bmesh                                     # noqa: E402
import bpy                                       # noqa: E402
import numpy as np                               # noqa: E402
from mathutils import Matrix, Vector             # noqa: E402
import f16_geometry as G                         # noqa: E402

TAU = math.pi * 2.0

# detail: 2 = alles, 1 = ohne Kleinstteile (<0.1 m), 0 = ohne Verkleidungen und Antennen
#
# WARUM L3 FAHRWERK BEHAELT (Runde-4-Befund 3). Das Cauchy-Kriterium (s. switch_table) ist JE
# KOERPER richtig und in der SUMME falsch: L2->L3 liess 13 Fahrwerkskoerper und sechs Pylone auf
# einmal fallen, jeder einzeln unter einem Pixel, zusammen 25.8 % XOR-Flaeche in der Seitenansicht
# und 58.1 % in der Frontansicht — und das Flugzeug schwebte 0.449 m ueber der Piste. Die
# Silhouette ist eine Summe, also wird sie als Summe gefordert: L3 baut dasselbe Teilelager wie
# L2, nur mit ring=8 statt 10. Was faellt, sind Aufloesung und Textur, nicht Koerper.
kLod = [
    dict(name="L0", fus_n=150, fus_m=104, wing_n=48, wing_m=60, petals=15, ring=24,
         duct=6, gear=2, detail=2, missiles=True, single_mat=False, tex=2048),
    dict(name="L1", fus_n=86, fus_m=60, wing_n=28, wing_m=36, petals=15, ring=16,
         duct=5, gear=2, detail=2, missiles=True, single_mat=False, tex=1024),
    dict(name="L2", fus_n=44, fus_m=28, wing_n=13, wing_m=18, petals=7, ring=10,
         duct=4, gear=1, detail=1, missiles=True, single_mat=True, tex=512),
    dict(name="L3", fus_n=22, fus_m=16, wing_n=7, wing_m=10, petals=0, ring=8,
         duct=3, gear=1, detail=1, missiles=True, single_mat=True, tex=256),
]

# Atlas: Rumpf oben, Fluegel links/rechts GETRENNT (damit die Hoheitszeichen seitenrichtig
# sitzen), Leitwerk, Sammelkachel und eine ruhige Kachel fuer flache Kleinteile.
kTile = dict(fus=(0.0, 0.5, 1.0, 0.5),
             wing_l=(0.0, 0.25, 0.5, 0.25), wing_r=(0.5, 0.25, 0.5, 0.25),
             tail=(0.0, 0.0, 0.34, 0.25), misc=(0.34, 0.0, 0.33, 0.25),
             plain=(0.67, 0.0, 0.33, 0.25))


# ================================================================ Kleinkram

def lerp_table(tab, x):
    if x <= tab[0][0]:
        return tab[0][1]
    if x >= tab[-1][0]:
        return tab[-1][1]
    for i in range(len(tab) - 1):
        x0, y0, x1, y1 = tab[i][0], tab[i][1], tab[i + 1][0], tab[i + 1][1]
        if x0 <= x <= x1:
            return y0 + (y1 - y0) * ((x - x0) / (x1 - x0) if x1 > x0 else 0.0)
    return tab[-1][1]


def lerp_row(tab, x, k):
    return lerp_table([(r[0], r[k]) for r in tab], x)


def superellipse(phi, a, b_up, b_dn, n_up, n_dn):
    c, s = math.cos(phi), math.sin(phi)
    b, n = (b_up, n_up) if s >= 0.0 else (b_dn, n_dn)
    return (a * math.copysign(abs(c) ** (2.0 / n), c),
            b * math.copysign(abs(s) ** (2.0 / n), s))


_AF_CACHE = {}


def airfoil_coeffs(t_c, xt):
    """NACA-4-stellig-MODIFIZIERTE Dickenverteilung.
    [WEB https://en.wikipedia.org/wiki/NACA_airfoil#Modified_NACA_four-digit_series]"""
    key = (round(t_c, 6), round(xt, 6))
    if key in _AF_CACHE:
        return _AF_CACHE[key]
    m = xt
    a0 = math.sqrt(2.0 * (1.1019 * (t_c ** 2) * 0.6))
    A = np.array([[m, m ** 2, m ** 3], [1.0, 2 * m, 3 * m ** 2], [0.0, 2.0, 6 * m]])
    a1, a2, a3 = np.linalg.solve(A, np.array([
        t_c / 2.0 - a0 * math.sqrt(m), -a0 / (2.0 * math.sqrt(m)), a0 / (4.0 * m ** 1.5)]))
    d0, k = 0.002, 1.0 - m
    B = np.array([[k, k ** 2, k ** 3], [1.0, 2 * k, 3 * k ** 2], [0.0, 2.0, 6 * k]])
    d1, d2, d3 = np.linalg.solve(B, np.array([t_c / 2.0 - d0, 0.0, 2 * a2 + 6 * a3 * m]))
    _AF_CACHE[key] = (a0, a1, a2, a3, d0, d1, d2, d3, m)
    return _AF_CACHE[key]


def airfoil(t_c, camber, xt, x):
    a0, a1, a2, a3, d0, d1, d2, d3, m = airfoil_coeffs(t_c, xt)
    x = min(max(x, 0.0), 1.0)
    if x <= m:
        y = a0 * math.sqrt(x) + a1 * x + a2 * x ** 2 + a3 * x ** 3
    else:
        u = 1.0 - x
        y = d0 + d1 * u + d2 * u ** 2 + d3 * u ** 3
    if camber:
        yc = (camber * (2 * xt * x - x * x) / (xt * xt) if x < xt else
              camber * ((1 - 2 * xt) + 2 * xt * x - x * x) / ((1 - xt) ** 2))
    else:
        yc = 0.0
    return max(y, 0.0), yc


# ================================================================ Netz-Werkzeug

def build(name, verts, faces, uvs, mat, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    uvl = me.uv_layers.new(name="UVMap")
    for f, quad in zip(me.polygons, uvs):
        for i in range(f.loop_total):
            uvl.data[f.loop_start + i].uv = quad[min(i, len(quad) - 1)]
    if smooth:
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    me.validate(verbose=False)
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    if mat:
        o.data.materials.append(mat)
    return o


def orient(obj, inward=False):
    """Runde-3-Befund 5: neun Koerper hatten umgestuelpte Normalen, verdeckt von doubleSided.

    Hier wird die Windung EINMAL zentral gerichtet: bmesh bestimmt die Aussenseite ueber den
    Extremalpunkt (kein Raten), danach wird bei Bedarf gespiegelt. Das erfindet keine Geometrie,
    es legt nur die Umlaufrichtung fest — und die Materialien schalten Rueckseitenkeulung EIN.
    """
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if inward:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return obj


def signed_volume(me):
    me.calc_loop_triangles()
    v = 0.0
    for t in me.loop_triangles:
        a, b, c = (me.vertices[i].co for i in t.vertices)
        v += a.dot(b.cross(c))
    return v / 6.0


def face_inward(obj):
    """Normalen DETERMINISTISCH nach innen drehen: das vorzeichenbehaftete Volumen eines fast
    geschlossenen Rohrs ist positiv, wenn die Windung nach aussen zeigt. Kein Heuristik-Aufruf.
    (bmesh.recalc_face_normals scheitert an einem einseitigen Kanal — Runde 3 gemessen: der
    Einlaufkanal war im Nahbild komplett unsichtbar, der Hintergrund schien durch das Maul.)"""
    if signed_volume(obj.data) > 0.0:
        obj.data.flip_normals()
        obj.data.update()
    return obj


def grid_mesh(name, rings, close_u, mat, rect, flip=False, smooth=True, caps=False,
              skip=None, inward=False, raw=False):
    """Loft ueber `rings`. `skip(v,u)` laesst eine Flaeche aus (Cockpitoeffnung),
    `caps` schliesst die beiden Endringe zu N-Ecken."""
    nv, nu = len(rings), len(rings[0])
    verts = [p for ring in rings for p in ring]
    faces, uvs = [], []
    du = nu if close_u else nu - 1
    u0, v0, uw, vh = rect
    for v in range(nv - 1):
        for u in range(du):
            if skip is not None and skip(v, u):
                continue
            un = (u + 1) % nu
            q = [v * nu + u, v * nu + un, (v + 1) * nu + un, (v + 1) * nu + u]
            tu = [u / du, (u + 1) / du, (u + 1) / du, u / du]
            tv = [v / (nv - 1)] * 2 + [(v + 1) / (nv - 1)] * 2
            if flip:
                q, tu, tv = q[::-1], tu[::-1], tv[::-1]
            faces.append(q)
            uvs.append([(u0 + a * uw, v0 + b * vh) for a, b in zip(tu, tv)])
    if caps:
        faces.append(list(range(nu))[::-1])
        uvs.append([(u0 + uw * (0.1 + 0.8 * i / nu), v0 + 0.10 * vh) for i in range(nu)])
        faces.append(list(range((nv - 1) * nu, nv * nu)))
        uvs.append([(u0 + uw * (0.1 + 0.8 * i / nu), v0 + 0.90 * vh) for i in range(nu)])
    o = build(name, verts, faces, uvs, mat, smooth)
    return o if raw else orient(o, inward)


def _ccw(outline):
    a = sum((outline[i][0] * outline[(i + 1) % len(outline)][1]
             - outline[(i + 1) % len(outline)][0] * outline[i][1]) for i in range(len(outline)))
    return outline if a > 0 else outline[::-1]


def plate(name, outline, thick, mat, rect, camber=0.0):
    """Koerper aus einem 2D-Umriss in der XY-Ebene, ueber z gedickt.

    Runde-3-Befund 11: die Deckel-UVs lagen frueher alle auf DERSELBEN v-Zeile — die Kachel wurde
    damit als eine einzige Texturzeile ueber die ganze Flaeche geschmiert (das "gestreckte
    Chevron" auf den Bremsklappen). Jetzt traegt der Deckel seine eigenen Umrisskoordinaten.
    """
    o2 = _ccw(list(outline))
    n = len(o2)
    xs = [p[0] for p in o2]
    ys = [p[1] for p in o2]
    x0, x1, y0v, y1v = min(xs), max(xs), min(ys), max(ys)
    span = max(x1 - x0, 1e-6)
    spanv = max(y1v - y0v, 1e-6)

    def bulge(x):
        t = (x - x0) / span
        return camber * math.sin(math.pi * min(max(t, 0.0), 1.0))
    verts = ([(x, y, +0.5 * thick + bulge(x)) for x, y in o2]
             + [(x, y, -0.5 * thick + 0.25 * bulge(x)) for x, y in o2])
    u0, v0, uw, vh = rect
    faces, uvs = [], []
    for i in range(n):
        j = (i + 1) % n
        faces.append([i, n + i, n + j, j])
        uvs.append([(u0 + uw * (0.05 + 0.9 * i / n), v0 + 0.05 * vh),
                    (u0 + uw * (0.05 + 0.9 * i / n), v0 + 0.25 * vh),
                    (u0 + uw * (0.05 + 0.9 * j / n), v0 + 0.25 * vh),
                    (u0 + uw * (0.05 + 0.9 * j / n), v0 + 0.05 * vh)])

    def cap_uv(i):
        x, y = o2[i]
        return (u0 + uw * (0.05 + 0.90 * (x - x0) / span),
                v0 + vh * (0.30 + 0.42 * (y - y0v) / spanv))
    faces.append(list(range(n)))
    uvs.append([cap_uv(i) for i in range(n)])
    faces.append(list(range(n, 2 * n))[::-1])
    uvs.append([cap_uv(i) for i in range(n)][::-1])
    return orient(build(name, verts, faces, uvs, mat, smooth=False))


def tube(name, path, radii, m, mat, rect, cap0=True, cap1=True, inward=False):
    m = m + (m & 1)                              # gerade Ringzahl: sonst ist die gespiegelte
    rings = []                                   # Seite kein exaktes Spiegelbild (Befund 10)
    for (p, ax), r in zip(path, radii):
        a = np.array(ax, dtype=float)
        a /= np.linalg.norm(a)
        ref = np.array([0.0, 0.0, 1.0]) if abs(a[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
        e1 = np.cross(a, ref)
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(a, e1)
        rings.append([tuple(np.array(p, dtype=float) + r * math.cos(t) * e1 + r * math.sin(t) * e2)
                      for t in [TAU * i / m for i in range(m)]])
    if cap0:
        c = np.mean(np.array(rings[0]), axis=0)
        rings.insert(0, [tuple(c + 0.02 * (np.array(p) - c)) for p in rings[0]])
    if cap1:
        c = np.mean(np.array(rings[-1]), axis=0)
        rings.append([tuple(c + 0.02 * (np.array(p) - c)) for p in rings[-1]])
    return grid_mesh(name, rings, True, mat, rect, inward=inward)


def hinge(obj, pivot, axis=(1.0, 0.0, 0.0)):
    """Objektherkunft = echtes Scharnier, lokale X-Achse = Drehachse."""
    ax = Vector(axis).normalized()
    up = Vector((0.0, 0.0, 1.0)) if abs(ax.z) < 0.9 else Vector((0.0, 1.0, 0.0))
    ey = ax.cross(up).normalized()
    ez = ax.cross(ey).normalized()
    rot = Matrix((ax, ey, ez)).transposed().to_4x4()
    obj.data.transform(rot.inverted() @ Matrix.Translation(-Vector(pivot)))
    obj.matrix_basis = Matrix.Translation(Vector(pivot)) @ rot
    return obj


def _remap_uv(me, src, dst):
    su, sv, sw, sh = src
    du, dv, dw, dh = dst
    for layer in me.uv_layers:
        for d in layer.data:
            u, v = d.uv
            d.uv = (du + (u - su) / max(sw, 1e-9) * dw, dv + (v - sv) / max(sh, 1e-9) * dh)


def mirror(obj, name, src=None, dst=None):
    """Die rechte Seite ist IMMER das Spiegelbild der linken, nie eine zweite Erzeugung —
    sonst weicht sie bei ungeraden Ringzahlen ab (Runde-3-Befund 10, gemessen 4.7 mm)."""
    o = obj.copy()
    o.data = obj.data.copy()
    o.name = o.data.name = name
    o.data.transform(Matrix.Diagonal((-1.0, 1.0, 1.0, 1.0)))
    o.data.flip_normals()
    if src is not None:
        _remap_uv(o.data, src, dst)
    m = obj.matrix_basis.copy()
    for j in (1, 2, 3):
        m[0][j] = -m[0][j]
    for i in (1, 2, 3):
        m[i][0] = -m[i][0]
    o.matrix_basis = m
    bpy.context.collection.objects.link(o)
    return o


# ================================================================ Material und Textur

def _img(name, arr, srgb):
    h, w, _ = arr.shape
    im = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    im.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    flat = np.ones((h, w, 4), dtype=np.float32)
    flat[:, :, :3] = arr
    im.pixels.foreach_set(flat.ravel())
    im.update()
    return im


# FS-595-Toene des "Hill Gray II"-Anstrichs der USAF-F-16C, sRGB genaehert.
# [WEB https://www.cybermodeler.com/aircraft/f-16/f-16c_profile01.shtml]
# [WEB https://www.usaf-sig.org/index.php/component/content/article/82-f-16-viper-faq-stuff-you-wanted-to-know-about-the-f-16cd]
kFs = {"36118": (0.353, 0.369, 0.373),      # Gunship Gray  — Ruecken
       "36270": (0.486, 0.502, 0.522),      # Neutral Gray  — Flanken
       "36375": (0.651, 0.678, 0.698),      # Light Ghost   — Bauch
       "36320": (0.490, 0.522, 0.545)}      # Dark Ghost    — Radom


def _smooth_upsample(g, w, h):
    """Bilinear von einem groben Gitter auf die Zielaufloesung — ohne Bloecke."""
    cv, cu = g.shape
    yi = np.linspace(0, cv - 1, h)
    xi = np.linspace(0, cu - 1, w)
    y0 = np.clip(yi.astype(int), 0, max(cv - 2, 0))
    x0 = np.clip(xi.astype(int), 0, max(cu - 2, 0))
    fy = (yi - y0)[:, None]
    fx = (xi - x0)[None, :]
    return ((g[y0][:, x0] * (1 - fx) + g[y0][:, x0 + 1] * fx) * (1 - fy)
            + (g[y0 + 1][:, x0] * (1 - fx) + g[y0 + 1][:, x0 + 1] * fx) * fy)


def _upsample(f, w, h, coarse=160):
    cu, cv = min(coarse, w), min(coarse, h)
    g = np.array([[f(i / (cu - 1), j / (cv - 1)) for i in range(cu)] for j in range(cv)],
                 dtype=np.float32)
    return _smooth_upsample(g, w, h)


def _breaks(rng, n, jitter=0.55):
    d = 1.0 + jitter * (rng.random(n) - 0.5) * 2.0
    c = np.cumsum(d)
    return c / c[-1]


def _lines(pos, n, width):
    idx = np.clip((pos * (n - 1)).astype(int), 0, n - 1)
    m = np.zeros(n, dtype=bool)
    for k in range(-(width // 2), width - width // 2):
        m[np.clip(idx + k, 0, n - 1)] = True
    return m


def _star(h, w, cx, cy, r):
    yy, xx = np.mgrid[0:h, 0:w]
    dx = (xx - cx) / max(r, 1e-6)
    dy = (yy - cy) / max(r, 1e-6)
    rr = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dy, dx) + math.pi / 2.0
    seg = math.pi * 2.0 / 5.0
    ph = np.abs(((th % seg) + seg) % seg - seg / 2.0)
    ri = 0.382
    bound = (ri * math.sin(seg / 2.0)) / (np.sin(ph) + ri * np.sin(seg / 2.0 - ph) + 1e-9)
    return rr <= np.clip(bound, 0.0, 1.0)


def _insignia(col, orm, x0, y0, s):
    """Low-Vis-Star-and-Bar der USAF, dunkelgrau statt blau/weiss.
    [WEB https://www.usaf-sig.org/index.php/component/content/article/82-f-16-viper-faq-stuff-you-wanted-to-know-about-the-f-16cd]"""
    h, w, _ = col.shape
    sw = int(s * 2.6)
    sh = int(s * 1.0)
    if sw < 6 or sh < 3 or x0 < 0 or y0 < 0 or x0 + sw >= w or y0 + sh >= h:
        return
    sub = col[y0:y0 + sh, x0:x0 + sw]
    tone = np.array((0.24, 0.25, 0.26), dtype=np.float32)
    ring = np.zeros((sh, sw), dtype=bool)
    cy, cx, r = sh * 0.5, sw * 0.5, sh * 0.48
    yy, xx = np.mgrid[0:sh, 0:sw]
    disc = ((xx - cx) ** 2 + (yy - cy) ** 2) <= r * r
    star = _star(sh, sw, cx, cy, r * 0.92)
    barh = sh * 0.42
    bar = (np.abs(yy - cy) <= barh) & (((xx > cx + r * 0.92) & (xx < cx + r * 2.35))
                                       | ((xx < cx - r * 0.92) & (xx > cx - r * 2.35)))
    ring |= (disc & ~star) | bar
    sub[ring] = tone
    orm[y0:y0 + sh, x0:x0 + sw, 1] = np.where(ring, 0.30, orm[y0:y0 + sh, x0:x0 + sw, 1])


def _block(col, x0, y0, w, h, tone):
    hh, ww, _ = col.shape
    x1, y1 = min(x0 + w, ww), min(y0 + h, hh)
    if x1 > x0 and y1 > y0:
        col[y0:y1, x0:x1] = tone


def _digits(col, x0, y0, cell, text, tone):
    seg = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc",
           "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcfgd",
           "-": "g", " ": ""}
    w, h = cell, int(cell * 1.8)
    t = max(1, cell // 5)
    for i, ch in enumerate(text):
        bx = x0 + i * (w + max(2, cell // 3))
        for s in seg.get(ch, ""):
            if s == "a":
                _block(col, bx, y0, w, t, tone)
            elif s == "b":
                _block(col, bx + w - t, y0, t, h // 2, tone)
            elif s == "c":
                _block(col, bx + w - t, y0 + h // 2, t, h // 2, tone)
            elif s == "d":
                _block(col, bx, y0 + h - t, w, t, tone)
            elif s == "e":
                _block(col, bx, y0 + h // 2, t, h // 2, tone)
            elif s == "f":
                _block(col, bx, y0, t, h // 2, tone)
            elif s == "g":
                _block(col, bx, y0 + h // 2 - t // 2, w, t, tone)


def paint_atlas(res, tiles, rng):
    """Backt Basisfarbe, Rauheit/Metall und Normalen prozedural in einen Atlas.

    Runde-3-Befund 11: das Hoehenfeld trug frueher ein 16x16-Blockrauschen (np.kron) und las sich
    deshalb als grobe Steppung statt als Beplankung. Jetzt gehen NUR Stoesse und Nieten in die
    Normalen; das Korn bleibt in Farbe und Rauheit und wird bilinear interpoliert statt geklotzt.

    Hoheitszeichen nach USAF-Praxis: Fluegel OBEN LINKS und UNTEN RECHTS (deshalb zwei getrennte
    Fluegelkacheln) sowie beide Rumpfflanken.
    """
    base = np.empty((res, res, 3), dtype=np.float32)
    base[:] = kFs["36270"]
    orm = np.zeros((res, res, 3), dtype=np.float32)
    orm[:, :, 0] = 1.0
    orm[:, :, 1] = 0.42
    hgt = np.zeros((res, res), dtype=np.float32)
    ds = min(1.0, res / 1024.0)
    cn = max(8, res // 24)
    grain = _smooth_upsample(rng.random((cn, cn)).astype(np.float32) - 0.5, res, res)
    fine = (rng.random((res, res)).astype(np.float32) - 0.5)

    for rect, nzf, radf, kind, (su, sv) in tiles:
        u0, v0, uw, vh = rect
        x0, x1 = int(u0 * res), int((u0 + uw) * res)
        y0, y1 = int(v0 * res), int((v0 + vh) * res)
        w, h = x1 - x0, y1 - y0
        col = np.empty((h, w, 3), dtype=np.float32)
        col[:] = kFs["36270"]
        if nzf is not None:
            ni = _upsample(nzf, w, h)
            col[ni > 0.32] = kFs["36118"]
            col[ni < -0.32] = kFs["36375"]
        if radf is not None:
            col[_upsample(radf, w, h) > 0.5] = kFs["36320"]
        lw = max(1, res // 512)
        lu = _lines(_breaks(rng, max(int(su * ds), 3)), w, lw)
        lv = _lines(_breaks(rng, max(int(sv * ds), 4)), h, lw)
        seam = lu[None, :] | lv[:, None]
        rivu = ((np.arange(w) % max(3, res // 170)) == 0)
        rivv = ((np.arange(h) % max(3, res // 170)) == 0)
        riv = (rivu[None, :] & lv[:, None]) | (rivv[:, None] & lu[None, :])
        col[seam] *= 1.0 - 0.105 * ds
        col[riv] *= 1.0 - 0.048 * ds
        gz = grain[y0:y1, x0:x1]
        col = col * (1.0 + 0.10 * gz)[:, :, None] + (0.014 * fine[y0:y1, x0:x1])[:, :, None]
        r = 0.40 + 0.11 * gz + 0.05 * ds * seam
        if kind in ("wing_l", "wing_r"):
            edge = (np.arange(w) / max(w - 1, 1) < 0.05) | (np.arange(w) / max(w - 1, 1) > 0.95)
            r = r - 0.10 * edge[None, :]
            col = col * (1.0 + 0.18 * edge[None, :, None])
            # u < 0.5 = Oberseite, u > 0.5 = Unterseite (Profilumlauf VK->HK->VK).
            ux = 0.16 if kind == "wing_l" else 0.66
            _insignia(col, orm[y0:y1, x0:x1], int(w * ux), int(h * 0.42), int(h * 0.15))
        if kind == "body":
            # u: 0 = Bauch, 0.25 = rechte Flanke, 0.5 = Ruecken, 0.75 = linke Flanke.
            for ux in (0.25, 0.75):
                _insignia(col, orm[y0:y1, x0:x1], int(w * ux) - int(h * 0.072),
                          int(h * 0.44), int(h * 0.055))
            _digits(col, int(w * 0.27), int(h * 0.30), max(2, h // 90), "91-0352",
                    np.array((0.20, 0.21, 0.22), dtype=np.float32))
        if kind == "tail":
            _digits(col, int(w * 0.30), int(h * 0.12), max(3, h // 40), "352",
                    np.array((0.18, 0.19, 0.20), dtype=np.float32))
            _block(col, int(w * 0.10), int(h * 0.05), int(w * 0.55), max(2, h // 40),
                   np.array((0.30, 0.31, 0.33), dtype=np.float32))
        if kind == "body":
            fade = np.clip((np.arange(h) / max(h - 1, 1) - 0.88) / 0.12, 0.0, 1.0)[:, None]
            col = col * (1.0 - 0.45 * fade)[:, :, None]
            r = r + 0.28 * fade
        base[y0:y1, x0:x1] = np.clip(col, 0.0, 1.0)
        orm[y0:y1, x0:x1, 1] = np.clip(r, 0.08, 0.95)
        orm[y0:y1, x0:x1, 2] = np.clip(0.04 + 0.30 * (r < 0.33), 0.0, 1.0)
        hgt[y0:y1, x0:x1] = ds * (-0.42 * seam + 0.22 * riv)

    gx = np.zeros_like(hgt)
    gy = np.zeros_like(hgt)
    gx[:, 1:-1] = hgt[:, 2:] - hgt[:, :-2]
    gy[1:-1, :] = hgt[2:, :] - hgt[:-2, :]
    nrm = np.stack([-gx * 4.0, -gy * 4.0, np.ones_like(hgt)], axis=-1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    return base, orm, (nrm * 0.5 + 0.5).astype(np.float32)


def mat_textured(name, base, orm, nrm):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.use_backface_culling = True                # Befund 5: doubleSided war selbst der Defekt
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tb = nt.nodes.new("ShaderNodeTexImage")
    tb.image = base
    nt.links.new(tb.outputs["Color"], b.inputs["Base Color"])
    to = nt.nodes.new("ShaderNodeTexImage")
    to.image = orm
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(to.outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs["Green"], b.inputs["Roughness"])
    nt.links.new(sep.outputs["Blue"], b.inputs["Metallic"])
    tn = nt.nodes.new("ShaderNodeTexImage")
    tn.image = nrm
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    return m


def mat_plain(name, rgb, rough=0.5, metal=0.0, alpha=1.0, ior=1.45, cull=True):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.use_backface_culling = cull
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "IOR" in b.inputs:
        b.inputs["IOR"].default_value = ior
    if alpha < 1.0:
        b.inputs["Alpha"].default_value = alpha
        for attr, val in (("blend_method", 'BLEND'), ("surface_render_method", 'BLENDED')):
            if hasattr(m, attr):
                try:
                    setattr(m, attr, val)
                except (TypeError, ValueError):
                    pass
    return m


# ================================================================ Rumpf

def fuse_section(s):
    """(a, z_deck, z_bot, z_chine, n_oben, n_unten) an Station s ab Radomspitze."""
    a = lerp_row(G.kFuseStations, s, 1)
    zt = lerp_row(G.kFuseStations, s, 2)
    zb = lerp_row(G.kFuseStations, s, 3)
    zc = lerp_table(G.kFuseChineZ, s)
    sh = lerp_table(G.kFuseChineSharp, s)
    nt = lerp_table(G.kFuseShapeTop, s) * (1.0 - sh) + 1.55 * sh
    nb = lerp_table(G.kFuseShapeBot, s) * (1.0 - sh) + 1.55 * sh
    zc = min(max(zc, zb + 0.05 * (zt - zb)), zt - 0.05 * (zt - zb))
    return a, zt, zb, zc, nt, nb


def fuse_ring(s, m, shrink=1.0):
    """Ringphase: k=0 ist der BAUCH, k=m/2 der RUECKEN. Damit liegt die Cockpitoeffnung in einem
    zusammenhaengenden Indexfenster (kein Umbruch bei u=0) und die Hoheitszeichen sitzen bei
    u=0.25 (rechts) und u=0.75 (links)."""
    a, zt, zb, zc, nt, nb = fuse_section(s)
    out = []
    for k in range(m):
        x, z = superellipse(TAU * k / m - math.pi / 2.0,
                            a * shrink, (zt - zc) * shrink, (zc - zb) * shrink, nt, nb)
        out.append((x, G.kS0 - s, zc + z))
    return out


def fuselage(cfg, mat, rect):
    """Rumpf mit COCKPITOEFFNUNG (Runde-3-Befund 4: die Haube oeffnete auf geschlossenes Blech).

    Die Oeffnung ist ein Rechteck im Indexraum des Lofts; ihr Rand ist damit ein sauberer,
    geschlossener Kantenzug, aus dem die Suellkante geloftet wird.
    """
    n = cfg["fus_n"]
    m = cfg["fus_m"] + (cfg["fus_m"] & 1)
    s0, s1 = G.kFuseStations[0][0], G.kFuseStations[-1][0]
    ss = [s0 + (s1 - s0) * (i / (n - 1)) ** 0.86 for i in range(n)]
    rings = [fuse_ring(s, m) for s in ss]
    rings.insert(0, fuse_ring(s0, m, shrink=0.08))
    rings.append(fuse_ring(s1, m, shrink=0.50))
    ss_all = [s0] + ss + [s1]

    inside = [i for i, s in enumerate(ss_all) if G.kCockpitOpenS0 <= s <= G.kCockpitOpenS1]
    v0, v1 = (inside[0], inside[-1]) if len(inside) >= 3 else (0, 0)
    # Indexbreite NUMERISCH bestimmen: die Superellipse laesst sich nicht in geschlossener Form
    # invertieren, also wird der Ring an der Mittelstation abgetastet, bis |x| das Sollmass hat.
    want = G.kCanopyHalfW * G.kCockpitOpenHalfW
    mid = fuse_ring(0.5 * (G.kCockpitOpenS0 + G.kCockpitOpenS1), m)
    kw = 2
    for k in range(2, m // 2 - 1):
        if abs(mid[(m // 2 - k) % m][0]) >= want:
            kw = k
            break
        kw = k
    u0, u1 = m // 2 - kw, m // 2 + kw
    cut = v1 > v0 + 1

    def skip(v, u):
        return v0 <= v < v1 and u0 <= u < u1

    o = grid_mesh("fuselage", rings, True, mat, rect, skip=skip if cut else None)
    if cut:
        wid = max(abs(p[0]) for v in range(v0, v1 + 1) for p in (rings[v][u0], rings[v][u1]))
        print("  COCKPIT Oeffnung s=%.2f..%.2f  halbe Breite %.3f m  (%d x %d Felder)"
              % (ss_all[v0], ss_all[v1], wid, v1 - v0, u1 - u0))

    rim = []
    if cut:
        for u in range(u0, u1 + 1):
            rim.append(rings[v0][u])
        for v in range(v0 + 1, v1 + 1):
            rim.append(rings[v][u1])
        for u in range(u1 - 1, u0 - 1, -1):
            rim.append(rings[v1][u])
        for v in range(v1 - 1, v0, -1):
            rim.append(rings[v][u0])

    def nzf(u, v):
        return -math.cos(TAU * u)

    def radf(u, v):
        return 1.0 if (s0 + (s1 - s0) * (v ** (1.0 / 0.86))) < 2.60 else 0.0
    return o, nzf, radf, rim


def coaming(rim, mat, rect):
    """Suellkante: das Blech am Rand der Cockpitoeffnung bekommt Dicke und einen Umschlag nach
    innen-unten. Ohne sie ist die Oeffnung eine Papierkante."""
    inner = []
    for x, y, z in rim:
        zc = fuse_section(G.kS0 - y)[1] - 0.16
        dx, dz = -x, zc - z
        nrm = math.hypot(dx, dz) or 1.0
        inner.append((x + 0.035 * dx / nrm, y, z + 0.035 * dz / nrm - 0.055))
    return grid_mesh("cockpit.sill", [rim, inner], True, mat, rect)


def strake(cfg, mat, rect):
    """Vorderkantenwurzel-Verlaengerung: duennes Schelf IN der Sehnenebene."""
    n = max(cfg["wing_n"], 10)
    k = max(cfg["wing_m"] // 7, 5)
    s_a, s_b = G.kStrakeEdge[0][0], G.kStrakeEdge[-1][0]
    rings = []
    for i in range(n):
        t = i / (n - 1)
        s = s_a + (s_b - s_a) * t
        xo = lerp_table(G.kStrakeEdge, s)
        a, _, _, zc, _, _ = fuse_section(s)
        xi = min(a * 0.93, xo - 0.02)
        th = lerp_table(G.kStrakeHalfThick, s)
        top, bot = [], []
        for j in range(k + 1):
            f = j / k
            x = xi + (xo - xi) * f
            e = th * (1.0 - f) ** 0.55 + 0.003
            top.append((-x, G.kS0 - s, zc + e))
            bot.append((-x, G.kS0 - s, zc - e))
        rings.append(bot[::-1] + top[1:-1])
    return grid_mesh("strake.l", rings, True, mat, rect, caps=True)


def fin_fillet(cfg, mat, rect):
    n = max(cfg["fus_n"] // 5, 8)
    k = max(cfg["ring"] // 2, 5)
    s0, s1 = 9.55, G.kFinTeS - 0.30
    rings = []
    for i in range(n):
        t = i / (n - 1)
        s = s0 + (s1 - s0) * t
        _, zt, _, _, _, _ = fuse_section(s)
        wid = 0.155 * math.sin(math.pi * min(max(t, 0.0), 1.0)) ** 0.55 + 0.030
        hgt = 0.07 + 0.15 * math.sin(math.pi * t) ** 0.9
        upper, lower = [], []
        for j in range(2 * k + 1):
            ph = math.pi * j / (2 * k)
            upper.append((wid * math.cos(ph), G.kS0 - s, zt - 0.03 + hgt * math.sin(ph)))
            lower.append((wid * math.cos(ph), G.kS0 - s, zt - 0.14))
        rings.append(upper + lower[-2:0:-1])
    rings.insert(0, [(0.02 * p[0], rings[0][0][1] + 0.02, p[2]) for p in rings[0]])
    rings.append([(0.02 * p[0], rings[-1][0][1] - 0.02, p[2]) for p in rings[-1]])
    return grid_mesh("fin.fillet", rings, True, mat, rect)


# ================================================================ Tragwerk

def wing_chord(y):
    b2 = G.kSpanRef / 2.0
    return G.kWingRootChord + (G.kWingTipChord - G.kWingRootChord) * min(y / b2, 1.0)


def wing_section(y, cf0, cf1, m, t_scale=1.0):
    """Profilschnitt der LINKEN Halbflaeche bei Halbspannweite y >= 0 (Blender-X = -y).
    cf0/cf1 duerfen Funktionen von y sein (Vorderkantenklappe: nicht-konstanter Tiefenanteil)."""
    c = wing_chord(y)
    le = G.kWingTeS - c
    a0 = cf0(y) if callable(cf0) else cf0
    a1 = cf1(y) if callable(cf1) else cf1
    up, dn = [], []
    for i in range(m):
        xf = 0.5 * (1.0 - math.cos(math.pi * i / (m - 1)))
        x = a0 + (a1 - a0) * xf
        th, cam = airfoil(G.kTcRoot * t_scale, 0.011, G.kAirfoilXt, x)
        s = le + x * c
        up.append((-y, G.kS0 - s, (cam + th) * c))
        dn.append((-y, G.kS0 - s, (cam - th) * c))
    return up + dn[-2:0:-1]


def wing_panel(name, y0, y1, cf0, cf1, n, m, mat, rect, t_scale=1.0):
    ys = [y0 + (y1 - y0) * (i / (n - 1)) for i in range(n)]
    return grid_mesh(name, [wing_section(y, cf0, cf1, m, t_scale) for y in ys], True, mat, rect,
                     caps=True)


def tailplane(name, semi, root_c, tip_c, sweep, te_s, thick, n, m, mat, rect, z=0.0, y0=0.0):
    tan_s = math.tan(math.radians(sweep))
    rings = []
    for i in range(n):
        f = i / (n - 1)
        y = y0 + (semi - y0) * f
        c = root_c + (tip_c - root_c) * (y / semi)
        le = (te_s - root_c) + y * tan_s
        up, dn = [], []
        for k in range(m):
            xf = 0.5 * (1.0 - math.cos(math.pi * k / (m - 1)))
            th, _ = airfoil(thick, 0.0, 0.38, xf)
            up.append((-y, G.kS0 - (le + xf * c), z + th * c))
            dn.append((-y, G.kS0 - (le + xf * c), z - th * c))
        rings.append(up + dn[-2:0:-1])
    return grid_mesh(name, rings, True, mat, rect, caps=True)


def fin(cfg, mat, rect, z0=0.62, z1=None):
    z1 = G.kFinTipZ if z1 is None else z1
    n = max(cfg["wing_n"] // 2, 6)
    m = max(cfg["wing_m"] // 2, 14)
    rings = []
    for i in range(n):
        t = i / (n - 1)
        z = z0 + (z1 - z0) * t
        le = G.kFinLeZ0S + G.kFinLeSlope * z
        te = G.kFinTeZ0S + G.kFinTeSlope * z
        c = te - le
        lo, hi = [], []
        for k in range(m):
            xf = 0.5 * (1.0 - math.cos(math.pi * k / (m - 1)))
            th, _ = airfoil(0.052, 0.0, 0.38, xf)
            lo.append((-th * c, G.kS0 - (le + xf * c), z))
            hi.append((+th * c, G.kS0 - (le + xf * c), z))
        rings.append(lo + hi[-2:0:-1])
    return grid_mesh("fin", rings, True, mat, rect, caps=True)


def rudder(cfg, mat, rect):
    z0, z1 = 0.72, G.kFinTipZ - 0.08
    n = max(cfg["wing_n"] // 3, 4)
    m = max(cfg["wing_m"] // 3, 8)
    rings = []
    for i in range(n):
        t = i / (n - 1)
        z = z0 + (z1 - z0) * t
        le = G.kFinLeZ0S + G.kFinLeSlope * z
        te = G.kFinTeZ0S + G.kFinTeSlope * z
        c = te - le
        r0 = le + 0.70 * c
        lo, hi = [], []
        for k in range(m):
            xf = k / (m - 1)
            th = 0.052 * c * (1.0 - xf) ** 0.7 * 0.55 + 0.004
            lo.append((-th, G.kS0 - (r0 + xf * (te - r0)), z))
            hi.append((+th, G.kS0 - (r0 + xf * (te - r0)), z))
        rings.append(lo + hi[-2:0:-1])
    o = grid_mesh("ctl.rudder", rings, True, mat, rect, caps=True)
    a0 = G.kFinLeZ0S + G.kFinLeSlope * z0 + 0.70 * ((G.kFinTeZ0S + G.kFinTeSlope * z0)
                                                    - (G.kFinLeZ0S + G.kFinLeSlope * z0))
    a1 = G.kFinLeZ0S + G.kFinLeSlope * z1 + 0.70 * ((G.kFinTeZ0S + G.kFinTeSlope * z1)
                                                    - (G.kFinLeZ0S + G.kFinLeSlope * z1))
    axis = (0.0, -(a1 - a0), (z1 - z0))
    hinge(o, (0.0, G.kS0 - a0, z0), axis)
    return o


# ================================================================ Einlauf (NSI) und Duese (P&W)

def inlet(cfg, mat_skin, mat_duct, rect):
    """NSI — das Block-52-Maul. 1.374 x 0.533 m aussen (Herleitung in f16_geometry.py), rundere
    Ecken als der MCID. Der Kanal bleibt bis L3 OFFEN: ein zugestopftes Maul misst noch bei
    L2-Reichweite mehr als ein Pixel und laesst die Silhouette springen (Runde-3-Befund 6)."""
    m = max(cfg["ring"] * 2, 14)
    m += m & 1
    wo, ho = G.kInletOuterW * 0.5, G.kInletOuterH * 0.5
    wi, hi = G.kInletInnerW * 0.5, G.kInletInnerH * 0.5
    y_lip = G.kS0 - G.kInletLipS
    z_c = G.kInletLipZ + ho
    nc = G.kInletCornerN

    def ring(a, b, y, zc, n=nc):
        return [(lambda p: (p[0], y, zc + p[1]))(superellipse(TAU * k / m, a, b, b, n, n))
                for k in range(m)]

    ln, steps = G.kInletFairingEndS - G.kInletLipS, max(cfg["fus_n"] // 4, 8)
    body = []
    for i in range(steps):
        t = i / (steps - 1)
        s = G.kInletLipS + ln * t
        zb = lerp_table(G.kInletFairingBot, s)
        a_f, _, zr, _, _, _ = fuse_section(s)
        zt = zr - G.kDiverterGap * (1.0 - t) ** 0.5
        a = wo + (a_f * 0.98 - wo) * (t ** 1.7)
        body.append(ring(a, 0.5 * (zt - zb), G.kS0 - s, 0.5 * (zt + zb), n=nc + 0.6 * t))
    # KEIN Deckel vorn: dort sitzt das Maul. (Ein Deckel legte in Runde 3 eine Scheibe ueber die
    # untere Haelfte der Oeffnung — im Nahbild sofort sichtbar.)
    parts = [grid_mesh("inlet.fairing", body, True, mat_skin, rect)]
    parts.append(grid_mesh("inlet.lip",
                           [ring(wo, ho, y_lip + 0.010, z_c),
                            ring(0.5 * (wo + wi), 0.5 * (ho + hi), y_lip + 0.035, z_c),
                            ring(wi, hi, y_lip - 0.015, z_c)], True, mat_skin, rect))
    # Der Kanal ist ein einseitiges Rohr, dessen Windung EXPLIZIT ueber das Vorzeichen des
    # Volumens nach innen gedreht wird (face_inward) — bmesh' Heuristik liefert hier die falsche
    # Seite und der Kanal verschwindet dann komplett hinter der Rueckseitenkeulung.
    dn = max(int(cfg["duct"]), 3)
    rad = G.kInletDuctD * 0.5
    # Der Kanalboden IST die Decke des Bugfahrwerksschachts (Runde-4-Befund 6). Runde 3 liess den
    # Kanal mit 0.40 m Steigung geradeaus laufen und schob das eingefahrene Rad hinein. Der Kanal
    # steigt jetzt mindestens so weit, dass seine Unterseite ueber der Radhuelle bleibt — das ist
    # keine Kosmetik, sondern die Bauweise: bei der F-16 liegt der Schacht unter dem Kanal.
    ceil = nose_bay()["ceiling"]

    def duct_side(off):
        out = []
        for i in range(dn):
            t = i / (dn - 1)
            b = hi + (rad - hi) * t + off
            s_here = G.kInletLipS + G.kInletDuctDepth * t
            zc_d = z_c + 0.40 * t * t
            if s_here >= nose_bay()["s0"]:
                zc_d = max(zc_d, ceil + b)
            out.append(ring(wi + (rad - wi) * t + off, b,
                            y_lip - 0.02 - G.kInletDuctDepth * t, zc_d, n=nc - 1.0 * t))
        out.append([(0.02 * p[0], p[1] - 0.30, out[-1][0][2] + 0.10) for p in out[-1]])
        return out
    parts.append(face_inward(grid_mesh("inlet.duct", duct_side(0.0), True, mat_duct, rect,
                                       raw=True)))
    if cfg["detail"] >= 1:
        sp = []
        for i in range(6):
            t = i / 5.0
            s = G.kInletLipS + 0.9 * t
            _, _, zr, _, _, _ = fuse_section(s)
            wdt = 0.06 + 0.34 * t
            sp.append([(-wdt, G.kS0 - s, zr - G.kDiverterGap),
                       (wdt, G.kS0 - s, zr - G.kDiverterGap),
                       (wdt, G.kS0 - s, zr + 0.01), (-wdt, G.kS0 - s, zr + 0.01)])
        parts.append(grid_mesh("inlet.diverter", sp, True, mat_skin, rect, caps=True))
    return parts


def nozzle(cfg, mat_noz, mat_dark, rect):
    """F100-PW-229: der Mantel ist ueber kNozzleStraight seiner Laenge ZYLINDRISCH und endet
    gerade abgeschnitten — die Kohlefaser-Aussenklappen der -229 verdecken die divergenten
    Klappen. Die konisch verjuengte, offene Blattkrone der F110 waere die falsche Variante."""
    # Runde-4-Befund 8: hier standen drei nackte Zahlen. s0 ist jetzt kNozzleShroudS0 (= das
    # hintere Ende der Bremsklappen, dieselbe Linie im Riss), und der vordere Radius wie die
    # Achshoehe kommen aus dem RUMPFQUERSCHNITT an genau dieser Station statt aus dem Gefuehl —
    # 0.620 widersprach kNozzleShroudR = 0.5293 um +17.1 % und war durch nichts gedeckt.
    s0, s1 = G.kNozzleShroudS0, G.kNozzleExitS
    ln = s1 - s0
    m = max(cfg["ring"] * 2, 12)
    m += m & 1
    ro, re = G.kNozzleShroudR, G.kNozzleExitR
    _, zt0, zb0, _, _, _ = fuse_section(s0)
    r0 = 0.5 * (zt0 - zb0)                       # Rumpfhalbhoehe dort: der Mantel setzt buendig an
    zc = 0.5 * (zt0 + zb0)                       # und auf der Achse des Rumpfes, nicht daneben
    st = G.kNozzleStraight
    prof = [(0.00, r0), (0.10, ro), (st, ro), (1.00, ro * 0.965)]
    out = []
    for i in range(7):
        t = i / 6.0
        r = lerp_table(prof, t)
        y = G.kS0 - (s0 + ln * t)
        out.append([(r * math.cos(TAU * k / m), y, zc + r * math.sin(TAU * k / m))
                    for k in range(m)])
    parts = [grid_mesh("nozzle.shroud", out, True, mat_noz, rect)]
    inner = []
    for t, r in ((0.0, ro * 0.93), (0.35, re * 1.10), (1.0, re * 0.60)):
        y = G.kS0 - s1 + 0.02 + 0.72 * t     # NACH VORN in die Duese hinein
        inner.append([(r * math.cos(TAU * k / m), y, zc + r * math.sin(TAU * k / m))
                      for k in range(m)])
    inner.append([(0.02 * p[0], G.kS0 - s1 + 0.76, zc) for p in inner[-1]])
    parts.append(grid_mesh("nozzle.inner", inner, True, mat_dark, rect, inward=True))
    p = cfg["petals"]
    for i in range(p):
        a0 = TAU * (i + 0.06) / p
        a1 = TAU * (i + 0.94) / p
        rings = []
        for t, r in ((0.0, ro * 1.008), (st, ro * 1.012), (1.0, ro * 0.978)):
            y = G.kS0 - (s0 + 0.16 + (ln - 0.16) * t)
            rings.append([(r * math.cos(a), y, zc + r * math.sin(a))
                          for a in (a0, 0.5 * (a0 + a1), a1)])
        parts.append(grid_mesh("nozzle.petal.%02d" % i, rings, False, mat_noz, rect, caps=True))
    return parts


# ================================================================ Kanzel und Cockpit

def canopy_arch(s, m, scale=1.0):
    prof = [(G.kCanopyFrontS, 0.00, 0.05), (G.kCanopyFrontS + 0.30, 0.34, 0.46),
            (G.kCanopyBowS, 0.74, 0.88), (3.70, 0.95, 0.98), (G.kCanopyApexS, 1.00, 1.00),
            (4.70, 0.96, 0.99), (5.30, 0.86, 0.95), (6.05, 0.60, 0.83),
            (G.kCanopyRearS, 0.10, 0.55)]
    hf = lerp_table([(p[0], p[1]) for p in prof], s)
    wf = lerp_table([(p[0], p[2]) for p in prof], s)
    z0 = fuse_section(s)[1] - 0.015
    hgt = max((G.kCanopyApexZ - z0) * hf, 1e-3) * scale
    a = max(G.kCanopyHalfW * wf, 1e-3) * scale
    ring = []
    for k in range(m + 1):
        x, z = superellipse(max(math.pi * (1.0 - k / m), 1e-4), a, hgt, hgt, 2.30, 2.30)
        ring.append((x, G.kS0 - s, z0 + max(z, 0.0)))
    return ring, z0


def canopy(cfg, mat_glass, mat_frame, rect):
    """DREI Koerper statt einem (Runde-4-Befund 4): Windschutz — Haube — Ruecken-Verkleidung.

    Runde 3 haengte alles von s=2.44 bis 6.70 an den Haubenknoten und riss beim Oeffnen 0.76 m
    festen Windschutz und 1.82 m feste Verkleidung mit. Die Offen-Ansicht des Risses zeigt den
    Windschutz stehen und die Haube an ihrem Rahmen bei s=4.88 enden (Belege in f16_geometry).

    Rueckgabe: (feste Teile, bewegliche Teile). Die Naht liegt auf EXAKT derselben Station und
    damit auf denselben Stuetzpunkten von canopy_arch — kein Spalt, unabhaengig von n.
    """
    m = max(cfg["ring"], 9)
    s_f, s_b, s_g, s_r = (G.kCanopyFrontS, G.kCanopyBowS,
                          G.kCanopyGlassRearS, G.kCanopyRearS)

    def segment(name, sa, sb, mat, thick, n):
        ss = [sa + (sb - sa) * (i / (n - 1)) for i in range(n)]
        shell = []
        for s in ss:
            o, z0 = canopy_arch(s, m, 1.0)
            inn, _ = canopy_arch(s, m, 1.0 - thick)
            shell.append(o + [(x, y, max(z, z0 - 0.004)) for x, y, z in inn][::-1])
        body = grid_mesh(name, shell, True, mat, rect, caps=True)
        band = []
        for s in ss:
            o, z0 = canopy_arch(s, m, 1.030)
            band.append(o + [(x, y, z0 - 0.045) for x, y, _ in o][::-1])
        return [body, grid_mesh(name.rsplit(".", 1)[0] + ".rail", band, True,
                                mat_frame, rect, caps=True)]

    nb = max(cfg["fus_n"] // 12, 5)
    fixed = segment("windscreen.glass", s_f, s_b, mat_glass, 0.022, nb)
    moving = segment("canopy.glass", s_b, s_g, mat_glass, 0.022, max(cfg["fus_n"] // 8, 7))
    fixed += segment("spine.fairing", s_g, s_r, mat_frame, 0.060,
                     max(cfg["fus_n"] // 8, 7))
    if cfg["detail"] >= 2:
        # Der Trennrahmen gehoert zum FESTEN Windschutz: die Haube dichtet gegen ihn ab.
        bow = [canopy_arch(s_b + 0.028, m, 1.032)[0], canopy_arch(s_b - 0.028, m, 1.032)[0]]
        fixed.append(grid_mesh("windscreen.bow", bow, False, mat_frame, rect, caps=True))
        # Haubenrahmen hinten — er faehrt MIT.
        rear = [canopy_arch(s_g - 0.026, m, 1.032)[0], canopy_arch(s_g + 0.026, m, 1.032)[0]]
        moving.append(grid_mesh("canopy.frame.rear", rear, False, mat_frame, rect, caps=True))
    return fixed, moving


def cockpit(cfg, mat_dark, mat_frame, mat_glass, rect):
    """Wanne, ACES-II-Sitz, Instrumententafel und HUD — verankert am EYEPOINT aus f16.xml.
    Ohne sie fuehrt die Cockpitoeffnung in ein leeres Rumpfinneres (Runde-3-Befund 4).

    Alle Abstaende haengen an genau EINEM belegten Punkt: dem Augenpunkt (-336.2 | 0 | +29.5) in
    aus [XML]. Sitzflaeche 0.80 m darunter, Tafel 0.62 m davor, HUD dazwischen. Die Wanne endet
    dort, wo die Lehne endet — deshalb ist die Hautoeffnung kuerzer als die Kanzelschale.
    """
    m = max(cfg["ring"], 8)
    ye = G.kS0 - G.kEyeS
    z_pan = G.kSeatPanZ
    z_flr = G.kEyeZ - G.kCockpitFloorDrop
    out = []

    n = max(cfg["fus_n"] // 8, 6)
    s_a, s_b = G.kCockpitOpenS0 + 0.05, G.kCockpitOpenS1 - 0.05
    tub = []
    for i in range(n):
        s = s_a + (s_b - s_a) * (i / (n - 1))
        zt = fuse_section(s)[1] - 0.03
        a = G.kCanopyHalfW * G.kCockpitOpenHalfW * 0.94
        top = [(a * math.cos(math.pi * (1.0 - k / m)), G.kS0 - s, zt) for k in range(m + 1)]
        bot = [(0.78 * a * math.cos(math.pi * (1.0 - k / m)), G.kS0 - s, z_flr)
               for k in range(m + 1)]
        tub.append(top + bot[::-1])
    out.append(grid_mesh("cockpit.tub", tub, True, mat_dark, rect, caps=True, inward=True))
    if cfg["detail"] < 1:
        return out

    def box(name, x0, x1, y0, y1, z0, z1, mat, rot=None, at=None):
        v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        o = build(name, v, [[0, 1, 2, 3], [7, 6, 5, 4], [0, 4, 5, 1],
                            [1, 5, 6, 2], [2, 6, 7, 3], [3, 7, 4, 0]],
                  [[(rect[0] + 0.5 * rect[2], rect[1] + 0.5 * rect[3])] * 4] * 6,
                  mat, smooth=False)
        if rot is not None:
            o.data.transform(Matrix.Translation(at) @ Matrix.Rotation(rot, 4, 'X')
                             @ Matrix.Translation(-Vector(at)))
        return orient(o)

    # DREHSINN, ein fuer alle Mal (Runde-4-Befund 2). Matrix.Rotation(t,4,'X') bildet
    # (y,z) -> (y cos t - z sin t, y sin t + z cos t) ab. Ein Punkt UEBER dem Drehpunkt
    # (dy=0, dz>0) geht damit nach dy' = -dz sin t. Soll er nach HINTEN kippen (dy'<0), muss
    # sin t > 0 sein, also t > 0. Runde 3 uebergab -tilt und kippte Lehne, Tafel und HUD
    # nach VORN: die Lehne lief 0.32 m vor dem Gesicht des Piloten durch.
    pw = G.kCanopyHalfW * 0.72
    # Instrumententafel: senkrechte Flaeche 0.62 m vor dem Auge, 15 deg zum Piloten geneigt.
    y_pan = ye + 0.62
    out.append(box("cockpit.panel", -pw, pw, y_pan - 0.06, y_pan + 0.06,
                   z_pan + 0.10, z_pan + 0.56, mat_dark,
                   rot=math.radians(15.0), at=Vector((0.0, y_pan, z_pan + 0.33))))
    # Blendschutzhaube darueber, waagerecht nach hinten ueberstehend.
    out.append(box("cockpit.glareshield", -pw, pw, y_pan - 0.10, y_pan + 0.16,
                   z_pan + 0.56, z_pan + 0.62, mat_dark))
    # HUD: zwei Kombinerscheiben zwischen zwei Saeulen, 0.12 m hinter dem Blendschutz.
    y_hud = ye + 0.46
    for i, dz in enumerate((0.10, 0.28)):
        # Der Kombiner spiegelt das Bild der Roehre VON UNTEN ins Auge nach HINTEN. Die
        # Spiegelnormale halbiert (0,0,+1) und (0,-1,0), die Scheibe kippt also mit der
        # Oberkante nach HINTEN — derselbe Drehsinn wie Tafel und Lehne.
        c = box("cockpit.hud.%d" % i, -0.135, 0.135, y_hud - 0.005, y_hud + 0.005,
                z_pan + 0.64 + dz, z_pan + 0.64 + dz + 0.13, mat_glass,
                rot=math.radians(12.0), at=Vector((0.0, y_hud, z_pan + 0.70 + dz)))
        out.append(c)
    for sgn in (-1, 1):
        out.append(box("cockpit.hud.post.%d" % (sgn > 0), sgn * 0.135, sgn * 0.155,
                       y_hud - 0.02, y_hud + 0.02, z_pan + 0.62, z_pan + 1.08, mat_frame))
    # Konsolen links und rechts, auf Ellbogenhoehe.
    for sgn, nm in ((-1, "left"), (1, "right")):
        out.append(box("cockpit.console.%s" % nm, sgn * 0.19, sgn * 0.34,
                       ye - 0.10, y_pan - 0.06, z_pan + 0.02, z_pan + 0.26, mat_dark))
    # ACES II: Sitzflaeche, Lehne (30 deg), Kopfstuetze.
    sw = 0.21
    out.append(box("cockpit.seat.pan", -sw, sw, ye - 0.12, ye + 0.30,
                   z_pan - 0.04, z_pan + 0.05, mat_dark))
    tilt = math.radians(G.kSeatBackAngle)
    out.append(box("cockpit.seat.back", -sw, sw, ye - 0.09, ye + 0.02,
                   z_pan + 0.02, z_pan + 0.02 + G.kSeatBackH, mat_dark,
                   rot=tilt, at=Vector((0.0, ye - 0.04, z_pan + 0.02))))
    hz = z_pan + 0.02 + G.kSeatBackH * math.cos(tilt)
    hy = ye - 0.04 - G.kSeatBackH * math.sin(tilt)
    out.append(box("cockpit.seat.headrest", -0.115, 0.115, hy - 0.09, hy + 0.11,
                   hz - 0.12, hz + 0.09, mat_dark))
    if cfg["detail"] >= 2:
        # Seitenknueppel rechts, Schubhebel links — das Erkennungsmerkmal des F-16-Cockpits.
        out.append(tube("cockpit.stick",
                        [((0.22, ye + 0.24, z_pan + 0.26), (0, 0.28, 1)),
                         ((0.22, ye + 0.30, z_pan + 0.46), (0, 0.28, 1))],
                        [0.018, 0.024, ], 6, mat_dark, rect))
        out.append(tube("cockpit.throttle",
                        [((-0.24, ye + 0.16, z_pan + 0.28), (0, 1, 0.25)),
                         ((-0.24, ye + 0.34, z_pan + 0.33), (0, 1, 0.25))],
                        [0.026, 0.022], 6, mat_dark, rect))
    return out


# ================================================================ Fahrwerk

def _retract_axis(a_pt, t_pt, sweep_deg):
    """Zwei-Achsen-Kinematik, Runde-3-Befund 3: EIN X-Scharnier kann das Hauptbein nicht
    verstauen — es faehrt nach VORN und legt das Rad dabei flach in den Bauch.

    Gegeben der ausgefahrene Radmittelpunkt A, der gewuenschte eingefahrene T und der
    Schwenkwinkel a, ist die Drehachse EINDEUTIG bestimmt:
        n steht senkrecht auf (T-A)                (beide Punkte liegen gleich weit entlang n)
        R = |T-A| / (2 sin(a/2))                   (Sehnenformel)
        Q = (A+T)/2 + R cos(a/2) * norm(n x d)     (Achsdurchstosspunkt)
    Damit ist der Drehpunkt gerechnet und nicht geraten.
    """
    A = np.array(a_pt, dtype=float)
    T = np.array(t_pt, dtype=float)
    ch = T - A
    L = np.linalg.norm(ch)
    d = ch / L
    # Referenzrichtung SEITENRICHTIG waehlen (+X rechts, -X links) — sonst sind die beiden
    # Schwenkachsen keine Spiegelbilder und die Raeder legen sich verschieden flach.
    n0 = np.array([math.copysign(1.0, A[0]) if abs(A[0]) > 1e-9 else 1.0, 0.0, 0.0])
    n = n0 - float(np.dot(n0, d)) * d
    n /= np.linalg.norm(n)
    a = math.radians(abs(sweep_deg))
    R = L / (2.0 * math.sin(a / 2.0))
    u = np.cross(n, d)
    u /= np.linalg.norm(u)
    Q = 0.5 * (A + T) + R * math.cos(a / 2.0) * u
    if Q[2] < A[2]:                              # der Drehpunkt gehoert nach OBEN
        Q = 0.5 * (A + T) - R * math.cos(a / 2.0) * u
        n = -n
    return tuple(Q), tuple(n)


_NOSE_BAY = {}


def nose_bay():
    """Bugfahrwerksschacht: Schwenkwinkel, eingefahrene Radlage, Schachtgrenzen — GERECHNET.

    Runde 3 hatte kGearNoseSweep = -92 deg gesetzt und "geprueft: Rad im Schacht" dazugeschrieben.
    Damit landete das Rad bei s = 6.05..6.52 (Klappe 4.70..5.94) und mitten im Einlaufkanal
    (Runde-4-Befund 6). Der Betrag ist jetzt die Loesung einer FORDERUNG:

        Drehe das Rad um sein Scharnier nach hinten, bis sein tiefster Punkt genau 20 mm ueber
        der oertlichen Unterkante der Einlaufverkleidung liegt.

    Das ist der kleinste Winkel, bei dem die Klappe schliessen kann, und damit zugleich die
    GROESSTE Luft nach oben zum Kanal. Der Schacht faellt als Nebenprodukt ab: seine Grenzen
    sind die Huelle des eingeschwenkten Rades, nicht zwei gesetzte Zahlen.
    """
    if _NOSE_BAY:
        return _NOSE_BAY
    ny = G.kS0 - G.kNoseGearS
    rn = G.kTireNose / 2.0
    piv = (0.0, ny + 0.10, G.kGroundZ + 1.30)
    axle = (0.0, ny, G.kGroundZ + rn)
    dy, dz = axle[1] - piv[1], axle[2] - piv[2]
    best = None
    for i in range(1, 1801):                       # 0.05-deg-Raster, negativ = nach hinten
        th = math.radians(-i * 0.05)
        c, s = math.cos(th), math.sin(th)
        wy, wz = piv[1] + dy * c - dz * s, piv[2] + dy * s + dz * c
        z_skin = lerp_table(G.kInletFairingBot, G.kS0 - wy)
        if wz - rn >= z_skin + 0.020:
            best = (-i * 0.05, wy, wz)
            break
    if best is None:                               # kann nicht auftreten, aber nie stillschweigen
        raise RuntimeError("Bugfahrwerk: kein Schwenkwinkel raeumt die Haut")
    sweep, wy, wz = best
    _NOSE_BAY.update(sweep_deg=sweep, pivot=piv, axle=axle,
                     wheel=(0.0, wy, wz), r=rn,
                     s0=G.kS0 - (piv[1] + 0.12), s1=G.kS0 - (wy - rn - 0.08),
                     ceiling=wz + rn + 0.04)
    return _NOSE_BAY


def gear(cfg, mat_metal, mat_tire, mat_skin, mat_dark, rect):
    """Radstand 4.0024 m und Bugradstation aus dem Fahrwerksriss, Spur 2.4384 m aus [XML] (vom
    Riss auf 0.09 % bestaetigt), Aufstandsebene -1.81864 m fuer BEIDE Beine (Runde-4-Befund 1
    und 11). Die Registrierungsregel und ihre Begruendung stehen in f16_geometry.py."""
    # Reifen: Ringzahl auf ein VIELFACHES VON 4 aufrunden. Sonst liegt bei ungerader Teilung
    # kein Eckpunkt bei phi = -90 deg, und der polygonale Reifen steht auf seiner Sehne statt
    # auf seinem Scheitel — L2 stand dadurch 11.5 mm hoeher als L0 (Runde-4-Befund 3).
    m = max(cfg["ring"], 9)
    mw = m + (-m) % 4
    gz = G.kGroundZ
    fine = cfg["gear"] >= 2
    out = {}

    def wheel(name, c, r, hw):
        prof = ((-1.00, 0.42), (-0.98, 0.78), (-0.86, 0.980), (-0.35, 1.0), (0.35, 1.0),
                (0.86, 0.980), (0.98, 0.78), (1.00, 0.42))
        return tube(name, [((c[0] + f * hw, c[1], c[2]), (1, 0, 0)) for f, _ in prof],
                    [r * g for _, g in prof], mw, mat_tire, rect)

    def rod(name, p0, p1, r0, r1, mat):
        d = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        return tube(name, [(p0, d), (p1, d)], [r0, r1], max(m // 2, 6), mat, rect)

    # ---- Bugfahrwerk: faehrt NACH HINTEN ein. Beide Beine stehen auf DERSELBEN Ebene gz.
    bay = nose_bay()
    ny = G.kS0 - G.kNoseGearS
    rn = G.kTireNose / 2.0
    top = bay["pivot"]
    axle = bay["axle"]
    meshes = [rod("gear.nose.strut", top, (0.0, ny, gz + rn + 0.42), 0.072, 0.052, mat_metal),
              rod("gear.nose.piston", (0.0, ny, gz + rn + 0.46), axle, 0.042, 0.042, mat_metal),
              wheel("gear.nose.wheel", axle, rn, G.kTireNoseW * 0.5),
              rod("gear.nose.hub", (-0.05, ny, gz + rn), (0.05, ny, gz + rn),
                  0.082, 0.082, mat_metal)]
    if fine:
        meshes += [
            rod("gear.nose.scissor.u", (0.055, ny - 0.075, gz + rn + 0.52),
                (0.030, ny - 0.115, gz + rn + 0.30), 0.014, 0.012, mat_metal),
            rod("gear.nose.scissor.l", (0.030, ny - 0.115, gz + rn + 0.30),
                (0.048, ny - 0.070, gz + rn + 0.09), 0.012, 0.014, mat_metal),
            rod("gear.nose.light", (0.0, ny + 0.10, gz + rn + 0.56),
                (0.0, ny + 0.17, gz + rn + 0.56), 0.055, 0.048, mat_dark)]
    out["gear.nose"] = (top, (1.0, 0.0, 0.0), meshes, [])

    # ---- Hauptfahrwerk: nach VORN, Rad flach.
    ry = G.kTireMain / 2.0
    yw = G.kS0 - G.kMainGearS
    for sgn, tag in ((-1, "l"), (1, "r")):
        xw = sgn * G.kWheelTrack / 2.0
        axle = (xw, yw, gz + ry)
        s_bay = G.kMainGearS - 1.15                       # [SET] Schacht 1.15 m vor dem Rad
        _, _, zbot, _, _, _ = fuse_section(s_bay)
        target = (sgn * 0.40, G.kS0 - s_bay, zbot + 0.30)
        pivot, axis = _retract_axis(axle, target, G.kGearMainSweep)
        knee = (sgn * (abs(xw) - 0.06), yw + 0.26, gz + ry + 0.34)
        legs = [rod("gear.main.%s.strut" % tag, pivot, knee, 0.088, 0.062, mat_metal),
                rod("gear.main.%s.link" % tag, knee, axle, 0.052, 0.046, mat_metal)]
        wh = [wheel("gear.main.%s.wheel" % tag, axle, ry, G.kTireMainW * 0.5)]
        if fine:
            legs.append(rod("gear.main.%s.drag" % tag,
                            (pivot[0] - sgn * 0.06, pivot[1] - 0.12, pivot[2] - 0.08),
                            (sgn * (abs(xw) - 0.10), yw + 0.44, gz + ry + 0.46),
                            0.030, 0.024, mat_metal))
            wh += [
                rod("gear.main.%s.scissor.u" % tag,
                    (xw - sgn * 0.10, yw + 0.30, gz + ry + 0.26),
                    (xw - sgn * 0.13, yw + 0.16, gz + ry + 0.14), 0.013, 0.011, mat_metal),
                rod("gear.main.%s.scissor.l" % tag,
                    (xw - sgn * 0.13, yw + 0.16, gz + ry + 0.14),
                    (xw - sgn * 0.09, yw + 0.02, gz + ry + 0.02), 0.011, 0.013, mat_metal),
                rod("gear.main.%s.brakeline" % tag,
                    (xw - sgn * 0.11, yw + 0.46, gz + ry + 0.40),
                    (xw - sgn * 0.11, yw + 0.05, gz + ry + 0.05), 0.010, 0.010, mat_dark),
                rod("gear.main.%s.hub" % tag, (xw - sgn * 0.06, yw, gz + ry),
                    (xw + sgn * 0.06, yw, gz + ry), 0.115, 0.115, mat_metal)]
        # Knuckle-Achse = Beinlaengsachse. Auf der LINKEN Seite wird sie umgedreht, damit auf
        # BEIDEN Seiten derselbe positive Winkel das Rad flach in den Bauch legt (sonst muesste
        # die Komponententabelle links -90 und rechts +90 fuehren — zwei Zahlen fuer eine Sache).
        leg_dir = tuple(sgn * (axle[i] - pivot[i]) for i in range(3))
        out["gear.main.%s" % tag] = (pivot, axis, legs,
                                     [("gear.main.%s.knuckle" % tag, axle, leg_dir, wh)])

    def door(name, x0, x1, y0, y1, z, camber):
        o = plate(name, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], 0.022, mat_skin,
                  kTile["plain"], camber=camber)
        o.data.transform(Matrix.Translation((0.0, 0.0, z)))
        return o

    # Klappen liegen auf der HAUT, nicht irgendwo im Rumpfinneren: ihre Hoehe kommt aus der
    # oertlichen Unterkante (Bugklappe an der Einlaufverkleidung, Hauptklappen am Rumpfbauch).
    # Die BUGKLAPPE deckt jetzt den gerechneten Schacht (nose_bay), nicht ein 1.24-m-Rechteck
    # um die ausgefahrene Radstation — Runde-4-Befund 6: das eingefahrene Rad lag 0.6 m hinter
    # der alten Klappe.
    y_nd0, y_nd1 = G.kS0 - bay["s0"], G.kS0 - bay["s1"]
    z_nd = min(lerp_table(G.kInletFairingBot, bay["s0"]),
               lerp_table(G.kInletFairingBot, bay["s1"])) + 0.012
    out["gear.door.nose"] = ((0.0, y_nd0, z_nd), (1.0, 0.0, 0.0),
                             [door("gear.door.nose.mesh", -0.26, 0.26,
                                   y_nd0, y_nd1, z_nd, 0.030)], [])
    s_md = G.kMainGearS - 0.55
    z_md = fuse_section(s_md)[2] + 0.012
    for sgn, tag in ((-1, "l"), (1, "r")):
        out["gear.door.main.%s" % tag] = (
            (sgn * 0.30, G.kS0 - s_md + 0.62, z_md), (0.0, 1.0, 0.0),
            [door("gear.door.main.%s.mesh" % tag, min(sgn * 0.30, sgn * 0.96),
                  max(sgn * 0.30, sgn * 0.96), G.kS0 - s_md + 0.62, G.kS0 - s_md - 0.62,
                  z_md, 0.045)], [])
    return out


# ================================================================ Aussenlasten

def rail(cfg, name, xw, mat, rect):
    """LAU-129 Fluegelspitzen-Startschiene. Aussenflaeche auf +-4.7244 m [TO]."""
    m = max(cfg["ring"], 8)
    s_f, s_r = 8.10, 10.83                       # [KH, gemessen: Schiene 2.73 m lang]
    xc = xw - math.copysign(0.075, xw)
    return tube(name, [((xc, G.kS0 - s_f, 0.03), (0, 1, 0)),
                       ((xc, G.kS0 - s_f - 0.30, 0.03), (0, 1, 0)),
                       ((xc, G.kS0 - s_r + 0.20, 0.03), (0, 1, 0)),
                       ((xc, G.kS0 - s_r, 0.03), (0, 1, 0))],
                [0.020, 0.075, 0.078, 0.055], m, mat, rect)


def _fin_box(name, v, mat, rect):
    return orient(build(name, v + [(p[0], p[1], p[2] - 0.006) for p in v],
                        [[3, 2, 1, 0], [4, 5, 6, 7],
                         [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]],
                        [[(rect[0] + 0.05, rect[1] + 0.05)] * 4] * 6, mat, smooth=False))


def sidewinder(cfg, name, xw, mat_body, mat_dark, rect):
    """AIM-9M: 2.85 m lang, 127 mm Zelle, Flossenspannweite 0.63 m.
    [WEB https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104557/aim-9-sidewinder/]"""
    m = max(cfg["ring"], 8)
    ln, r = G.kAim9Len, G.kAim9BodyD * 0.5
    # Station HERGELEITET (Runde-4-Befund 8): der FK haengt so an der Schiene, dass sein Heck
    # buendig mit deren hinterem Ende abschliesst — die Startschiene endet dort, wo der
    # Abschussschuh sie verlaesst. s_f/s_r sind [KH]-Messungen aus rail().
    s_n = 10.83 - ln
    xc = xw - math.copysign(0.075, xw)
    z = 0.03 - 0.075 - r
    y0 = G.kS0 - s_n
    parts = [tube(name + ".body",
                  [((xc, y0, z), (0, 1, 0)), ((xc, y0 - 0.22, z), (0, 1, 0)),
                   ((xc, y0 - ln + 0.10, z), (0, 1, 0)), ((xc, y0 - ln, z), (0, 1, 0))],
                  [0.012, r, r, r * 0.92], m, mat_body, rect)]
    sp_c = 0.5 * G.kAim9CanardSpan - r
    sp_f = 0.5 * G.kAim9FinSpan - r
    for i in range(4):
        a = math.radians(G.kAim9RollDeg + 90.0 * i)
        ca, sa = math.cos(a), math.sin(a)
        for tag, (yt, cr, ct, sp) in (("canard", (y0 - 0.42, 0.26, 0.14, sp_c)),
                                      ("fin", (y0 - ln + 0.34, 0.30, 0.16, sp_f))):
            v = [(xc + ca * r, yt, z + sa * r),
                 (xc + ca * (r + sp), yt - 0.02, z + sa * (r + sp)),
                 (xc + ca * (r + sp), yt - ct, z + sa * (r + sp)),
                 (xc + ca * r, yt - cr, z + sa * r)]
            parts.append(_fin_box("%s.%s.%d" % (name, tag, i), v, mat_dark, rect))
    return parts


def pylon(cfg, name, xw, mat, rect):
    c = wing_chord(abs(xw))
    s_le = G.kWingTeS - c
    ln = 1.55 - 0.075 * abs(xw)
    s_m = s_le + 0.46 * c
    depth, z_w = 0.245, -0.5 * G.kTcRoot * c - 0.005
    n = max(cfg["ring"], 8)
    rings = []
    for i in range(7):
        t = i / 6.0
        z = z_w - depth * t
        half = 0.098 * (1.0 - 0.42 * t ** 2)
        fwd = s_m - 0.5 * ln * (1.0 - 0.30 * t ** 2)
        aft = s_m + 0.5 * ln * (1.0 - 0.16 * t ** 2)
        pos, neg = [], []
        for k in range(n + 1):
            f = k / n
            th, _ = airfoil(0.30, 0.0, 0.35, f)
            y = G.kS0 - (fwd + (aft - fwd) * f)
            pos.append((xw + th * half / 0.15, y, z))
            neg.append((xw - th * half / 0.15, y, z))
        rings.append(pos + neg[-2:0:-1])
    rings.append([(xw, p[1], z_w - depth - 0.03) for p in rings[-1]])
    return grid_mesh(name, rings, True, mat, rect, caps=True)


# ================================================================ Anbauten und Heckrumpf

def block52(cfg, mat_skin, mat_dark, rect):
    out = []
    m = max(cfg["ring"], 8)
    y = G.kS0 - G.kGunPortS
    out.append(tube("gun.port", [((-G.kGunPortY, y + 0.34, G.kGunPortZ), (0, 1, 0)),
                                 ((-G.kGunPortY, y - 0.10, G.kGunPortZ), (0, 1, 0))],
                    [0.088, 0.075], m, mat_skin, rect))
    out.append(tube("gun.muzzle", [((-G.kGunPortY, y + 0.36, G.kGunPortZ), (0, 1, 0)),
                                   ((-G.kGunPortY, y + 0.24, G.kGunPortZ), (0, 1, 0))],
                    [0.052, 0.052], m, mat_dark, rect, cap0=False, inward=True))
    ya = G.kS0 - G.kAlq213S
    out.append(tube("fairing.alq213", [((-0.70, ya + 0.34, -0.42), (0, 1, 0)),
                                       ((-0.74, ya, -0.42), (0, 1, 0)),
                                       ((-0.70, ya - 0.34, -0.42), (0, 1, 0))],
                    [0.030, 0.085, 0.030], m, mat_skin, rect))
    yh = G.kS0 - G.kHtsS
    out.append(tube("mount.hts", [((G.kHtsY, yh + 0.24, G.kHtsZ), (0, 1, 0)),
                                  ((G.kHtsY, yh - 0.24, G.kHtsZ), (0, 1, 0))],
                    [0.055, 0.055], m, mat_skin, rect))
    yb = G.kS0 - G.kBladeDorsalS
    zb = fuse_section(G.kBladeDorsalS)[1]
    o = plate("antenna.uhf", [(0.0, yb + 0.22), (0.0, yb - 0.20),
                              (G.kBladeDorsalH, yb - 0.10), (G.kBladeDorsalH, yb + 0.12)],
              0.026, mat_skin, kTile["plain"])
    o.data.transform(Matrix.Translation((0.0, 0.0, zb - 0.01))
                     @ Matrix.Rotation(math.radians(-90.0), 4, 'Y'))
    out.append(o)
    for i, s in enumerate(G.kBladeVentralS):
        yv = G.kS0 - s
        zv = fuse_section(s)[2]
        o = plate("antenna.iff.%d" % i, [(0.0, yv + 0.16), (0.0, yv - 0.14),
                                         (0.13, yv - 0.07), (0.13, yv + 0.09)],
                  0.022, mat_skin, kTile["plain"])
        o.data.transform(Matrix.Translation((0.0, 0.0, zv + 0.01))
                         @ Matrix.Rotation(math.radians(90.0), 4, 'Y'))
        out.append(o)
    return out


def aft_body(cfg, mat_skin, mat_dark, rect):
    """Runde-3-Befund 7: Fanghaken samt linsenfoermiger Bauchverkleidung, Streuwerfer (links 3,
    rechts 1), JFS-Einlauf und -Auslass, UARRSI-Klappe. Stationen aus [TO] Blatt 3 bzw. dem
    Grundriss [KH] — s. f16_geometry.py."""
    out = []
    m = max(cfg["ring"], 8)

    h0, h1 = G.kHookS
    n = max(cfg["fus_n"] // 8, 6)
    lens = []
    for i in range(n):
        t = i / (n - 1)
        s = h0 + (h1 - h0) * t
        zb = lerp_row(G.kFuseStations, s, 3)
        w = 0.155 * math.sin(math.pi * t) ** 0.6
        d = 0.135 * math.sin(math.pi * t) ** 0.75
        ring = []
        for k in range(m):
            a = TAU * k / m
            ring.append((w * math.cos(a), G.kS0 - s, zb + 0.02 - d * (1.0 + math.sin(a)) * 0.5))
        lens.append(ring)
    lens.insert(0, [(0.02 * p[0], lens[0][0][1] + 0.02, p[2]) for p in lens[0]])
    lens.append([(0.02 * p[0], lens[-1][0][1] - 0.02, p[2]) for p in lens[-1]])
    out.append(grid_mesh("fairing.hook", lens, True, mat_skin, rect))
    zb0 = lerp_row(G.kFuseStations, h0 + 0.15, 3)
    hook = tube("gear.hook.mesh",
                [((0.0, G.kS0 - h0 - 0.15, zb0 - 0.03), (0, 1, 0.10)),
                 ((0.0, G.kS0 - h1 + 0.06, zb0 - 0.10), (0, 1, 0.10)),
                 ((0.0, G.kS0 - h1 - 0.10, zb0 - 0.16), (0, 1, 0.10))],
                [0.045, 0.038, 0.055], max(m // 2, 6), mat_dark, rect)

    d0, d1 = G.kDispenserS
    nl, nr = G.kDispenserCount
    for sgn, cnt in ((-1, nl), (1, nr)):
        for i in range(cnt):
            f0 = d0 + (d1 - d0) * i / max(cnt, 1)
            f1 = d0 + (d1 - d0) * (i + 0.86) / max(cnt, 1)
            a, _, _, zc, _, _ = fuse_section(0.5 * (f0 + f1))
            o = plate("dispenser.%s.%d" % ("l" if sgn < 0 else "r", i),
                      [(0.0, G.kS0 - f0), (0.0, G.kS0 - f1),
                       (0.20, G.kS0 - f1 + 0.02), (0.20, G.kS0 - f0 - 0.02)],
                      0.030, mat_dark, kTile["plain"])
            o.data.transform(Matrix.Translation((sgn * (a - 0.015), 0.0, zc - 0.16))
                             @ Matrix.Rotation(math.radians(sgn * 90.0), 4, 'Y'))
            out.append(o)

    for tag, (j0, j1), dz in (("inlet", G.kJfsInletS, -0.30),
                              ("exhaust", G.kJfsExhaustS, -0.44)):
        a, _, _, zc, _, _ = fuse_section(0.5 * (j0 + j1))
        o = plate("jfs.%s" % tag,
                  [(0.0, G.kS0 - j0), (0.0, G.kS0 - j1), (0.17, G.kS0 - j1), (0.17, G.kS0 - j0)],
                  0.026, mat_dark, kTile["plain"])
        o.data.transform(Matrix.Translation((-(a - 0.012), 0.0, zc + dz))
                         @ Matrix.Rotation(math.radians(-90.0), 4, 'Y'))
        out.append(o)

    u0, u1 = G.kUarrsiS
    zt = fuse_section(0.5 * (u0 + u1))[1]
    o = plate("uarrsi.door", [(-G.kUarrsiHalfW, G.kS0 - u0), (G.kUarrsiHalfW, G.kS0 - u0),
                              (G.kUarrsiHalfW * 0.82, G.kS0 - u1),
                              (-G.kUarrsiHalfW * 0.82, G.kS0 - u1)],
              0.024, mat_skin, kTile["plain"])
    o.data.transform(Matrix.Translation((0.0, 0.0, zt - 0.008)))
    out.append(o)
    return out, hook


# ================================================================ Zusammenbau

def build_lod(cfg, out_dir, keep_blend=False):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    res = cfg["tex"]
    r_fus, r_wl, r_wr = kTile["fus"], kTile["wing_l"], kTile["wing_r"]
    r_tail, r_misc, r_plain = kTile["tail"], kTile["misc"], kTile["plain"]

    fus, fus_nz, fus_rad, rim = fuselage(cfg, None, r_fus)
    tiles = [
        (r_fus, fus_nz, fus_rad, "body", (17, 27)),
        (r_wl, lambda u, v: 1.0 if u < 0.5 else -1.0, None, "wing_l", (9, 13)),
        (r_wr, lambda u, v: 1.0 if u < 0.5 else -1.0, None, "wing_r", (9, 13)),
        (r_tail, lambda u, v: 0.30 if u < 0.5 else -0.30, None, "tail", (7, 9)),
        (r_misc, lambda u, v: 0.0, None, "misc", (8, 11)),
        (r_plain, lambda u, v: 0.0, None, "plain", (5, 7)),
    ]
    base, orm, nrm = paint_atlas(res, tiles, np.random.default_rng(20260803))
    mat_skin = mat_textured("f16_skin", _img("f16_base", base, True),
                            _img("f16_orm", orm, False), _img("f16_nrm", nrm, False))
    if cfg["single_mat"]:
        mat_dark = mat_duct = mat_noz = mat_glass = mat_frame = mat_metal = mat_tire = mat_skin
    else:
        mat_dark = mat_plain("f16_dark", (0.020, 0.021, 0.024), rough=0.85)
        mat_duct = mat_plain("f16_duct", (0.28, 0.29, 0.31), rough=0.62)
        # Kohlefaser-Aussenklappen der -229: schwarz statt metallisch. [WEB usaf-sig FAQ]
        mat_noz = mat_plain("f16_nozzle", (0.075, 0.075, 0.080), rough=0.52, metal=0.35)
        mat_glass = mat_plain("f16_canopy", (0.62, 0.70, 0.64), rough=0.035, alpha=0.16, ior=1.52)
        mat_frame = mat_plain("f16_frame", (0.095, 0.100, 0.110), rough=0.35)
        mat_metal = mat_plain("f16_gear", (0.70, 0.71, 0.72), rough=0.30, metal=0.55)
        mat_tire = mat_plain("f16_tire", (0.042, 0.042, 0.046), rough=0.92)
    fus.data.materials.append(mat_skin)
    fixed = [fus]
    if rim:
        fixed.append(coaming(rim, mat_skin, r_misc))

    st = strake(cfg, mat_skin, r_misc)
    fixed += [st, mirror(st, "strake.r")]
    fixed.append(fin_fillet(cfg, mat_skin, r_misc))

    b2 = G.kSpanRef / 2.0
    wl = wing_panel("wing.l", G.kWingRootY, b2, 0.0, 1.0, cfg["wing_n"], cfg["wing_m"],
                    mat_skin, r_wl)
    fixed += [wl, mirror(wl, "wing.r", r_wl, r_wr)]

    moving = {}
    (y_a, f_a), (y_b, f_b) = G.kLefHinge

    def lef_frac(y):
        return f_a + (f_b - f_a) * (y - y_a) / (y_b - y_a)

    def lef_pt(y):
        c = wing_chord(y)
        return (-y, G.kS0 - (G.kWingTeS - c + lef_frac(y) * c), 0.0)

    lef = wing_panel("ctl.lef.l", G.kLefY[0], b2 * G.kLefY[1], 0.0, lef_frac,
                     max(cfg["wing_n"] // 2, 5), max(cfg["wing_m"] // 2, 11), mat_skin, r_wl)
    p_a, p_b = lef_pt(y_a), lef_pt(y_b)
    hinge(lef, p_a, (p_b[0] - p_a[0], p_b[1] - p_a[1], 0.0))

    fl0 = 1.0 - G.kFlaperonChord

    def flp_pt(y):
        c = wing_chord(y)
        return (-y, G.kS0 - (G.kWingTeS - c + fl0 * c), 0.0)

    flp = wing_panel("ctl.aileron.l", G.kFlaperonY[0], G.kFlaperonY[1], fl0, 1.0,
                     max(cfg["wing_n"] // 2, 5), max(cfg["wing_m"] // 2, 11), mat_skin, r_wl)
    q_a, q_b = flp_pt(G.kFlaperonY[0]), flp_pt(G.kFlaperonY[1])
    hinge(flp, q_a, (q_b[0] - q_a[0], q_b[1] - q_a[1], 0.0))
    moving["ctl.lef.l"] = lef
    moving["ctl.lef.r"] = mirror(lef, "ctl.lef.r", r_wl, r_wr)
    moving["ctl.aileron.l"] = flp
    moving["ctl.aileron.r"] = mirror(flp, "ctl.aileron.r", r_wl, r_wr)

    ht_z = G.kHtPlaneZ
    ht = tailplane("ctl.elevon.l", G.kHtSemi, G.kHtRootChord, G.kHtTipChord, G.kHtSweepLE,
                   G.kHtTeS, 0.042, max(cfg["wing_n"] // 2, 6), max(cfg["wing_m"] // 2, 14),
                   mat_skin, r_tail, z=ht_z)
    hinge(ht, (0.0, G.kS0 - (G.kHtTeS - G.kHtHingeChord * G.kHtRootChord), ht_z), (1.0, 0.0, 0.0))
    moving["ctl.elevon.l"] = ht
    moving["ctl.elevon.r"] = mirror(ht, "ctl.elevon.r")

    fixed.append(fin(cfg, mat_skin, r_tail))
    moving["ctl.rudder"] = rudder(cfg, mat_skin, r_tail)

    s0v, s1v = G.kVentralS0, G.kVentralS1
    z_top = lerp_row(G.kFuseStations, 0.5 * (s0v + s1v), 3)
    dep = z_top - G.kVentralTipZ
    vf = plate("ventral.l", [(0.0, G.kS0 - s0v), (0.0, G.kS0 - s1v),
                             (dep, G.kS0 - s1v + 0.10), (dep, G.kS0 - s0v - 0.62)],
               0.038, mat_skin, r_plain)
    vf.data.transform(Matrix.Translation((-0.62, 0.0, z_top))
                      @ Matrix.Rotation(math.radians(G.kVentralCant), 4, 'Y')
                      @ Matrix.Rotation(math.radians(90.0), 4, 'Y'))
    fixed += [vf, mirror(vf, "ventral.r")]

    fixed += inlet(cfg, mat_skin, mat_duct, r_misc)
    fixed += nozzle(cfg, mat_noz, mat_dark, r_misc)

    s_a, s_b = G.kSpeedbrakeS
    for i, up in enumerate((1, -1)):
        _, zt, zb, _, _, _ = fuse_section(0.5 * (s_a + s_b))
        z = (zt - 0.05) if up > 0 else (zb + 0.05)
        pl = plate("ctl.speedbrake.%d" % i,
                   [(G.kSpeedbrakeInner, G.kS0 - s_a), (G.kSpeedbrakeOuter, G.kS0 - s_a),
                    (G.kSpeedbrakeOuter * 0.92, G.kS0 - s_b),
                    (G.kSpeedbrakeInner * 1.20, G.kS0 - s_b)],
                   0.032, mat_skin, r_plain)
        pl.data.transform(Matrix.Translation((0.0, 0.0, z)))
        hinge(pl, (0.0, G.kS0 - s_a, z), (1.0, 0.0, 0.0))
        moving["ctl.speedbrake.%d" % i] = pl
        moving["ctl.speedbrake.%d" % (i + 2)] = mirror(pl, "ctl.speedbrake.%d" % (i + 2))

    can_fix, can_move = canopy(cfg, mat_glass, mat_frame, r_misc)
    can_pivot = (0.0, G.kS0 - G.kCanopyHingeS, G.kCanopyHingeZ)
    fixed += can_fix
    fixed += cockpit(cfg, mat_dark, mat_frame, mat_glass, r_misc)

    x_rail = G.kSpan / 2.0
    rl = rail(cfg, "rail.wingtip.l", -x_rail, mat_skin, r_misc)
    fixed += [rl, mirror(rl, "rail.wingtip.r")]
    if cfg["missiles"]:
        sw = sidewinder(cfg, "store.aim9.l", -x_rail, mat_skin, mat_dark, r_misc)
        fixed += sw
        fixed += [mirror(o, o.name.replace(".l", ".r", 1)) for o in sw]
    if cfg["gear"] > 0:
        for xi, px in enumerate((1.85, 2.90, 3.80)):
            py = pylon(cfg, "pylon.l.%d" % xi, -px, mat_skin, r_misc)
            fixed += [py, mirror(py, "pylon.r.%d" % xi)]

    if cfg["detail"] >= 1:
        fixed += block52(cfg, mat_skin, mat_dark, r_misc)
        aft, hook_mesh = aft_body(cfg, mat_skin, mat_dark, r_misc)
        fixed += aft
    else:
        hook_mesh = None
    fixed.append(tube("probe.pitot",
                      [((0.0, G.kS0 + 0.02, -0.295), (0, 1, 0)),
                       ((0.0, G.kS0 + G.kProbeLen, -0.300), (0, 1, 0))],
                      [0.026, 0.011], 8 if cfg["detail"] >= 1 else 6, mat_skin, r_misc))

    gears = gear(cfg, mat_metal, mat_tire, mat_skin, mat_dark, r_misc) if cfg["gear"] else {}

    # --- Szenengraph: f16 -> airframe -> {feste Teile, bewegliche Knoten}
    root = bpy.data.objects.new("f16", None)
    bpy.context.collection.objects.link(root)
    body = bpy.data.objects.new("airframe", None)
    bpy.context.collection.objects.link(body)
    body.parent = root
    for o in fixed:
        o.parent = body
    for name, o in moving.items():
        o.name = name
        o.parent = body

    def group(name, pivot, axis, meshes, parent, parent_world=None):
        ax = Vector(axis).normalized()
        up = Vector((0.0, 0.0, 1.0)) if abs(ax.z) < 0.9 else Vector((0.0, 1.0, 0.0))
        ey = ax.cross(up).normalized()
        ez = ax.cross(ey).normalized()
        rot = Matrix((ax, ey, ez)).transposed().to_4x4()
        node = bpy.data.objects.new(name, None)
        bpy.context.collection.objects.link(node)
        node.parent = parent
        world = Matrix.Translation(Vector(pivot)) @ rot
        node.matrix_basis = (parent_world.inverted() @ world) if parent_world else world
        inv = world.inverted()
        for o in meshes:
            o.data.transform(inv)
            o.parent = node
        return node, world

    group("canopy", can_pivot, (1.0, 0.0, 0.0), can_move, body)
    if hook_mesh is not None:
        h0 = G.kHookS[0]
        group("gear.hook", (0.0, G.kS0 - h0 - 0.10,
                            lerp_row(G.kFuseStations, h0 + 0.10, 3) - 0.02),
              (1.0, 0.0, 0.0), [hook_mesh], body)
    retract = {}
    for name, (pivot, axis, meshes, kids) in gears.items():
        node, world = group(name, pivot, axis, meshes, body)
        retract[name] = (node, world, {})
        for kname, kpivot, kaxis, kmeshes in kids:
            knode, kworld = group(kname, kpivot, kaxis, kmeshes, node, world)
            retract[name][2][kname] = (knode, kworld)

    # --- Probe der Fahrwerkskinematik: wo landet das Rad bei vollem Einzug?
    for name, ang in (("gear.nose", nose_bay()["sweep_deg"]), ("gear.main.l", G.kGearMainSweep),
                      ("gear.main.r", G.kGearMainSweep)):
        if name not in retract:
            continue
        node, world, kids = retract[name]
        rot = Matrix.Rotation(math.radians(ang), 4, 'X')
        pts = []
        for o in bpy.data.objects:
            if o.type != 'MESH' or "wheel" not in o.name or not o.name.startswith(name):
                continue
            par = o.parent
            if par is node:
                mw = world @ rot
            else:
                kn = kids.get(par.name)
                if kn is None:
                    continue
                mw = (world @ rot @ par.matrix_basis
                      @ Matrix.Rotation(math.radians(G.kGearMainRoll), 4, 'X'))
            pts += [mw @ v.co for v in o.data.vertices]
        if not pts:
            continue
        xs = [p.x for p in pts]
        ys = [p.y for p in pts]
        zs = [p.z for p in pts]
        s_mid = G.kS0 - 0.5 * (min(ys) + max(ys))
        a_f, zt_f, zb_f, _, _, _ = fuse_section(s_mid)
        # Unter dem Bugschacht liegt die EINLAUFVERKLEIDUNG, nicht die Rumpfkontur — Runde 3
        # mass gegen die falsche Referenz und bekam "innen" bzw. "AUSSEN" ohne Aussagewert.
        if name == "gear.nose":
            zb_f = lerp_table(G.kInletFairingBot, s_mid)
            zt_f = nose_bay()["ceiling"]
        ok = (max(abs(min(xs)), abs(max(xs))) <= a_f + 0.01
              and min(zs) >= zb_f - 0.03 and max(zs) <= zt_f)
        print("  RETRACT %-12s x[%+.3f %+.3f] y[%+.3f %+.3f] z[%+.3f %+.3f] s=%5.2f  "
              "Schacht a=%.3f z[%+.3f %+.3f]  %s"
              % (name, min(xs), max(xs), min(ys), max(ys), min(zs), max(zs), s_mid,
                 a_f, zb_f, zt_f, "innen" if ok else "AUSSEN"))

    # --- Export
    path = os.path.join(out_dir, "f16_%s.glb" % cfg["name"])
    for img in bpy.data.images:
        if img.source == 'GENERATED' and img.size[0]:
            tmp = os.path.join(out_dir, "_tmp_%s.png" % img.name)
            img.filepath_raw = tmp
            img.file_format = 'PNG'
            img.save()
            img.pack()
            try:
                os.remove(tmp)
            except OSError:
                pass
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False,
                              export_yup=True, export_apply=False, export_normals=True,
                              export_texcoords=True, export_materials='EXPORT')

    # NORMALEN: Runde 3 meldete "inverted_normals: []" und verschwieg dabei, dass vier Namen aus
    # der Pruefung gefiltert werden (Runde-4-Befund 9). Gemeldet wird jetzt BEIDES — die Liste
    # der absichtlich nach innen gedrehten Koerper und die Liste der unerwarteten. Eine leere
    # zweite Liste ist erst dann eine Aussage, wenn die erste danebensteht.
    kInward = ("duct", "tub", "muzzle", "inner")
    tris, bb = 0, [[1e9, -1e9], [1e9, -1e9], [1e9, -1e9]]
    mats, feats, bad, inward = set(), {}, [], []
    for o in bpy.data.objects:
        if o.type != 'MESH':
            continue
        o.data.calc_loop_triangles()
        tris += len(o.data.loop_triangles)
        if signed_volume(o.data) < 0.0:
            (inward if any(t in o.name for t in kInward) else bad).append(o.name)
        for ms in o.data.materials:
            if ms:
                mats.add(ms.name)
        area = 0.0
        for t in o.data.loop_triangles:
            a, b, c = (o.matrix_world @ o.data.vertices[i].co for i in t.vertices)
            area += 0.5 * (b - a).cross(c - a).length
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            for i, c in enumerate((p.x, p.y, p.z)):
                bb[i][0] = min(bb[i][0], c)
                bb[i][1] = max(bb[i][1], c)
        # Merkmalsgroesse nach Cauchy: die ueber alle Richtungen GEMITTELTE Schattenflaeche eines
        # konvexen Koerpers ist A/4. Die Kantenlaenge des gleich grossen Quadrats ist sqrt(A/4) —
        # genau das Mass, das entscheidet, ob ein wegfallender Koerper ein Pixel hinterlaesst.
        # Eine Bounding-Box ueberschaetzt duenne Rahmen (Kanzelbuegel) um ein Vielfaches.
        feats[o.name] = math.sqrt(max(area, 1e-9) / 4.0)
    if bad:
        print("  WARN negatives Volumen: %s" % ", ".join(sorted(bad)[:12]))
    if keep_blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "f16_%s.blend" % cfg["name"]))
    print("ASSET f16 %-3s %-16s %8d B %7d Tri %d Mat  x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]"
          % (cfg["name"], os.path.basename(path), os.path.getsize(path), tris, len(mats),
             bb[0][0], bb[0][1], bb[1][0], bb[1][1], bb[2][0], bb[2][1]))
    return dict(lod=cfg["name"], file=os.path.basename(path), triangles=tris,
                bytes=os.path.getsize(path), texture=res, materials=sorted(mats),
                normals=dict(
                    test="Vorzeichen des Divergenzintegrals (signed_volume) je Koerper.",
                    filter=("Koerper, deren Name %s enthaelt, sind ABSICHTLICH nach innen "
                            "gedreht (Kanal, Cockpitwanne, Muendungsrohre, Dueseninneres) — "
                            "ihr negatives Volumen ist die Sollmessung, nicht der Defekt."
                            % " | ".join(kInward)),
                    inward_by_design=sorted(inward),
                    unexpected_inverted=sorted(bad)),
                bbox={k: [round(v[0], 3), round(v[1], 3)] for k, v in zip("xyz", bb)},
                size_m={k: round(v[1] - v[0], 3) for k, v in zip("xyz", bb)},
                _feats=feats)


def switch_table(lods):
    """Runde-3-Befund 6: die Schwellen waren gesetzt, nicht hergeleitet.

    Regel: eine Stufe darf erst fallen, wenn ALLES, was die naechste weglaesst, unter ein Pixel
    faellt. Merkmalsgroesse eines weggelassenen Koerpers = sqrt(Oberflaeche/4): nach Cauchys
    Formel ist A/4 die ueber alle Blickrichtungen gemittelte Schattenflaeche. Reichweite:
        R = Merkmalsgroesse / ifov ,  ifov = 60 deg / 1920 px = 5.454e-4 rad/px
    """
    ifov = math.radians(60.0) / 1920.0
    steps = []
    for i, lod in enumerate(lods):
        if i + 1 >= len(lods):
            steps.append(dict(lod=lod["lod"], drops=[], driver=None,
                              feature_m=None, max_range_m=None,
                              note="letzte Stufe, keine Umschaltweite"))
            continue
        cur, nxt = lod["_feats"], lods[i + 1]["_feats"]
        lost = {k: v for k, v in cur.items() if k not in nxt}
        driver, f = (max(lost, key=lambda k: lost[k]), max(lost.values())) if lost else (None, 0.0)
        # Auch wenn nichts wegfaellt, begrenzt die Texturaufloesung: vier Texel der Kachel
        # entsprechen rund 15.06 m / res * 4 auf der Haut.
        tex_feat = 15.06 / max(lods[i + 1]["texture"], 1) * 4.0
        if tex_feat > f:
            driver, f = "textur %d px" % lods[i + 1]["texture"], tex_feat
        steps.append(dict(lod=lod["lod"],
                          drops=sorted(lost, key=lambda k: -lost[k])[:8],
                          driver=driver, feature_m=round(f, 4),
                          max_range_m=round(f / ifov)))
    # MONOTONIE erzwingen: eine Stufe kann nicht frueher fallen als ihre Vorgaengerin, sonst
    # wird sie nie benutzt. Runde 3 lieferte 108 / 759 / 519 m — L2 war tot. Der laufende
    # Maximalwert ist die richtige Lesart: die Schwelle ist eine UNTERE Schranke je Stufe.
    run = 0.0
    for st in steps:
        if st["max_range_m"] is None:
            continue
        run = max(run, float(st["max_range_m"]))
        st["max_range_m"] = round(run)
    return dict(rule=("Eine Stufe faellt erst, wenn ihr groesstes VERLORENES Merkmal unter ein "
                      "Pixel faellt. Merkmalsgroesse = sqrt(Oberflaeche/4) nach Cauchy (mittlere "
                      "Schattenflaeche eines konvexen Koerpers = A/4). Die Weiten sind die "
                      "Grenzen, ab denen das Umschalten UNSICHTBAR ist; ein Renderer darf aus "
                      "Budgetgruenden frueher schalten und zahlt dafuer den genannten Fehler."),
                silhouette_gate=dict(
                    script="sim/assets/models/check_lod.py",
                    call=("Blender --background --python check_lod.py -- --models "
                          "sim/assets/models --res 1200 --limit 2.0"),
                    metric=("XOR-Flaeche zweier Alpha-Masken bei IDENTISCHER orthografischer "
                            "Kamera, geteilt durch die Flaeche der groeberen Stufe."),
                    limit_pct=2.0,
                    measured_round4_pct={
                        "L0->L1": {"side": 0.12, "top": 0.07, "front": 0.24},
                        "L1->L2": {"side": 0.48, "top": 0.22, "front": 0.68},
                        "L2->L3": {"side": 0.58, "top": 0.46, "front": 0.88},
                        "L0->L3": {"side": 1.00, "top": 0.62, "front": 1.51}},
                    before_round4_pct={"L2->L3": {"side": 25.8, "front": 58.1}}),
                rule_2=("ZWEITE, SCHAERFERE BEDINGUNG (Runde-4-Befund 3): das Kriterium oben "
                        "gilt je Koerper und ist in der SUMME blind. L2->L3 liess 13 "
                        "Fahrwerkskoerper und sechs Pylone fallen, jeden einzeln unter einem "
                        "Pixel, zusammen 25.8 % XOR-Flaeche in der Seiten- und 58.1 % in der "
                        "Frontsilhouette; das Flugzeug schwebte 0.449 m ueber der Piste. Es "
                        "gilt deshalb zusaetzlich: KEINE Stufe darf einen Koerper verlieren, "
                        "der die Silhouette begrenzt. Alle vier Stufen bauen dasselbe "
                        "Teilelager; sie unterscheiden sich in Abtastung und Textur."),
                pixel_angle_rad=round(ifov, 8), steps=steps)


def sidecar(out_dir, lods):
    src_nasa = "NASA TP-1538 Tab.I 'Surface deflection limits' (Seite 49, im Bild gelesen)"
    src_xml = "sim/assets/aircraft/f16/f16.xml"

    def comp(node, reads, lo, hi, src, axis="lokal X"):
        return dict(node=node, reads=reads, axis=axis, limits_deg=[lo, hi], source=src)

    table = switch_table(lods)
    doc = {
        "asset": "f16", "name": "F-16C Block 52", "unit_scale_m": 1.0,
        "variant_source": (src_xml + ":245 <engine file=\"F100-PW-229\"> — Pratt & Whitney, "
                           "also Block 52: NSI-Einlauf und gerade P&W-Duese, nicht MCID/F110."),
        "origin": ("VRP aus " + src_xml + " (<location name=\"VRP\"> -180 0 0 in). Physisch der "
                   "Punkt 0.35 MAC in der Fluegelsehnenebene; die Fahrwerkskontakte des XML "
                   "haengen als Offsets an ihm."),
        "axes": "glTF +Y oben / -Z vorwaerts; gebaut in +X rechts / +Y vorwaerts / +Z oben",
        "tolerance_rule": (
            "Die T.O.-Karte quantisiert auf 0.1 ft = 30.5 mm. Fuer jede [TO]-Zahl gilt deshalb "
            "ihre eigene Aufloesung als Toleranz, nicht die 0.5 % des Baums: auf der 7.8-ft-Spur "
            "sind 0.1 ft bereits 0.39 %."),
        "reference_dimensions_m": {
            "length": round(G.kLength, 4),
            "length_source": ("49 ft 5 in [WEB en.wikipedia.org F-16]; die T.O.-Karte nennt "
                              "49.5 ft = dieselbe Zahl auf 0.1 ft quantisiert; der "
                              "Massstabsbalken des Risses gibt unabhaengig 15.0669 m (0.03 %)."),
            "span_over_rails": round(G.kSpan, 4),
            "span_over_missiles_TO": round(G.kSpanMissiles, 4),
            "span_over_missiles_model": round(
                2.0 * (G.kSpan / 2.0 - 0.075
                       + 0.5 * G.kAim9FinSpan * math.cos(math.radians(G.kAim9RollDeg))), 4),
            "span_over_missiles_note": (
                "Runde 3 publizierte 9.9974 m und baute 9.728 m — die Referenz war nie mit der "
                "Geometrie verrechnet. Der AIM-9 haengt in X-Stellung (die LAU-129 traegt ihn "
                "ueber zwei Schuhe auf dem RUECKEN; in Plus-Stellung stuende dort eine Flosse). "
                "Damit ist die groesste Breite 9.7514 m, also -2.46 % gegen [TO] 32.8 ft. Die "
                "[TO]-Zahl braucht Plus-Stellung (9.939 m) UND die FK-Achse auf der "
                "Schienen-Aussenflaeche (9.989 m); beides widerspricht der Aufhaengung. Die "
                "Frontansicht des Risses ist am Blattrand abgeschnitten und kann nicht "
                "entscheiden. Das Netz baut die Mechanik und traegt die Abweichung."),
            "height_on_gear": round(G.kFinTipZ - G.kGroundZ, 4),
            "height_source": ("Flossenspitze [KH] ueber der Aufstandsebene aus " + src_xml
                              + " (MLG-Kontakt z = -71.6 in). Gegen [TO] 16.7 ft: 5.1 mm."),
            "tail_span": round(G.kTailSpan, 4),
            "wheel_track": round(G.kWheelTrack, 4), "wheel_base": round(G.kWheelBase, 4),
            "gear_rule": (
                "[XML] bestimmt, was die Simulation RECHNET; [KH]/[TO] bestimmen, wie das Netz "
                "AUSSIEHT. Prinzip 1 schuetzt das Flugmodell, nicht das Netz — kein Dreieck hier "
                "geht in eine Kraft ein. Runde 3 hatte 'XML ist massgeblich' geschrieben und die "
                "Regel dann selektiv angewandt (NOSE_LG massgeblich, TOP_VS 'keine Formangabe'). "
                "Die Regel entscheidet jetzt in beide Richtungen, s. die zwei Deltas."),
            "gear_delta_wheelbase": (
                "Netz 4.0024 m aus dem Fahrwerksriss (Reifenmitten 2832.5 / 3490.5 px, "
                "Radomspitze 2040 px), bestaetigt durch [TO] 13.1 ft = 3.9929 m (0.24 %, "
                "innerhalb kToQuantum). [XML] NOSE_LG/MLG ergibt 3.5814 m = -10.5 %; der Fehler "
                "sitzt vollstaendig im Bugbein (Hauptbein trifft den Riss auf 82 mm). Das Netz "
                "folgt dem Riss; das Bugrad steht damit 0.503 m vor dem gerechneten "
                "Kontaktpunkt. Sichtbarer Preis bei statischer Bodenlage: 0.503*tan(0) = 0."),
            "gear_delta_track": (
                "Hier bestaetigt der Riss das XML: Frontansicht, Aussenkanten der Hauptreifen "
                "567.5/602.5 und 968.75/1003.75 px -> Mitten 585.0/986.25 -> 2.4407 m gegen "
                "[XML] 2.4384 m (0.09 %). [TO] 7.8 ft = 2.3774 m liegt 2.6 % daneben und ist mit "
                "kToQuantum = 30.5 mm nicht vereinbar. Die Regel picht also keine Rosinen."),
            "gear_delta_ground_plane": (
                "Beide Beine stehen auf derselben Ebene. Die 0.4 in zwischen NOSE_LG (z=-72.0) "
                "und MLG (z=-71.6) im XML sind die statische Federvorspannung, aus der JSBSim "
                "den Standnickwinkel rechnet — keine Formangabe. Runde 3 hatte sie nachgebaut "
                "und das Bugrad 10.2 mm unter die Piste gestellt.")},
        "reference_wing": {
            "span_ref": G.kSpanRef, "area_m2": G.kWingArea, "mac": G.kMac,
            "root_chord": round(G.kWingRootChord, 4), "tip_chord": round(G.kWingTipChord, 4),
            "taper": round(G.kWingTaper, 5), "sweep_le_deg": round(G.kSweepLE, 3),
            "source": ("NASA TP-1538 Tab.I; Wurzel- und Spitzentiefe daraus EINDEUTIG bestimmt, "
                       "die 40 deg Pfeilung fallen als Probe heraus (39.986 deg). Gegenprobe im "
                       "Grundriss [KH]: Wurzeltiefe 4.9591 m gegen 4.96537 m (0.13 %).")},
        "inlet": {
            "type": "NSI (Normal Shock Inlet), Block 52",
            "mouth_w": round(G.kInletOuterW, 4), "mouth_h": round(G.kInletOuterH, 4),
            "duct_d": round(G.kInletDuctD, 4),
            "derivation": ("MCID amtlich 57.5 x 21 in [WEB rec.models.scale]; die Aenderung war "
                           "eine Verbreiterung bei gleicher Hoehe; Fangflaeche proportional zum "
                           "Auslegungsmassenstrom 254/270 lb/s -> Breite x 0.94074.")},
        "blueprint": {
            "source": ("A.W. Chaustow, Awiazija i Wremja 3/1999 — Titelblatt 'Lockheed Martin "
                       "F-16C Block 52'. NUR die Untersicht und die Frontansicht sind dort als "
                       "Block 50 beschriftet; von ihnen stammten die falschen Maulmasse."),
            "url": "https://drawingdatabase.com/lockheed-martin-f-16c-block-50/",
            "scale_m_per_px": G.kPx,
            "note": ("Massstabsbalken 822 px = 5 m. Runde 2 hatte auf 49.5 ft kalibriert und den "
                     "Riss dadurch um +0.14 %% gestreckt; jede [KH]-Zahl traegt jetzt den "
                     "Korrekturfaktor %.7f." % G.kKh)},
        "lods": [{k: v for k, v in lod.items() if not k.startswith("_")} for lod in lods],
        "lod_switch": table,
        "components": [
            comp("ctl.aileron.l", "fcs/left-aileron-pos-rad", -21.5, 21.5,
                 src_nasa + " 'Ailerons (flaperons) +-21.5'; " + src_xml + " max 0.375 rad",
                 "lokal X = Scharnierlinie bei %.4f c [KH Grundriss]" % (1.0 - G.kFlaperonChord)),
            comp("ctl.aileron.r", "fcs/right-aileron-pos-rad", -21.5, 21.5, "wie links"),
            comp("ctl.elevon.l", "fcs/dht-left-pos-rad", -25.0, 25.0,
                 src_nasa + " 'Horizontal tail symmetric +-25, differential +-5.375'; "
                 + src_xml + " max 0.436 rad. Drehpunkt bei %.2f der Wurzeltiefe ist [SET] — "
                 "der Riss zeichnet die Achse nicht." % G.kHtHingeChord),
            comp("ctl.elevon.r", "fcs/dht-right-pos-rad", -25.0, 25.0, "wie links"),
            comp("ctl.rudder", "fcs/rudder-pos-rad", -30.0, 30.0,
                 src_nasa + " 'Rudder +-30'",
                 "lokal X = Scharnierlinie, aus Finnen-VK/HK hergeleitet (48.5/27.5 deg)"),
            comp("ctl.lef.l", "fcs/lef-pos-deg", -2.0, 25.0,
                 src_nasa + " 'Leading-edge flap 25'; untere Grenze [SET]. ScharnierGERADE aus "
                 "zwei gemessenen Stuetzstellen [KH Grundriss]: 0.1657 c bei y=2.366 m und "
                 "0.2009 c bei y=4.312 m."),
            comp("ctl.lef.r", "fcs/lef-pos-deg", -2.0, 25.0, "wie links"),
            comp("ctl.speedbrake.0..3", "fcs/speedbrake-pos-deg", 0.0, 60.0,
                 src_nasa + " 'Speed brake 60'"),
            comp("gear.nose", "gear/gear-pos-norm", 0.0, round(nose_bay()["sweep_deg"], 2),
                 "Einzug nach HINTEN [WEB baseops.net: 'retracts aft into the wheel well, "
                 "rotating about its trunnion pins']; der BETRAG ist gerechnet, nicht gesetzt: "
                 "kleinster Winkel, bei dem der tiefste Radpunkt 20 mm ueber der oertlichen "
                 "Unterkante der Einlaufverkleidung liegt (build_f16.nose_bay)"),
            comp("gear.main.l", "gear/gear-pos-norm", 0.0, G.kGearMainSweep,
                 "Einzug nach VORN um eine SCHRAEGE Achse; die Achse ist aus dem ausgefahrenen "
                 "und dem eingefahrenen Radmittelpunkt gerechnet (Sehnenformel, _retract_axis), "
                 "nicht gesetzt", "lokal X = gerechnete Schwenkachse"),
            comp("gear.main.l.knuckle", "gear/gear-pos-norm", 0.0, G.kGearMainRoll,
                 "Rad flach in den Bauch [WEB f-16.net: 'the main gear wheel can lie flat "
                 "against the fuselage']", "lokal X = Beinlaengsachse"),
            comp("gear.main.r", "gear/gear-pos-norm", 0.0, G.kGearMainSweep, "wie links"),
            comp("gear.main.r.knuckle", "gear/gear-pos-norm", 0.0, G.kGearMainRoll, "wie links"),
            comp("gear.door.nose", "gear/gear-pos-norm", 0.0, 85.0, "[SET]"),
            comp("gear.door.main.l/.r", "gear/gear-pos-norm", 0.0, 90.0, "[SET]"),
            comp("gear.hook", "gear/tailhook-pos-norm", 0.0, 42.0,
                 "[SET] Winkel; Station und Verkleidung aus [TO] Blatt 3"),
            comp("canopy", "fcs/canopy-pos-norm", 0.0, G.kCanopyOpenDeg,
                 "Winkel GEMESSEN, nicht gesetzt: Hough ueber die untere Haubenkante der "
                 "Offen-Ansicht (290 Spalten) gibt 27.90 +-0.15 deg unter der Waagerechten, die "
                 "geschlossene Bruestung steigt um 1.80 deg -> 29.7 deg, gerundet 30.0. Der "
                 "Knoten traegt NUR noch die bewegliche Haube (s = %.2f .. %.2f). Windschutz "
                 "(%.2f .. %.2f, mit Trennrahmen) und Ruecken-Verkleidung (%.2f .. %.2f) sind "
                 "feste Struktur — Runde 3 riss beim Oeffnen 0.76 m Windschutz und 1.82 m "
                 "Verkleidung mit. Restlage, offen benannt: der Riss zeichnet die Vorderkante "
                 "0.15 m weiter vorn, als eine reine Drehung sie hinbringt; die reale F-16 "
                 "faehrt die Haube beim Entriegeln zusaetzlich nach hinten, und dieser zweite "
                 "Freiheitsgrad hat keine Quelle und bleibt ungebaut."
                 % (G.kCanopyBowS, G.kCanopyGlassRearS, G.kCanopyFrontS, G.kCanopyBowS,
                    G.kCanopyGlassRearS, G.kCanopyRearS)),
            comp("rail.wingtip.l/.r", "(statisch)", 0.0, 0.0,
                 "LAU-129, Aussenflaeche auf +-4.7244 m -> Gesamtspannweite 9.4488 m [TO]", "-"),
            comp("store.aim9.l/.r", "(statisch)", 0.0, 0.0,
                 "AIM-9L/M, %.3f m x %.3f m, Flossen %.3f m in X-Stellung "
                 "[WEB en.wikipedia.org/wiki/AIM-9_Sidewinder, Tabelle 'All-aspect variants']. "
                 "Runde 3 belegte 2.85 m mit der af.mil-Fact-Sheet, die 9 ft 11 in nennt."
                 % (G.kAim9Len, G.kAim9BodyD, G.kAim9FinSpan), "-"),
        ],
        "materials_L0_L1": ["f16_skin (PBR: gebackene Basisfarbe + ORM + Normalen)",
                            "f16_canopy (Glas)", "f16_frame", "f16_duct",
                            "f16_nozzle (Kohlefaser, schwarz — -229-Aussenklappen)",
                            "f16_dark", "f16_gear", "f16_tire"],
        "materials_L2_L3": ["f16_skin"],
        "backface_culling": ("Alle Materialien keulen Rueckseiten. Runde 2 hatte doubleSided auf "
                             "acht Materialien und verbarg damit neun umgestuelpte Koerper."),
        "rule": "Kein Knoten schreibt in die Simulation. Alle Knoten LESEN publizierte Werte.",
    }
    p = os.path.join(out_dir, "f16.asset.json")
    with open(p, "w") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
    print("SIDECAR %s" % p)
    for st in table["steps"]:
        print("  LOD %-3s bis %-8s Merkmal %-22s %s m   %s"
              % (st["lod"], "-" if st["max_range_m"] is None else str(st["max_range_m"]) + " m",
                 str(st["driver"]), st["feature_m"], ",".join(st["drops"][:4])))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--lod", default="")
    ap.add_argument("--blend", action="store_true")
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    levels = [kLod[int(a.lod)]] if a.lod else kLod
    stats = [build_lod(c, a.out, a.blend) for c in levels]
    sidecar(a.out, stats)
    return 0


if __name__ == "__main__":
    sys.exit(main())

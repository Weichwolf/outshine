#!/usr/bin/env python3
"""FlightBox — mig23 (MiG-23MLD "Flogger-K"). Vier LOD-Stufen aus EINER parametrischen Quelle.

    /Applications/Blender.app/Contents/MacOS/Blender --background --python build_mig23.py -- \
        --out mods/f16/src/models/mig23 [--lod 0] [--blend]

WARUM SCHNITTE STATT KOERPERPRIMITIVEN. Ein Jet ist ein Loft: der Rumpf entsteht aus 35
Spantrissen des Dreiseitenrisses, nicht aus einem Zylinder. Was die MiG-23 lesbar macht, ist
genau das, was ein Zylinder nicht hat — die Kastenform zwischen den Einlaeufen, der flache
Ruecken, die duenne Heckroehre unter der hoch sitzenden Flosse.

WARUM DIE SCHWENKKINEMATIK UND NICHT DREI NETZE. doc/body-format.md §2: Knotennamen sind mit
der Physik geteilt. `ctl.wingsweep.l/.r` ist EIN Gelenk mit EINEM Drehpunkt; die drei belegten
Rasten 16/45/72 Grad (und die vierte der MLD bei 33 Grad) sind Winkel an diesem Gelenk, keine
Geometrievarianten. Drei Netze waeren drei Wahrheiten, die auseinanderlaufen koennen.

WARUM KEINE TEXTUR. doc/render/visual-target.md §1: 60 GB/s Bandbreite gegen 2.5..4 TFLOPS ALU.
Bandbreite ist der Mangel. Was diesen Koerper traegt, sind Kanten und Formen — Einlauflippe,
Handschuhsaegezahn, Duesenblaetter, Bauchflosse —, und die sind GEOMETRIE. Sieben PBR-Materialien
ohne ein einziges Bild.

WARUM DIE SILHOUETTE NICHT SPRINGT. Jeder Rumpfschnitt bekommt nicht seinen Nennradius, sondern
den Umkreisradius der gleich-UMFAENGIGEN n-Ecke (G.ring_radius). Nach Cauchy ist die ueber alle
Blickrichtungen gemittelte Schattenbreite eines konvexen Koerpers Umfang/pi — gleicher Umfang
heisst gleiche mittlere Silhouettenbreite auf allen vier Stufen. Der Rest wird gemessen.

KOORDINATEN. Blender (+X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m); der glTF-Export dreht
auf +Y-oben / -Z-vorwaerts. y = 0 ist die Drehzapfenstation, z = 0 die Triebwerksachse.
"""
import argparse
import json
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy                                        # noqa: E402
import numpy as np                                # noqa: E402
import mig23_geometry as G                        # noqa: E402

TAU = math.pi * 2.0
EPS_WELD = 1.0e-5          # 10 um — unter jeder Fertigungstoleranz, ueber float32-Rauschen

kAsset = "mig23"
kPrefix = "mig23"

# detail: 2 = alles, 1 = ohne Kleinstteile (Scherenglieder, Antennen), 0 = zusaetzlich ohne
# Vorfluegel-/Klappenfugen.
#
# WARUM ALLE STUFEN DIESELBEN GROSSKOERPER BAUEN. Die Silhouette ist eine SUMME — die F-16 hat
# das teuer gelernt (19 einzeln unsichtbare Koerper rissen zusammen 58 % aus der Frontsilhouette).
# Hier faellt deshalb NICHTS, was den Umriss begrenzt: Fahrwerk, Pylone, Bauchflosse und
# Leitwerke stehen auf allen vier Stufen. Was faellt, ist Abtastung und Innenleben.
kLod = [
    dict(name="L0", seg=72, fus=4, span=26, af=52, detail=2, single_mat=False),
    dict(name="L1", seg=48, fus=3, span=18, af=34, detail=2, single_mat=False),
    dict(name="L2", seg=32, fus=2, span=12, af=22, detail=1, single_mat=True),
    # L3 BEHAELT detail=1. Der erste Lauf liess dort Stoerklappen, Rampen, Fahrwerksklappen,
    # Strebe und Waermepeiler fallen und mass dafuer 3.95 % Silhouettenaenderung gegen eine
    # 2-%-Grenze — die Summe kleiner Koerper, genau der Fehler, den die F-16 mit 58 %
    # bezahlt hat. Was auf L3 faellt, ist nur noch Abtastung.
    dict(name="L3", seg=22, fus=1, span=8, af=14, detail=1, single_mat=True),
]


# ================================================================ Netzbau

class Mesh:
    """Rohnetz in WELTKOORDINATEN: Punkte, Vielecke, Eck-Normalen. Kein bpy, damit die
    Pruefungen exakt bleiben und von der Knotenhierarchie unabhaengig sind."""

    def __init__(self, name, mat):
        self.name = name
        self.mat = mat
        self.v = []
        self.f = []
        self.n = []

    def add(self, pts, normals):
        i0 = len(self.v)
        self.v += [tuple(float(c) for c in p) for p in pts]
        self.f.append(tuple(range(i0, i0 + len(pts))))
        self.n.append([tuple(float(c) for c in x) for x in normals])
        return self

    def bbox(self):
        a = np.array(self.v)
        return a.min(axis=0), a.max(axis=0)


def _unit(v):
    n = math.sqrt(sum(c * c for c in v))
    return (v[0] / n, v[1] / n, v[2] / n) if n > 1e-15 else (0.0, 0.0, 1.0)


def _face_normal(pts):
    """Newell — robust auch bei nicht ganz ebenen Vielecken."""
    nx = ny = nz = 0.0
    m = len(pts)
    for i in range(m):
        a, b = pts[i], pts[(i + 1) % m]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    return np.array(_unit((nx, ny, nz)))


def signed_volume(me):
    v = 0.0
    for f in me.f:
        p = [np.array(me.v[i]) for i in f]
        for i in range(1, len(p) - 1):
            v += float(np.dot(p[0], np.cross(p[i], p[i + 1])))
    return v / 6.0


def orient(me):
    """Windung deterministisch nach aussen drehen; Eck-Normalen einzeln in den Halbraum der
    Flaechennormale kippen (pauschales Negieren war beim Tank falsch, s. dessen DEFECTS)."""
    if signed_volume(me) < 0.0:
        me.f = [tuple(reversed(f)) for f in me.f]
        me.n = [list(reversed(nn)) for nn in me.n]
    for k, (f, nn) in enumerate(zip(me.f, me.n)):
        fn = _face_normal([me.v[i] for i in f])
        me.n[k] = [tuple(-np.array(x)) if float(np.dot(x, fn)) < 0.0 else x for x in nn]
    return me


def tube(name, mat, rings, cap0=True, cap1=True, smooth=True, cyclic=False):
    """Schlauch aus gleich langen Punktringen. Jeder Ring ist eine geschlossene Schleife;
    r=0-Ringe (Pole) werden zu einem Punkt kollabiert. Geschlossen und mannigfaltig."""
    me = Mesh(name, mat)
    n = len(rings[0])
    # Eck-Normalen: quer zum Ring gemittelt (glatt), laengs nicht (Kanten bleiben Kanten),
    # ausser smooth=False.
    def rn(k, i):
        p = np.array(rings[k][i])
        a = np.array(rings[k][(i + 1) % n]) - p
        b = np.array(rings[k][(i - 1) % n]) - p
        c = np.array(rings[min(k + 1, len(rings) - 1)][i]) - p
        d = np.array(rings[max(k - 1, 0)][i]) - p
        u = np.cross(a - b, c - d)
        return _unit(u) if np.linalg.norm(u) > 1e-12 else (0.0, 0.0, 1.0)

    nrm = [[rn(k, i) for i in range(n)] for k in range(len(rings))] if smooth else None
    for k in range(len(rings) - (0 if cyclic else 1)):
        A, B = rings[k], rings[(k + 1) % len(rings)]
        degA = all(abs(A[0][j] - A[i][j]) < 1e-12 for i in range(n) for j in range(3))
        degB = all(abs(B[0][j] - B[i][j]) < 1e-12 for i in range(n) for j in range(3))
        for i in range(n):
            j = (i + 1) % n
            if degA and degB:
                continue
            quad = [A[i], A[j], B[j], B[i]]
            if degA:
                quad = [A[i], B[j], B[i]]
            elif degB:
                quad = [A[i], A[j], B[i]]
            if smooth:
                k1 = (k + 1) % len(rings)
                nn = [nrm[k][i], nrm[k][j], nrm[k1][j], nrm[k1][i]]
                if degA:
                    nn = [nrm[k][i], nrm[k1][j], nrm[k1][i]]
                elif degB:
                    nn = [nrm[k][i], nrm[k][j], nrm[k1][i]]
            else:
                fn = _face_normal(quad)
                nn = [fn] * len(quad)
            me.add(quad, nn)
    for k, want in ((0, cap0 and not cyclic), (len(rings) - 1, cap1 and not cyclic)):
        if not want:
            continue
        R = rings[k]
        if all(abs(R[0][j] - R[i][j]) < 1e-12 for i in range(n) for j in range(3)):
            continue
        loop = list(R) if k else list(reversed(R))
        c = tuple(sum(p[j] for p in R) / n for j in range(3))
        fn = _face_normal(loop)
        for i in range(n):
            me.add([c, loop[i], loop[(i + 1) % n]], [fn, fn, fn])
    return orient(me)


def solid(name, mat, loops, smooth=False):
    """Wie tube, aber flach schattiert — fuer kantige Koerper (Pylone, Klappen, Kaesten)."""
    return tube(name, mat, loops, smooth=smooth)


def box(name, mat, lo, hi):
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    a = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]
    b = [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    return solid(name, mat, [a, b])


def ring(n, cx, cy, r, z, phase=0.0, ax=0, ay=1, az=2):
    """n-Eck in der Ebene, Achsenzuordnung frei (fuer Ringe quer zur y-Achse usw.)."""
    out = []
    for i in range(n):
        t = phase + TAU * i / n
        p = [0.0, 0.0, 0.0]
        p[ax] = cx + r * math.cos(t)
        p[ay] = cy + r * math.sin(t)
        p[az] = z
        out.append(tuple(p))
    return out


# ---------------------------------------------------------------- Profilschnitte

def mirror_name(n):
    """Steuerbordname -> Backbordname. Ersetzt das Seitenzeichen, egal wo es im Namen steht:
    `wing.r` -> `wing.l`, `gear.main.r.strut` -> `gear.main.l.strut`."""
    t = n.split(".")
    return ".".join("l" if x == "r" else x for x in t)


def mirror(me, name):
    """Exakte Spiegelung an x = 0. Kein zweiter Bauweg fuer die linke Seite — der Tank hat
    gezeigt, was zwei Wege kosten, und die Symmetriepruefung am Netz fand hier auf Anhieb
    352 Punkte ohne Spiegelbild (die Rundstaebe waehlten ihren Referenzvektor seitenabhaengig).
    Eine Spiegelung kann das nicht."""
    out = Mesh(name, me.mat)
    out.v = [(-x, y, z) for x, y, z in me.v]
    out.f = [tuple(reversed(f)) for f in me.f]
    out.n = [[(-a, b, c) for a, b, c in reversed(nn)] for nn in me.n]
    return out


def naca_t(x, t):
    """Halbe Dicke des symmetrischen NACA-4-Ziffern-Profils an der Sehnenstelle x."""
    x = min(max(x, 0.0), 1.0)
    return 5.0 * t * (0.2969 * math.sqrt(x) - 0.1260 * x - 0.3516 * x * x
                      + 0.2843 * x ** 3 - 0.1015 * x ** 4)


def prof_seg(t, f0, f1, n):
    """Geschlossene Profilschleife zwischen den Sehnenanteilen f0 und f1 (Kosinusverteilung).
    Wo die Dicke null wird (Nasenpunkt), faellt der doppelte Punkt weg — und NUR dort.

    Die erste Fassung liess den unteren HINTERKANTENpunkt mit weg. Ein NACA-Profil ist an der
    Hinterkante nicht dick null (1.05 % t), also fehlte dem oberen HK-Punkt sein Spiegelbild:
    die Symmetriepruefung am Netz meldete 138 Punkte ohne Partner, alle in Flosse und Ruder,
    und wies damit auf einen Profilfehler statt auf einen Seitenfehler.
    """
    half = max(3, n // 2)
    xs = [f0 + (f1 - f0) * 0.5 * (1.0 - math.cos(math.pi * i / half)) for i in range(half + 1)]
    up = [(x, naca_t(x, t)) for x in xs]
    lo = [(x, -naca_t(x, t)) for x in reversed(xs)]
    if abs(up[0][1]) < 1e-9:
        lo = lo[:-1]
    if abs(up[-1][1]) < 1e-9:
        lo = lo[1:]
    return up + lo


def superellipse(n, w, at, ab, ne_t, ne_b, zc):
    """Rumpfschnitt: halbe Breite w, Hoehe oben at / unten ab, Formexponenten ne.
    Punkt 0 liegt OBEN, danach im Uhrzeigersinn ueber Steuerbord."""
    out = []
    for i in range(n):
        phi = TAU * i / n
        cx, cy = math.sin(phi), math.cos(phi)
        ne = ne_t if cy >= 0.0 else ne_b
        a = at if cy >= 0.0 else ab
        e = 2.0 / ne
        x = w * math.copysign(abs(cx) ** e, cx)
        z = zc + a * math.copysign(abs(cy) ** e, cy)
        out.append((x, 0.0, z))
    return out


def _interp(knots, x):
    """Lineare Interpolation ueber eine nach x sortierte Knotenliste (x, *werte)."""
    if x <= knots[0][0]:
        return knots[0][1:]
    if x >= knots[-1][0]:
        return knots[-1][1:]
    for i in range(len(knots) - 1):
        a, b = knots[i], knots[i + 1]
        if a[0] <= x <= b[0]:
            f = (x - a[0]) / (b[0] - a[0])
            return tuple(a[j + 1] + f * (b[j + 1] - a[j + 1]) for j in range(len(a) - 1))
    return knots[-1][1:]


def _spline(ts, vals, t):
    """Monotone Catmull-Rom-Auswertung ueber (ts, vals). ts aufsteigend."""
    if t <= ts[0]:
        return vals[0]
    if t >= ts[-1]:
        return vals[-1]
    i = max(1, min(len(ts) - 3, int(np.searchsorted(ts, t)) - 1))
    p0, p1, p2, p3 = vals[i - 1], vals[i], vals[i + 1], vals[i + 2]
    u = (t - ts[i]) / (ts[i + 1] - ts[i])
    return (0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u
                   + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3))


# ================================================================ Selbstpruefungen (doc/assets.md §3.1)

def _edges(me):
    d = {}
    for fi, f in enumerate(me.f):
        for i in range(len(f)):
            d.setdefault((f[i], f[(i + 1) % len(f)]), []).append(fi)
    return d


def check_body(me, tj_limit=200000):
    """Alle Invarianten eines Koerpers. Liste von Befunden (leer = sauber) plus Kennzahlen."""
    bad = []
    v = np.array(me.v)
    key = np.round(v / EPS_WELD).astype(np.int64)
    uniq, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    inv = np.asarray(inv).reshape(-1)
    dup = int((cnt > 1).sum())

    de = _edges(me)
    multi = [e for e, fs in de.items() if len(fs) > 1]
    wd = {}
    for e, fs in de.items():
        k = (int(inv[e[0]]), int(inv[e[1]]))
        wd[k] = wd.get(k, 0) + len(fs)
    boundary = [e for e in wd if (e[1], e[0]) not in wd]
    nonmanifold = [e for e, c in wd.items() if c != 1]
    if multi:
        bad.append("gerichtete Kante mehrfach: %d" % len(multi))
    if boundary:
        bad.append("Randkanten (Loch): %d" % len(boundary))
    if nonmanifold:
        bad.append("nicht-mannigfaltige Kanten: %d" % len(nonmanifold))

    nv, ne, nf = len(uniq), len(wd) // 2, len(me.f)
    chi = nv - ne + nf
    genus = (2 - chi) / 2.0
    if genus < 0 or abs(genus - round(genus)) > 1e-9:
        bad.append("Euler-Charakteristik %d -> Geschlecht %.1f" % (chi, genus))

    worst_len, worst_dot = 0.0, 1.0
    for f, nn in zip(me.f, me.n):
        fn = _face_normal([me.v[i] for i in f])
        for x in nn:
            a = np.array(x)
            if not np.all(np.isfinite(a)):
                bad.append("NaN in Normale")
                break
            worst_len = max(worst_len, abs(float(np.linalg.norm(a)) - 1.0))
            worst_dot = min(worst_dot, float(np.dot(a, fn)))
    if worst_len > 1e-6:
        bad.append("Normale nicht Einheitslaenge (max Abw. %.2e)" % worst_len)
    if worst_dot < 0.0:
        bad.append("Eck-Normale gegen die Flaechennormale (min cos %.3f)" % worst_dot)

    degen = 0
    for f in me.f:
        p = [np.array(me.v[i]) for i in f]
        a = 0.0
        for i in range(1, len(p) - 1):
            a += 0.5 * float(np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])))
        if a < 1e-14:
            degen += 1
    if degen:
        bad.append("entartete Vielecke: %d" % degen)

    # T-Stoesse: ein Punkt im INNEREN einer Kante, an der er nicht haengt. Der Test ist
    # O(Kanten * Punkte); bei den grossen Loft-Koerpern wird er ueber ein Gitter beschleunigt
    # statt weggelassen — ein weggelassener Test ist kein Test.
    pos = uniq.astype(np.float64) * EPS_WELD
    tj = 0
    seen = set()
    cell = max(EPS_WELD * 4.0, 0.02)
    grid = {}
    for i, p in enumerate(pos):
        grid.setdefault((int(p[0] // cell), int(p[1] // cell), int(p[2] // cell)), []).append(i)
    for (a, b) in wd:
        if (b, a) in seen:
            continue
        seen.add((a, b))
        p0, p1 = pos[a], pos[b]
        d = p1 - p0
        ln2 = float(np.dot(d, d))
        if ln2 < 1e-18:
            continue
        lo = np.minimum(p0, p1) - cell
        hi = np.maximum(p0, p1) + cell
        cand = []
        for cx in range(int(lo[0] // cell), int(hi[0] // cell) + 1):
            for cy in range(int(lo[1] // cell), int(hi[1] // cell) + 1):
                for cz in range(int(lo[2] // cell), int(hi[2] // cell) + 1):
                    cand += grid.get((cx, cy, cz), [])
        if not cand:
            continue
        q = pos[cand]
        t = ((q - p0) @ d) / ln2
        m = (t > 1e-6) & (t < 1.0 - 1e-6)
        if not m.any():
            continue
        proj = p0 + np.outer(t[m], d)
        tj += int((np.linalg.norm(q[m] - proj, axis=1) < EPS_WELD * 2.0).sum())
        if tj > tj_limit:
            break
    if tj:
        bad.append("T-Stoesse: %d" % tj)

    vol = signed_volume(me)
    if vol <= 0.0:
        bad.append("Volumen %.4e <= 0 (Normalen nach innen)" % vol)
    return bad, dict(verts=nv, raw_verts=len(me.v), edges=ne, faces=nf, chi=chi,
                     genus=int(round(genus)), volume=vol, duplicates=dup)


def _tris(me):
    v = np.array(me.v)
    out = []
    for f in me.f:
        for i in range(1, len(f) - 1):
            out.append((f[0], f[i], f[i + 1]))
    return v, np.array(out, dtype=np.int64)


def _cross(a, b):
    ax, ay, az = a[..., 0], a[..., 1], a[..., 2]
    bx, by, bz = b[..., 0], b[..., 1], b[..., 2]
    return np.stack([ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx], axis=-1)


kRayDir = np.array([1.0, 0.0137, 0.0071])
kRayDir = kRayDir / np.linalg.norm(kRayDir)


def _cull(v, tri, lo, hi, d):
    tlo = np.minimum(np.minimum(v[tri[:, 0]], v[tri[:, 1]]), v[tri[:, 2]])
    thi = np.maximum(np.maximum(v[tri[:, 0]], v[tri[:, 1]]), v[tri[:, 2]])
    span = float(np.max(thi - tlo + (v.max(0) - v.min(0))))
    slo = np.minimum(lo, lo + d * span) - 1e-6
    shi = np.maximum(hi, hi + d * span) + 1e-6
    m = (thi >= slo).all(1) & (tlo <= shi).all(1)
    return tri[m]


def inside(pts, v, tri):
    """Punkte im Inneren eines geschlossenen Netzes: schraeger Strahl, Kreuzungen zaehlen."""
    if not len(tri) or not len(pts):
        return np.zeros(len(pts), dtype=bool)
    a, b, c = v[tri[:, 0]], v[tri[:, 1]], v[tri[:, 2]]
    e1, e2 = b - a, c - a
    d = kRayDir
    h = _cross(np.broadcast_to(d, e2.shape), e2)
    det = (e1 * h).sum(1)
    ok = np.abs(det) > 1e-14
    inv = np.zeros_like(det)
    inv[ok] = 1.0 / det[ok]
    hit = np.zeros(len(pts), dtype=np.int64)
    chunk = max(1, int(4e6 // max(len(tri), 1)))
    for i0 in range(0, len(pts), chunk):
        s = pts[i0:i0 + chunk, None, :] - a[None, :, :]
        u = (s * h).sum(2) * inv
        q = _cross(s, np.broadcast_to(e1, s.shape))
        vv = (q * d).sum(2) * inv
        t = (e2 * q).sum(2) * inv
        m = ok & (u >= 0.0) & (u <= 1.0) & (vv >= 0.0) & (u + vv <= 1.0) & (t > 1e-9)
        hit[i0:i0 + chunk] = m.sum(1)
    return (hit % 2) == 1


# ---------------------------------------------------------------- Erklaerte Durchdringungen
#
# EIN FLUGZEUG IST KEIN TANK. Beim Tank war geteiltes Volumen die Ausnahme (Schweisspunkte,
# Kappe 10 %). Hier ist es die REGEL: ein Fluegelholm steckt im Rumpf, ein Fahrwerksbein im
# Schacht, eine Flosse im Ruecken. Die Liste sagt deshalb nicht "Naht", sondern ANSCHLUSS, und
# jede Zeile traegt ihre eigene Kappe als Anteil des KLEINEREN Koerpers. Was nicht in der Liste
# steht, ist weiterhin ein Defekt — die Kappe ist eine Aussage, keine Blankovollmacht.
kJoint = (
    ("wing.", "glove.", 0.45),
    ("ctl.slat.", "wing.", 0.30),
    ("ctl.flap.", "wing.", 0.30),
    ("ctl.spoiler.", "wing.", 0.60),
    ("glove.", "fus.body", 0.45),
    ("glove.", "inlet.", 0.35),
    ("inlet.", "fus.body", 0.55),
    ("inlet.ramp.", "fus.body", 0.60),
    ("inlet.ramp.", "inlet.duct", 0.60),
        ("fin", "fus.body", 0.55),
    ("ctl.rudder", "fin", 0.35),
    ("fin.chute", "fin", 0.50),
    ("fin.chute", "fus.body", 0.50),
    ("ctl.taileron.", "fus.body", 0.45),
    ("ctl.ventral", "fus.body", 0.35),
    ("canopy.glass", "fus.body", 0.60),
    ("canopy.frame", "fus.body", 0.35),
    ("canopy.frame", "canopy.glass", 0.60),
    ("boom.pitot", "fus.body", 0.90),
    ("nozzle", "fus.body", 0.90),
    ("gear.", "fus.body", 0.60),
    ("gear.", "glove.", 0.60),
    ("gear.", "gear.", 0.75),
    ("pylon.", "fus.body", 0.50),
    ("pylon.", "glove.", 0.55),
    ("gun.", "fus.body", 0.60),
    ("sensor.", "fus.body", 0.60),
    ("ctl.airbrake.", "fus.body", 0.45),
    ("ctl.airbrake.", "ctl.ventral", 0.45),
    ("antenna.", "fin", 0.70),
    ("antenna.", "ctl.rudder", 0.40),
    ("antenna.", "fus.body", 0.70),
)

# [SET] Schwelle des URTEILS. Darunter ist ein Detektortreffer eine Beruehrung an der
# Messaufloesung, kein Defekt. Sie ENTFERNT nichts aus dem Bericht.
kOverlapMinCm3 = 25.0

# [SET] Kleinster Halbmesser eines Rumpfschnitts. 6 mm — 600-fach ueber EPS_WELD.
kMinSection = 0.006


def joint_cap(a, b):
    """Erklaerte Kappe fuer dieses Paar, oder None."""
    best = None
    for x, y, cap in kJoint:
        if (a.startswith(x) and b.startswith(y)) or (a.startswith(y) and b.startswith(x)):
            best = cap if best is None else max(best, cap)
    return best


def _body_edges(me):
    e = set()
    for f in me.f:
        for i in range(len(f)):
            a, b = f[i], f[(i + 1) % len(f)]
            e.add((a, b) if a < b else (b, a))
    return np.array(sorted(e), dtype=np.int64)


def _seg_hits_tri(p0, p1, v, tri):
    if not len(tri) or not len(p0):
        return False
    a, b, c = v[tri[:, 0]], v[tri[:, 1]], v[tri[:, 2]]
    e1, e2 = b - a, c - a
    chunk = max(1, int(2e6 // max(len(tri), 1)))
    for k in range(0, len(p0), chunk):
        d = p1[k:k + chunk] - p0[k:k + chunk]
        h = _cross(d[:, None, :], e2[None, :, :])
        det = (e1[None, :, :] * h).sum(2)
        ok = np.abs(det) > 1e-15
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        sv = p0[k:k + chunk, None, :] - a[None, :, :]
        u = (sv * h).sum(2) * inv
        q = _cross(sv, np.broadcast_to(e1[None, :, :], sv.shape))
        w = (d[:, None, :] * q).sum(2) * inv
        t = (e2[None, :, :] * q).sum(2) * inv
        m = ok & (u >= 0) & (u <= 1) & (w >= 0) & (u + w <= 1) & (t > 1e-9) & (t < 1.0 - 1e-9)
        if m.any():
            return True
    return False


def _grid_volume(v1, t1, v2, t2, lo, hi, n):
    d = hi - lo
    g = [np.linspace(lo[k] + d[k] / (2 * n), hi[k] - d[k] / (2 * n), n) for k in range(3)]
    pts = np.stack(np.meshgrid(*g, indexing="ij"), -1).reshape(-1, 3)
    c1 = _cull(v1, t1, lo, hi, kRayDir)
    c2 = _cull(v2, t2, lo, hi, kRayDir)
    if not len(c1) or not len(c2):
        return 0.0
    m = inside(pts, v1, c1)
    if not m.any():
        return 0.0
    return float(np.prod(d)) * int(inside(pts[m], v2, c2).sum()) / len(pts)


def check_overlap(bodies, vol_cm3, n0=10, nmax=20, rtol=0.10):
    """doc/assets.md §3.1 "mesh merge", in der Fassung, die MESSEN kann.

    ZWEI STUFEN: (1) EXAKTER Detektor (Kante gegen Dreieck, Moeller-Trumbore, plus
    Vollumschliessung), (2) konvergenter Gitterschaetzer NUR wo der Detektor Ja sagt, mit
    verdoppelnder Aufloesung bis rtol. Gemeldet wird der Wert MIT seiner Konvergenz.
    """
    prep = []
    for nm, me in bodies:
        v, t = _tris(me)
        prep.append((nm, v, t, _body_edges(me), v.min(0), v.max(0)))
    hits = []
    for i in range(len(prep)):
        n1, v1, t1, e1, lo1, hi1 = prep[i]
        for j in range(i + 1, len(prep)):
            n2, v2, t2, e2, lo2, hi2 = prep[j]
            lo, hi = np.maximum(lo1, lo2), np.minimum(hi1, hi2)
            if (hi - lo <= 1e-9).any():
                continue
            def _tmask(v, t):
                return t[((np.maximum.reduce([v[t[:, k]] for k in range(3)]) >= lo).all(1)
                          & (np.minimum.reduce([v[t[:, k]] for k in range(3)]) <= hi).all(1))]

            c1, c2 = _tmask(v1, t1), _tmask(v2, t2)

            def _emask(v, e):
                a, b = v[e[:, 0]], v[e[:, 1]]
                return (np.maximum(a, b) >= lo).all(1) & (np.minimum(a, b) <= hi).all(1)

            m1, m2 = _emask(v1, e1), _emask(v2, e2)
            touch = (_seg_hits_tri(v1[e1[m1, 0]], v1[e1[m1, 1]], v2, c2)
                     or _seg_hits_tri(v2[e2[m2, 0]], v2[e2[m2, 1]], v1, c1))
            if not touch:
                cand = v1[(v1 >= lo).all(1) & (v1 <= hi).all(1)][:64]
                if not len(cand) or not inside(cand, v2, _cull(v2, t2, lo, hi, kRayDir)).any():
                    continue
            cap = joint_cap(n1, n2)
            box = float(np.prod(hi - lo)) * 1e6
            vmin = min(vol_cm3.get(n1, 0.0), vol_cm3.get(n2, 0.0))
            # Stufe 1.5, STRENGER als der Schaetzer: das gemeinsame Volumen kann den
            # Schnittquader nicht ueberschreiten. Liegt schon der unter der erklaerten Kappe,
            # ist sie BEWIESEN und die teure Gitterprobe entfaellt.
            if cap is not None and box <= cap * vmin:
                hits.append((n1, n2, box, 0, 0.0))
                continue
            n, prev, seq = n0, None, []
            while True:
                vol = _grid_volume(v1, t1, v2, t2, lo, hi, n)
                seq.append((n, vol * 1e6))
                if prev is not None and abs(vol - prev) <= rtol * max(vol, 1e-12):
                    break
                if n >= nmax:
                    break
                prev, n = vol, n * 2
            cm3 = seq[-1][1]
            rel = (abs(seq[-1][1] - seq[-2][1]) / max(seq[-1][1], 1e-9)) if len(seq) > 1 else 1.0
            hits.append((n1, n2, cm3, n, rel))
    return sorted(hits, key=lambda x: -x[2])


# ================================================================ Die AUSGELIEFERTE Szene lesen
#
# WARUM DAS HIER STEHT UND NICHT DIE NETZE VON VORHIN GEMESSEN WERDEN. Runde 1 mass am
# gebauten Netz — und genau in dem Spalt zwischen "gebautes Netz" und "was in der Datei
# steht" sass ihr schwerster Fehler: die Knotenmatrizen entstehen NACH den Netzen, und ein
# Vorzeichenfehler in ihnen stellte sechs Koerper bis 3.3 m neben das Flugzeug, waehrend elf
# Masse gruen meldeten und die Bodenfreiheit 0.000 m. Die Regel lautet deshalb nicht mehr
# "miss am gebauten Netz", sondern MISS AN DER AUSGELIEFERTEN SZENE: .glb einlesen,
# Knotenhierarchie aufloesen, danach messen. Der Leser hier ist bewusst EIGENSTAENDIG (kein
# bpy-Reimport), sonst teilte er einen Fehler mit dem Exporteur, der ihn erzeugt hat.

_kGltfComp = {5120: 'i1', 5121: 'u1', 5122: 'i2', 5123: 'u2', 5125: 'u4', 5126: 'f4'}
_kGltfCount = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

# glTF (+Y oben, -Z vorwaerts) -> Bauframe (+X rechts, +Y vorwaerts, +Z oben)
kGltfToBuild = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]])


def _gltf_accessor(doc, blob, i):
    a = doc["accessors"][i]
    n = _kGltfCount[a["type"]]
    dt = np.dtype("<" + _kGltfComp[a["componentType"]])
    bv = doc["bufferViews"][a["bufferView"]]
    off = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = bv.get("byteStride", dt.itemsize * n)
    if stride == dt.itemsize * n:
        return np.frombuffer(blob, dtype=dt, count=a["count"] * n,
                             offset=off).reshape(a["count"], n)
    raw = np.frombuffer(blob, dtype=np.uint8, count=a["count"] * stride, offset=off)
    return raw.reshape(a["count"], stride)[:, :dt.itemsize * n].copy().view(dt)


def _gltf_local(nd):
    if "matrix" in nd:
        return np.array(nd["matrix"], dtype=np.float64).reshape(4, 4).T
    m = np.eye(4)
    if "rotation" in nd:
        x, y, z, w = nd["rotation"]
        m[:3, :3] = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    if "scale" in nd:
        m[:3, :3] = m[:3, :3] @ np.diag(nd["scale"])
    if "translation" in nd:
        m[:3, 3] = nd["translation"]
    return m


def read_glb(path):
    """(Knotenweltmatrizen, Koerper) aus der FERTIGEN Datei, alles im Bauframe."""
    raw = open(path, "rb").read()
    doc, blob, off = None, None, 12
    while off < len(raw):
        ln, ty = struct.unpack_from("<II", raw, off)
        if ty == 0x4E4F534A:
            doc = json.loads(raw[off + 8:off + 8 + ln].decode("utf-8"))
        elif ty == 0x004E4942:
            blob = raw[off + 8:off + 8 + ln]
        off += 8 + ln + ((-ln) % 4)
    nds = doc["nodes"]
    parent = {c: i for i, nd in enumerate(nds) for c in nd.get("children", [])}
    world = {}

    def wm(i):
        if i not in world:
            world[i] = (_gltf_local(nds[i]) if i not in parent
                        else wm(parent[i]) @ _gltf_local(nds[i]))
        return world[i]

    r4 = np.eye(4)
    r4[:3, :3] = kGltfToBuild
    r4i = np.linalg.inv(r4)
    node_world, bodies = {}, []
    for i, nd in enumerate(nds):
        m = wm(i)
        name = nd.get("name", "node%d" % i)
        node_world[name] = r4 @ m @ r4i
        if "mesh" not in nd:
            continue
        me = Mesh(name, None)
        base = 0
        for p in doc["meshes"][nd["mesh"]]["primitives"]:
            v = _gltf_accessor(doc, blob, p["attributes"]["POSITION"]).astype(np.float64)
            v = ((m[:3, :3] @ v.T).T + m[:3, 3]) @ kGltfToBuild.T
            idx = _gltf_accessor(doc, blob, p["indices"]).astype(np.int64).reshape(-1, 3)
            me.v += [tuple(x) for x in v]
            me.f += [tuple(t) for t in (idx + base)]
            base += len(v)
        bodies.append(me)
    return node_world, bodies


def check_placement(bodies, nodes, node_world, scene):
    """Steht in der DATEI, was gebaut wurde? Zwei Fragen, beide exakt beantwortbar.

    (1) Traegt jeder Gelenkknoten die Weltmatrix aus node_table()? (2) Deckt sich die
    Punktwolke jedes Koerpers in der Datei mit der gebauten? Toleranz 1 mm bzw. 0.05 Grad —
    weit ueber dem float32-Ruecklauf (~1e-6 m) und weit unter jedem Platzierungsfehler.
    """
    fails, rows = [], []
    worst_o, worst_a, worst_o_n, worst_a_n = 0.0, 0.0, "-", "-"
    for nd in nodes:
        got = node_world.get(nd["node"])
        if got is None:
            fails.append("Platzierung: Knoten %s fehlt in der Datei" % nd["node"])
            continue
        want = _mat4(nd["origin"], nd["axis"])
        do = float(np.linalg.norm(got[:3, 3] - want[:3, 3]))
        da = math.degrees(math.acos(max(-1.0, min(1.0, float(np.dot(got[:3, 0], want[:3, 0]))))))
        if do > worst_o:
            worst_o, worst_o_n = do, nd["node"]
        if da > worst_a:
            worst_a, worst_a_n = da, nd["node"]
        if do > 1e-3 or da > 0.05:
            fails.append("Platzierung: %s steht %.4f m / %.3f Grad neben node_table()"
                         % (nd["node"], do, da))
    built = {m.name: m for _, m in bodies}
    got = {m.name: m for m in scene}
    for miss in sorted(set(built) - set(got)):
        fails.append("Platzierung: Koerper %s fehlt in der Datei" % miss)
    for extra in sorted(set(got) - set(built)):
        fails.append("Platzierung: Koerper %s steht in der Datei, wurde aber nicht gebaut"
                     % extra)
    worst_b, worst_b_n = 0.0, "-"
    for nm in sorted(set(built) & set(got)):
        a, b = np.array(built[nm].v), np.array(got[nm].v)
        d = max(float(np.abs(a.min(0) - b.min(0)).max()),
                float(np.abs(a.max(0) - b.max(0)).max())) if len(a) == len(b) else 9e9
        if d > worst_b:
            worst_b, worst_b_n = d, nm
        if d > 1e-3:
            fails.append("Platzierung: Koerper %s liegt in der Datei %.4f m daneben" % (nm, d))
    rows = dict(worst_node_origin_m=round(worst_o, 6), worst_node_origin=worst_o_n,
                worst_node_axis_deg=round(worst_a, 6), worst_node_axis=worst_a_n,
                worst_body_bbox_m=round(worst_b, 6), worst_body=worst_b_n,
                tolerance_m=1e-3, tolerance_deg=0.05,
                nodes=len(nodes), bodies=len(got), passed=not fails)
    return rows, fails


# ================================================================ Normpruefung AN DER SZENE

class Norms:
    """Jedes Mass wird an der AUSGELIEFERTEN SZENE nachgemessen, nicht an den Eingaben und
    nicht an den Netzen vor dem Parenting.

    Die Regel, die der Tank in Runde 4 teuer gelernt hat und die Runde 1 hier zu eng gefasst
    hat: eine Pruefung in einem Bauskript misst an dem, was AUSGELIEFERT wird. Es steht
    deshalb KEIN ausfuehrbares `assert` in diesem Verzeichnis. Eine Bandpruefung kann mehr:
    sie haelt beim ersten Verstoss nicht an, legt JEDE Zeile mit Wert, Grenze und Quelle im
    Sidecar ab, faellt nicht weg wenn jemand mit -O startet, und unterscheidet das URTEIL
    (L0) vom blossen Messen (L1..L3).
    """

    def __init__(self, bodies):
        self.b = {m.name: m for _, m in bodies}
        self.rows = []
        self.fails = []

    def has(self, pre):
        return any(n.startswith(pre) for n in self.b)

    def pick(self, pre):
        return [(n, self.b[n]) for n in sorted(self.b) if n.startswith(pre)]

    def pts(self, *pre):
        out = []
        for n, m in self.b.items():
            if any(n.startswith(p) for p in pre):
                out.append(np.array(m.v))
        return np.concatenate(out) if out else np.zeros((0, 3))

    def band(self, key, value, lo, hi, unit, rule):
        tol = 1e-6
        ok = (lo is None or value >= lo - tol) and (hi is None or value <= hi + tol)
        self.rows.append(dict(key=key, measured=round(float(value), 6), min=lo, max=hi,
                              unit=unit, rule=rule, passed=bool(ok)))
        if not ok:
            self.fails.append("%s: gemessen %.4f %s, gefordert %s..%s [%s]"
                              % (key, value, unit, lo, hi, rule))
        return ok

    def note(self, key, value, unit, rule):
        self.rows.append(dict(key=key, measured=round(float(value), 6), min=None, max=None,
                              unit=unit, rule=rule, passed=None))

    def skip(self, key, why):
        self.rows.append(dict(key=key, measured=None, min=None, max=None, unit=None,
                              rule=why, passed=None))


def check_norms(bodies, lod):
    """Alle Hauptmasse am gebauten Netz gegen [PUB] und [BP].

    GEMESSEN WIRD AUF JEDER STUFE, GEURTEILT AUF L0. L0 IST der Koerper; L1..L3 sind
    Naeherungen, deren Abweichung absichtlich, beziffert und vom Silhouettentor begrenzt ist.
    Ihre Messwerte stehen trotzdem vollstaendig im Sidecar — verschwiegen wird nichts.
    """
    N = Norms(bodies)

    allv = N.pts("")
    N.band("length.overall", float(allv[:, 1].max() - allv[:, 1].min()),
           G.kLengthOverall * 0.995, G.kLengthOverall * 1.005, "m",
           "[BP] Pitotspitze bis Spitze der Bremsschirmverkleidung, Riss %.1f..%.1f px "
           "= %.3f m, Band 0.5 %%" % (G.kPxPitotTip, G.kPxRearmost, G.kLengthOverall))

    # Laenge OHNE Pitotrohr — die Zahl, die [PUB] mit 16.7 m nennt.
    noboom = N.pts("fus.", "ctl.", "wing.", "glove.", "fin", "nozzle", "canopy", "inlet.",
                   "gear.", "pylon.", "gun.", "sensor.", "antenna.")
    N.band("length.airframe", float(noboom[:, 1].max() - noboom[:, 1].min()),
           G.kLength * 0.99, G.kLength * 1.01, "m",
           "[PUB] 16.7 m ohne Pitotrohr; Riss misst %.3f m (%.2f %%), Band 1 %%"
           % (G.kLengthMeasured, 100 * G.kLengthErrRel))

    N.band("height.on_gear", float(allv[:, 2].max() - allv[:, 2].min()),
           G.kHeight * 0.995, G.kHeight * 1.005, "m",
           "[PUB] 4.82 m; die Beinlaenge ist DARAUS gerechnet, weil Front- und Seitenriss "
           "keine gemeinsame Hoehenbezugslinie haben (DEFECTS.md #1)")

    wing = N.pts("wing.", "ctl.slat.", "ctl.flap.", "ctl.spoiler.")
    if len(wing):
        span = float(wing[:, 0].max() - wing[:, 0].min())
        N.band("wing.span.built", span, G.kSpanSpread * 0.995, G.kSpanSpread * 1.005, "m",
               "[PUB] 13.965 m gespreizt; gebaut wird bei %.0f Grad Zapfenstellung"
               % G.kSweepDefault)
        # Die Kinematik wird am NETZ geprueft: die aeusserste Punktkoordinate wird um den
        # gemessenen Zapfen in jede belegte Raste gedreht und gegen [PUB] gehalten.
        # DIE KINEMATIK WIRD AM NETZ GEPRUEFT, nicht an der Formel: die GEBAUTE
        # Steuerbord-Punktwolke wird um den Zapfen in jede belegte Raste gedreht und die
        # groesste x-Koordinate gegen [PUB] gehalten. Ein einzelner "Spitzenpunkt" waere
        # mehrdeutig — alle Punkte des Randbogens haben dieselbe Spannweite.
        stb = wing[wing[:, 0] > 0.0]
        pv = np.array([G.kPivotX, G.kPivotY])
        rel = stb[:, :2] - pv
        r = np.hypot(rel[:, 0], rel[:, 1])
        th = np.arctan2(rel[:, 1], rel[:, 0])
        N.note("wing.pivot_radius_max", float(r.max()), "m",
               "[DERIVED] groesster Abstand eines Fluegelpunkts vom Zapfen")
        for sw, want in ((G.kSweepMin, G.kSpanSpread), (G.kSweepMax, G.kSpanSwept)):
            d = math.radians(sw - G.kSweepDefault)
            x = float((pv[0] + r * np.cos(th - d)).max())
            N.band("wing.span.sweep%02.0f" % sw, 2.0 * x, want * 0.99, want * 1.01, "m",
                   "[PUB] Spannweite bei %.0f Grad; die GEBAUTE Punktwolke um den gemessenen "
                   "Zapfen gedreht, Band 1 %%" % sw)

    fin = N.b.get("fin")
    if fin is not None:
        fv = np.array(fin.v)
        N.note("fin.tip_z", float(fv[:, 2].max()), "m",
               "[BP] Flossenspitze, Riss 1853 px -> %.3f m ueber der Triebwerksachse"
               % G.kFinTipZ)

    tail = N.pts("ctl.taileron.")
    if len(tail):
        N.note("tailplane.span", float(tail[:, 0].max() - tail[:, 0].min()), "m",
               "[BP] Grundriss, Spitzen bei 1250 px / spiegelbildlich")

    noz = N.b.get("nozzle")
    if noz is not None:
        nv = np.array(noz.v)
        y_ex = G.y_side(G.kNozzleExitPx)
        aft = nv[np.abs(nv[:, 1] - y_ex) < 0.03]
        N.band("nozzle.exit_diameter", float(aft[:, 2].max() - aft[:, 2].min()),
               G.kNozzleExitDia * 0.97, G.kNozzleExitDia * 1.03, "m",
               "[BP] Seitenriss x=188 px, 2307..2557 px = %.3f m, Band 3 %%" % G.kNozzleExitDia)

    tyres = N.pick("gear.main.")
    tyre = [m for n, m in tyres if n.endswith(".wheel")]
    if tyre:
        tv = np.array(tyre[0].v)
        N.band("gear.tyre_main_diameter", float(tv[:, 2].max() - tv[:, 2].min()),
               G.kTireMainDia * 0.97, G.kTireMainDia * 1.03, "m",
               "[WEB] Hauptrad 830 x 225 mm; Gegenprobe Frontansicht 48 px Breite")
        cx = [float(np.array(m.v)[:, 0].mean()) for n, m in tyres if n.endswith(".wheel")]
        if len(cx) == 2:
            N.band("gear.track", abs(cx[0] - cx[1]), G.kTrack * 0.98, G.kTrack * 1.02, "m",
                   "[WEB] Spurweite 2.658 m, Band 2 %")

    ground = float(allv[:, 2].min())
    N.band("ground.clearance", abs(ground - G.kGroundZ), 0.0, 0.005, "m",
           "[DERIVED] tiefster Netzpunkt MUSS die Aufstandsebene sein (Reifenunterkante)")

    # Symmetrie: das Netz ist spiegelsymmetrisch bis auf die Koerper, die es nicht sind.
    # SYMMETRIE MIT EINEM RASTERSCHRITT SPIEL. Ohne ihn misst der Test sein eigenes Raster:
    # x und -x koennen in verschiedene 0.1-mm-Zellen fallen, wenn |x| genau auf einer
    # Zellgrenze liegt (cos(pi/3) ist 0.5000000000000001, cos(2pi/3) ist -0.4999999999999998).
    # Der erste Lauf meldete dafuer 8 Punkte am Pitotrohr, die geometrisch exakt symmetrisch
    # sind. Ein Nachbarschaftsfenster von +-1 Zelle heisst: symmetrisch auf 0.2 mm.
    have = set(map(tuple, np.rint(allv / 1e-4).astype(np.int64).tolist()))
    off = [(a, b, c) for a in (-1, 0, 1) for b in (-1, 0, 1) for c in (-1, 0, 1)]

    def unmatched(q):
        kk = np.rint(np.stack([-q[:, 0], q[:, 1], q[:, 2]], 1) / 1e-4).astype(np.int64)
        bad = 0
        for t in map(tuple, kk.tolist()):
            if t in have:
                continue
            if not any((t[0] + d[0], t[1] + d[1], t[2] + d[2]) in have for d in off):
                bad += 1
        return bad

    miss = unmatched(allv)
    if miss:
        bad = []
        for nm, m in sorted(N.b.items()):
            c = unmatched(np.array(m.v))
            if c:
                bad.append("%s:%d" % (nm, c))
        N.note("symmetry.offenders", float(len(bad)), "Koerper", " ".join(bad[:12]))
        print("  NORM Symmetriefehler in: %s" % " ".join(bad[:12]))
    N.band("symmetry.mirror_misses", float(miss), 0.0, 0.0, "Punkte",
           "[DERIVED] die MiG-23 ist symmetrisch; jeder Punkt muss seinen Spiegelpunkt haben")

    N.note("triangles", float(sum(len(f) - 2 for _, m in bodies for f in m.f)), "-",
           "Dreiecke dieser Stufe")
    return N.rows, N.fails


# ================================================================ Silhouette ohne Renderer

def flatten(bodies):
    """Punktfeld und Dreiecksindizes einer LOD-Stufe — EINMAL je Stufe, nicht je Ansicht."""
    vs, ts, off = [], [], 0
    for me in bodies:
        vs.append(np.asarray(me.v, dtype=np.float64))
        for f in me.f:
            for i in range(1, len(f) - 1):
                ts.append((f[0] + off, f[i] + off, f[i + 1] + off))
        off += len(me.v)
    return np.concatenate(vs), np.asarray(ts, dtype=np.int64)


def raster(flat, u_ax, v_ax, res, ctr, size, front=None):
    """Orthografische Alpha-Maske ohne Renderer, zeilenweise und voll vektorisiert.

    `front` haelt nur die dem Betrachter zugewandten Dreiecke. Fuer GESCHLOSSENE Koerper —
    und check_body erzwingt, dass jeder es ist — ist die Projektion der Vorderseiten EXAKT
    die Silhouette; die Rueckseiten decken dieselbe Flaeche. Das halbiert die Dreiecksarbeit
    ohne einen einzigen Pixel zu aendern, und genau das war hier der Engpass: das Tor
    rastert 4 Stufen x 361 Azimute.
    """
    V, T = flat
    if front is not None:
        T = T[front]
    o = np.array([float(np.dot(ctr, u_ax)) - size / 2.0,
                  float(np.dot(ctr, v_ax)) - size / 2.0])
    uv = (np.stack([V @ u_ax, V @ v_ax], 1) - o) * (res / size)
    tri = uv[T]
    lo = np.ceil(tri[:, :, 1].min(1) - 0.5)
    hi = np.floor(tri[:, :, 1].max(1) - 0.5)
    y0 = np.clip(lo, 0, res - 1).astype(np.int64)
    y1 = np.clip(hi, 0, res - 1).astype(np.int64)
    cnt = np.where((hi >= 0) & (lo <= res - 1), y1 - y0 + 1, 0)
    cnt = np.maximum(cnt, 0)
    tot = int(cnt.sum())
    mask = np.zeros((res, res), dtype=bool)
    if not tot:
        return mask
    idx = np.repeat(np.arange(len(tri)), cnt)
    starts = np.cumsum(cnt) - cnt
    rows = (np.arange(tot) - np.repeat(starts, cnt) + np.repeat(y0, cnt))
    yc = rows + 0.5
    t = tri[idx]
    xmin = np.full(tot, np.inf)
    xmax = np.full(tot, -np.inf)
    for e in range(3):
        a, b = t[:, e], t[:, (e + 1) % 3]
        dy = b[:, 1] - a[:, 1]
        ok = (dy != 0.0) & (np.minimum(a[:, 1], b[:, 1]) <= yc) & (yc <= np.maximum(a[:, 1], b[:, 1]))
        with np.errstate(divide="ignore", invalid="ignore"):
            x = a[:, 0] + (b[:, 0] - a[:, 0]) * (yc - a[:, 1]) / dy
        xmin = np.minimum(xmin, np.where(ok, x, np.inf))
        xmax = np.maximum(xmax, np.where(ok, x, -np.inf))
    i0 = np.clip(np.ceil(xmin - 0.5), 0, res - 1)
    i1 = np.clip(np.floor(xmax - 0.5), 0, res - 1)
    keep = np.isfinite(xmin) & np.isfinite(xmax) & (i1 >= i0) & (xmax >= -0.5) & (xmin <= res - 0.5)
    if not keep.any():
        return mask
    r_, a_, b_ = rows[keep], i0[keep].astype(np.int64), i1[keep].astype(np.int64)
    w = res * (res + 1)
    diff = (np.bincount(r_ * (res + 1) + a_, minlength=w)
            - np.bincount(r_ * (res + 1) + b_ + 1, minlength=w))
    return np.cumsum(diff[:w].reshape(res, res + 1), axis=1)[:, :res] > 0


def front_faces(flat, d):
    """Maske der dem Betrachter zugewandten Dreiecke (Blickrichtung d)."""
    V, T = flat
    a, b, c = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    n = _cross(b - a, c - a)
    return (n @ d) > 0.0


def _views(k):
    """k Azimute plus die Draufsicht.

    k MUSS TEILERFREMD ZU JEDER SEGMENTZAHL DER LEITER SEIN, sonst misst das Tor sich selbst.
    Der Tank hat das gemessen: 180/360/720 Azimute lieferten dreimal EXAKT denselben Wert am
    selben Azimut, weil ggT(k,20)=20 dieselben Facettenphasen abtastet — ein Alias, keine
    Konvergenz. Die Leiter DIESES Assets hat 72/48/32/22 Segmente (kLod); 361 = 19^2 ist zu
    allen vieren teilerfremd, weil 19 keine davon teilt. Runde 1 nannte hier 64/40/24/14 —
    Zahlen, die in diesem Asset nicht vorkommen, aus einem anderen uebernommen.
    """
    out = []
    for j in range(k):
        a = TAU * j / k
        out.append(("az%08.4f" % math.degrees(a), a,
                    np.array([-math.sin(a), math.cos(a), 0.0]), np.array([0.0, 0.0, 1.0]),
                    np.array([math.cos(a), math.sin(a), 0.0])))
    out.append(("top", -1.0, np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0]),
                np.array([0.0, 0.0, -1.0])))
    return out


kSilAzimuths = 361
# [MESSUNG + EXTRAPOLATION] Zuschlag fuer das, was ein endliches Azimutraster am Supremum
# vorbeilaeuft. Runde 1 setzte 0.03 pp und NANNTE ES SCHRANKE — das ist es nicht. Gemessen
# (--refine-sil, Paarung L0->L3, 1024 px):
#     361 Azimute 0.9440 %   1447 Azimute 1.0309 %   2887 Azimute 1.0799 %
# Die Zuwaechse sind +0.0869 und +0.0490 pp je Vervierfachung, Verhaeltnis 0.564. Als
# geometrische Folge fortgesetzt bleibt ein Rest von 0.0490 * 0.564/(1-0.564) = 0.063 pp,
# also liegt das Supremum etwa 0.199 pp ueber dem 361er-Wert. Genommen wird 0.20.
# DAS IST EINE SCHAETZUNG DES ABSTANDS, KEINE SCHRANKE — das Wort faellt bewusst weg.
kSilSupShortfallPp = 0.20
kSilRefine = (361, 1447, 2887)
kSilRefineResult = dict(azimuths=[361, 1447, 2887], pair="L0->L3", res_px=1024,
                        worst_pct=[0.9440, 1.0309, 1.0799],
                        increments_pp=[0.0869, 0.0490], ratio=0.564,
                        extrapolated_tail_pp=0.063, gap_to_361_pp=0.199,
                        note=("Gemessen mit --refine-sil (211.7 s). Die Folge steigt noch; "
                              "kSilSupShortfallPp ist die geometrische Fortsetzung, keine "
                              "obere Schranke."))
kSilCoarse = 4
kSilScreenAbove = 300
kSilResCap = 1024             # [SET] feinste Rasterung; s. DEFECTS.md #15


def refine_silhouette(stats, ranges, azimuths):
    """Misst die schlechteste Paarung ueber eine REIHE von Azimutzahlen. Der einzige Weg,
    den Zuschlag kSilSupShortfallPp zu belegen statt ihn zu behaupten."""
    global kSilAzimuths
    keep, out = kSilAzimuths, []
    for k in azimuths:
        kSilAzimuths = k
        sil, _ = check_silhouette(stats, ranges)
        out.append((k, sil["worst"]["pair"], sil["worst"]["pct"]))
    kSilAzimuths = keep
    print("  SIL-VERFEINERUNG " + "  ".join("%d:%s %.4f %%" % r for r in out))
    inc = [out[i + 1][2] - out[i][2] for i in range(len(out) - 1)]
    r = inc[-1] / inc[-2] if len(inc) > 1 and abs(inc[-2]) > 1e-12 else 0.0
    tail = inc[-1] * r / (1.0 - r) if 0.0 < r < 1.0 else float("inf")
    gap = sum(inc) + tail
    print("  SIL-VERFEINERUNG Zuwaechse %s pp, Verhaeltnis %.3f, geometrischer Rest %.4f pp "
          "-> Abstand zum Supremum %.4f pp; kSilSupShortfallPp = %.2f  %s"
          % ("/".join("%+.4f" % i for i in inc), r, tail, gap, kSilSupShortfallPp,
             "DECKT" if kSilSupShortfallPp >= gap else "ZU KLEIN"))
    return out


def check_silhouette(stats, ranges, limit=2.0):
    """Silhouetten-Tor OHNE Renderer, kSilAzimuths Azimute plus Draufsicht."""
    lo = np.array([stats[0]["bbox"][k][0] for k in "xyz"])
    hi = np.array([stats[0]["bbox"][k][1] for k in "xyz"])
    ctr = (lo + hi) / 2.0
    size = float((hi - lo).max()) * 1.06
    names = [s["lod"] for s in stats]
    flat = [flatten([m for _, m in s["_bodies"]]) for s in stats]
    views = _views(kSilAzimuths)
    pairs = list(zip(range(len(names) - 1), range(1, len(names)))) + [(0, len(names) - 1)]
    cache, ff, rows, worst, worst_row = {}, {}, [], 0.0, None

    def mask(lv, ang, u, v, d, res):
        key = (lv, round(ang, 9), res)
        if key not in cache:
            fk = (lv, round(ang, 9))
            if fk not in ff:
                ff[fk] = front_faces(flat[lv], d)
            cache[key] = raster(flat[lv], u, v, res, ctr, size, ff[fk])
        return cache[key]

    def sweep(a, b, res):
        w, wv = 0.0, None
        for tag, ang, u, v, d in views:
            ma, mb = mask(a, ang, u, v, d, res), mask(b, ang, u, v, d, res)
            pct = 100.0 * int(np.logical_xor(ma, mb).sum()) / max(int(ma.sum()), 1)
            if pct > w:
                w, wv = pct, tag
        return w, wv

    for a, b in pairs:
        rng = ranges[a] if b == a + 1 else ranges[len(names) - 2]
        res = int(max(48, min(kSilResCap, round(size / max(rng * G.kPixelAngle, 1e-9)))))
        res_c = max(48, res // kSilCoarse)
        screen = res > kSilScreenAbove and res_c < res
        w, wv = sweep(a, b, res_c if screen else res)
        fine = not screen or w + kSilSupShortfallPp >= 0.5 * limit
        if fine and screen:
            w, wv = sweep(a, b, res)
        judged = w + kSilSupShortfallPp
        rows.append(dict(pair="%s->%s" % (names[a], names[b]), res=res if fine else res_c,
                         res_full=res, range_m=round(rng, 1), azimuths=kSilAzimuths,
                         worst_pct=round(w, 4), judged_pct=round(judged, 4), worst_view=wv,
                         escalated=bool(fine),
                         note=("volle Aufloesung" if fine else
                               "grob gemessen und angenommen: obere Schranke unter der halben "
                               "Grenze, die feine Messung kann nur kleiner sein")))
        if judged > worst:
            worst, worst_row = judged, (names[a], names[b], wv, res if fine else res_c, w)
        print("  SIL %s->%s  %4d px (%6.1f m)  %d Azimute  schlechtester %-12s %5.3f %% "
              "(+%.2f pp Raster = %5.3f %%)  %-5s %s"
              % (names[a], names[b], res if fine else res_c, rng, kSilAzimuths, wv, w,
                 kSilSupShortfallPp, judged, "fein" if fine else "grob",
                 "OK" if judged <= limit else "UEBER GRENZE"))
    ok = worst <= limit
    print("  SIL Grenze %.2f %%  schlechteste Zeile %s->%s %s bei %d px = %.3f %% + %.2f pp  %s"
          % (limit, worst_row[0], worst_row[1], worst_row[2], worst_row[3], worst_row[4],
             kSilSupShortfallPp, "BESTANDEN" if ok else "DURCHGEFALLEN"))
    return dict(limit_pct=limit, passed=bool(ok), azimuths=kSilAzimuths,
                sup_shortfall_pp=kSilSupShortfallPp, coarse_divisor=kSilCoarse,
                res_cap=kSilResCap,
                worst=dict(pair="%s->%s" % (worst_row[0], worst_row[1]), view=worst_row[2],
                           res=worst_row[3], pct=round(worst_row[4], 4),
                           judged_pct=round(worst, 4)),
                method=(
                    "%d Azimute plus Draufsicht, fest und teilerfremd zu den vier "
                    "Segmentzahlen dieser Leiter (72/48/32/22). Nur die dem Betrachter "
                    "ZUGEWANDTEN Dreiecke werden gerastert: bei einem geschlossenen Koerper "
                    "ist deren Projektion exakt die Silhouette, und das halbiert die Arbeit "
                    "ohne einen Pixel zu aendern. Azimutzuschlag %.2f pp — GEMESSEN, nicht "
                    "hergeleitet: die schlechteste Paarung gibt bei %s Azimuten die Werte, "
                    "die unter azimuth_refinement stehen, und die Folge steigt noch. Der "
                    "Zuschlag ist die dreifache gemessene Differenz, keine Schranke."
                    % (kSilAzimuths, kSilSupShortfallPp,
                       "/".join(str(k) for k in kSilRefine))),
                azimuth_refinement=kSilRefineResult,
                rows=rows), ok


# ================================================================ Die Baugruppen

def _fus_table():
    """Rumpfstationen in Metern: (y, z_oben, z_unten, halbe Breite)."""
    out = []
    for px, zt, zb, w in G.kFusStations:
        out.append((G.y_side(px), G.z_side(zt), G.z_side(zb), w * G.kPxM))
    return sorted(out, key=lambda r: r[0])


def _fus_shape(y):
    """Formexponenten (oben, unten) an der Station y."""
    kn = sorted((G.y_side(px), a, b) for px, a, b in G.kFusShapeKnots)
    return _interp([(k[0], k[1], k[2]) for k in kn], y)


def fuselage(cfg):
    """Der Rumpf: aus 35 Spantrissen geloftet, auf cfg['fus'] mal so viele Stationen
    aufgeloest. Kein Zylinder, keine Rotationsflaeche — die Taille zwischen den Einlaeufen und
    der flache Ruecken kommen aus den Schnitten selbst."""
    tab = _fus_table()
    ys = [r[0] for r in tab]
    kfac = G.ring_radius(1.0, cfg["seg"])
    rings = []
    steps = max(1, cfg["fus"])
    for i in range(len(tab) - 1):
        for s in range(steps):
            f = s / steps
            y = ys[i] + f * (ys[i + 1] - ys[i])
            zt = _spline(ys, [r[1] for r in tab], y)
            zb = _spline(ys, [r[2] for r in tab], y)
            # UNTERGRENZE, und sie ist keine Kosmetik: an der Radomspitze laeuft der Schnitt
            # auf null, und ein 0.1-mm-Schnitt legt bei 64 Segmenten mehrere Punkte in
            # dieselbe Weld-Zelle. Der T-Stoss-Test hat genau das gefunden (4 Stoesse bei
            # y = 9.179 m, drei Punkte in einem 10-um-Fenster). 6 mm liegen sicher darueber
            # und sind 0.1 m vor der Spitze unsichtbar; geschlossen wird sie vom Pol.
            w = max(kMinSection, _spline(ys, [r[3] for r in tab], y))
            nt, nb = _fus_shape(y)
            zc = 0.5 * (zt + zb)
            at = max(kMinSection, (zt - zc)) * kfac
            ab = max(kMinSection, (zc - zb)) * kfac
            sec = superellipse(cfg["seg"], w * kfac, at, ab, nt, nb, zc)
            rings.append([(p[0], y, p[2]) for p in sec])
    # Nasenspitze und Duesenende als echte Pole
    y0, y1 = ys[0], ys[-1]
    zt0 = _spline(ys, [r[1] for r in tab], y1)
    zb0 = _spline(ys, [r[2] for r in tab], y1)
    rings.append([(0.0, y1, 0.5 * (zt0 + zb0))] * cfg["seg"])
    rings.insert(0, [(0.0, y0, 0.5 * (tab[0][1] + tab[0][2]))] * cfg["seg"])
    return tube("fus.body", "skin", rings)


def pitot(cfg):
    """Pitotrohr mit dem Wirbelerzeuger-Kranz der MLD."""
    n = max(6, cfg["seg"] // 4)
    y0 = G.y_side(G.kFusStations[0][0]) - 0.28
    y1 = G.y_side(G.kPitotTipPx)
    z = G.z_side(2529.0)
    rings = [ring(n, 0.0, 0.0, G.kPitotRootDia / 2.0, y0, ax=0, ay=2, az=1)]
    rings = [[(p[0], p[2], p[1] + z) for p in rings[0]]]
    out = []
    for f in (0.0, 0.45, 0.9, 1.0):
        r = G.kPitotRootDia / 2.0 + f * (G.kPitotTipDia - G.kPitotRootDia) / 2.0
        y = y0 + f * (y1 - y0)
        out.append([(r * math.cos(TAU * i / n), y, z + r * math.sin(TAU * i / n))
                    for i in range(n)])
    return tube("boom.pitot", "frame", out)


def _panel_section(u, af_n, thk, f0=0.0, f1=1.0):
    """Fluegelschnitt im Paneelrahmen an der Spannweitenstation u -> Punkte (u, v, w)."""
    c = G.panel_chord(u)
    v0 = G.panel_le_v(u)
    prof = prof_seg(thk, f0, f1, af_n)
    dz = (u - G.kPanelRootU) * math.tan(math.radians(G.kPanelDihedralDeg))
    return [(u, v0 + c * px, dz + c * py) for px, py in prof]


def _panel_to_body(pts, side):
    """Paneelrahmen -> Rumpfrahmen bei der GEBAUTEN Zapfenstellung (= Paneelrahmen)."""
    return [(G.kPivotX + u, -v, G.kPivotZ + w) for u, v, w in pts]


def _panel_thk(u):
    f = (u - G.kPanelRootU) / (G.kPanelLeTipU - G.kPanelRootU)
    return G.kAirfoilRootThk + f * (G.kAirfoilTipThk - G.kAirfoilRootThk)


def wing_panel(cfg):
    """Der bewegliche Aussenfluegel (Steuerbord; Backbord wird gespiegelt).

    Er endet an den Sehnenschnitten der Klappen: Vorfluegel und Landeklappe sind KEINE
    Kopien, die im Fluegel stecken, sondern die fehlenden Stuecke. Die erste Fassung baute
    sie als volle Profilausschnitte im Inneren des Fluegels — die Durchdringungspruefung mass
    97.9 % des Klappenvolumens im Fluegel und hat damit einen Modellfehler gefunden, keinen
    Messfehler: eine Klappe, die im Fluegel steckt, kann sich nicht bewegen.
    """
    us = np.linspace(G.kPanelRootU, G.kPanelLeTipU - 1e-3, cfg["span"])
    rings = []
    for u in us:
        rings.append(_panel_to_body(_panel_section(u, cfg["af"], _panel_thk(u),
                                                   G.kSlatChordFrac, 1.0 - G.kFlapChordFrac),
                                    "r"))
    return tube("wing.r", "skin", rings)


def _flap_body(name, mat, u0, u1, f0, f1, n, af_n, thk_scale=1.0, lift=0.0):
    """Ein Klappenkoerper als Sehnenausschnitt [f0..f1] des Profils zwischen u0 und u1."""
    rings = []
    for u in np.linspace(u0, u1, n):
        c = G.panel_chord(u)
        v0 = G.panel_le_v(u)
        thk = _panel_thk(u) * thk_scale
        prof = prof_seg(thk, f0, f1, af_n)
        dz = (u - G.kPanelRootU) * math.tan(math.radians(G.kPanelDihedralDeg)) + lift
        rings.append(_panel_to_body([(u, v0 + c * px, dz + c * py) for px, py in prof], "r"))
    return solid(name, mat, rings)


def wing_controls(cfg):
    """Vorfluegel, Landeklappe, Stoerklappe — je ein Koerper mit eigenem Gelenk."""
    u0, u1 = G.kPanelRootU, G.kPanelLeTipU - 1e-3
    n = max(3, cfg["span"] // 2)
    out = [_flap_body("ctl.slat.r.skin", "skin", u0, u1, 0.0, G.kSlatChordFrac - 0.004, n, cfg["af"]),
           _flap_body("ctl.flap.r.skin", "skin", u0, u1,
                      1.0 - G.kFlapChordFrac + 0.004, 1.0, n, cfg["af"])]
    if cfg["detail"] >= 1:
        # Die Stoerklappe LIEGT AUF der Oberseite (sie faehrt nach oben aus), sie steckt nicht
        # im Fluegel: ihre Unterseite folgt der Profilkontur, ihre Oberseite liegt darueber.
        rings = []
        for u in np.linspace(G.kSpoilerU[0], G.kSpoilerU[1], max(3, n // 2)):
            c = G.panel_chord(u)
            v0 = G.panel_le_v(u)
            thk = _panel_thk(u)
            dz = (u - G.kPanelRootU) * math.tan(math.radians(G.kPanelDihedralDeg))
            f0, f1 = 0.42, 0.42 + G.kSpoilerChordFrac
            xs = [f0 + (f1 - f0) * i / 6.0 for i in range(7)]
            up = [(x, naca_t(x, thk) + 0.022) for x in xs]
            lo = [(x, naca_t(x, thk) + 0.001) for x in reversed(xs)]
            rings.append(_panel_to_body(
                [(u, v0 + c * px, dz + c * py) for px, py in up + lo], "r"))
        out.append(solid("ctl.spoiler.r.skin", "frame", rings))
    return out


def glove(cfg):
    """Der feste Handschuh mit dem MLD-Saegezahn an der Vorderkante (Steuerbord).

    Seine Vorderkante ist KEINE eigene Messung, sondern die Verlaengerung der Paneel-VK aus
    dem 72-Grad-Riss nach innen: bei voller Pfeilung flieht das Paneel mit dem Handschuh — das
    ist der Sinn der Stellung, und der Riss zeigt es. Die so gerechnete Handschuhspitze landet
    bei y = %.2f m, also dicht vor der Einlauflippe (%.2f m) — was der Riss unabhaengig zeigt.
    """
    xin = G.kGloveRootX
    xout = G.kPivotX + G.kPanelRootU
    tanlam = math.tan(math.radians(G.kGloveLeSweep72))
    y_out = -G.panel_le_v(G.kPanelRootU)
    y_in = min(y_out + (xout - xin) * tanlam, G.y_side(G.kInletLipPx) + 0.16)
    # HINTERKANTE: keine gerade Kante quer zum Rumpf, sondern eine Fluchtlinie von der
    # Rumpfseite bis zur Paneelhinterkante — sonst steht der Handschuh als Regal neben dem
    # Rumpf und die Fuge zum Fluegel klafft. Beides war im ersten Bild zu sehen.
    y_te_out = -G.panel_te_v(G.kPanelRootU)
    saw = G.kGloveSawtoothU
    prof = []
    for f in (0.0, saw - 1e-3, saw, 1.0):
        x = xin + f * (xout - xin)
        yl = y_in + f * (y_out - y_in)
        if f >= saw:
            yl -= G.kGloveSawtoothDepth * (1.0 - f) / max(1.0 - saw, 1e-6)
        prof.append((x, yl, G.kGloveTeY + f * (y_te_out - G.kGloveTeY)))
    rings = []
    for z, sc in ((G.kPivotZ - 0.11, 0.62), (G.kPivotZ, 1.0), (G.kPivotZ + 0.11, 0.62)):
        loop = [(x, G.kPivotY + (yl - G.kPivotY) * sc, z) for x, yl, _ in prof]
        loop += [(x, yt, z) for x, _, yt in reversed(prof)]
        rings.append(loop)
    return solid("glove.r", "skin", rings)


def inlet(cfg):
    """Rechteckeinlauf mit Lippe und TIEFEM, offenem Kanal (Steuerbord).

    KEIN Deckel und kein zweiter Koerper im ersten: der Kasten ist ein BECHER — die Aussenhaut
    laeuft bis zur Lippe, faltet dort nach innen und laeuft %.2f m in den Rumpf zurueck, wo eine
    dunkle Platte den Grund bildet. Die erste Fassung steckte einen dunklen Kanalkoerper in
    einen geschlossenen Kasten; die Durchdringungspruefung mass 100.9 %% und hatte recht: was
    ganz im anderen steckt, ist nicht sichtbar, also auch kein Einlauf.
    """ % G.kInletDuctDepth
    y_lip = G.y_side(G.kInletLipPx)
    y_aft = G.y_side(1500.0)
    zt, zb = G.z_side(G.kInletTopPx), G.z_side(G.kInletBotPx)
    xo = G.kInletOuterPx * G.kPxM
    xi = xo - 0.40
    r = G.kInletLipRadius
    m = max(2, cfg["seg"] // 12)

    def rect(y, x0, x1, z0, z1, mm):
        pts = []
        n = max(2, mm)
        for i in range(n):
            pts.append((x0 + (x1 - x0) * i / n, y, z1))
        for i in range(n):
            pts.append((x1, y, z1 + (z0 - z1) * i / n))
        for i in range(n):
            pts.append((x1 + (x0 - x1) * i / n, y, z0))
        for i in range(n):
            pts.append((x0, y, z0 + (z1 - z0) * i / n))
        return pts

    d = G.kInletDuctDepth
    ya = y_aft - 0.25
    ins = 0.055
    rings = [rect(ya, xi - 0.05, xo - 0.22, zb - 0.06, zt + 0.02, m),
             rect(y_aft, xi - 0.05, xo - 0.10, zb - 0.05, zt + 0.04, m),
             rect(y_lip - 0.9, xi - 0.03, xo, zb - 0.02, zt + 0.02, m),
             rect(y_lip - 0.06, xi, xo, zb, zt, m),
             rect(y_lip, xi + r, xo - r, zb + r, zt - r, m),
             # ab hier laeuft DIESELBE Haut nach innen zurueck — der Becher ist ein Schlauch,
             # kein Deckel: erste Fassung kappte vorn und hinten mit Scheiben und schloss damit
             # den Hohlraum EIN; die Durchdringungspruefung sah die dunkle Platte zu 85 % im
             # Kasten und hatte recht.
             rect(y_lip - 0.02, xi + r + ins, xo - r - ins, zb + r + ins, zt - r - ins, m),
             rect(y_lip - d, xi + r + ins + 0.05, xo - r - ins - 0.08,
                  zb + r + ins + 0.06, zt - r - ins - 0.06, m),
             rect(y_aft, xi - 0.05 + ins, xo - 0.10 - ins, zb - 0.05 + ins, zt + 0.04 - ins, m),
             rect(ya, xi - 0.05 + ins, xo - 0.22 - ins, zb - 0.06 + ins, zt + 0.02 - ins, m)]
    out = [tube("inlet.box.r", "skin", rings, smooth=False, cyclic=True)]
    # Der Grund des Kanals: eine dunkle Platte IM Hohlraum, ohne die Haut zu beruehren.
    zc = 0.5 * (zt + zb)
    xc = 0.5 * (xi + xo)
    hw, hh = (xo - xi) * 0.5 - r - ins - 0.10, (zt - zb) * 0.5 - r - ins - 0.09
    yp = y_lip - d - 0.05
    plate = [[(xc - hw, yp, zc + hh), (xc + hw, yp, zc + hh),
              (xc + hw, yp, zc - hh), (xc - hw, yp, zc - hh)],
             [(xc - hw, yp - 0.09, zc + hh), (xc + hw, yp - 0.09, zc + hh),
              (xc + hw, yp - 0.09, zc - hh), (xc - hw, yp - 0.09, zc - hh)]]
    out.append(solid("inlet.duct.r", "dark", plate))
    if cfg["detail"] >= 1:
        rx = xi - G.kInletRampGapPx * G.kPxM
        ramp = [[(rx, y_lip, zt - 0.02), (rx - 0.05, y_lip, zt - 0.02),
                 (rx - 0.05, y_lip, zb + 0.02), (rx, y_lip, zb + 0.02)],
                [(rx + 0.10, y_lip - 1.1, zt), (rx - 0.06, y_lip - 1.1, zt),
                 (rx - 0.06, y_lip - 1.1, zb), (rx + 0.10, y_lip - 1.1, zb)]]
        out.append(solid("inlet.ramp.r", "frame", ramp))
    return out


def _plate(name, mat, quad_root, quad_tip, thk_root, thk_tip, n, af_n, axis=2):
    """Eine Flosse aus zwei Sehnen (Wurzel, Spitze) mit Profildicke. axis = Dickenrichtung."""
    rings = []
    for i in range(n):
        f = i / (n - 1)
        p0 = [quad_root[0][k] + f * (quad_tip[0][k] - quad_root[0][k]) for k in range(3)]
        p1 = [quad_root[1][k] + f * (quad_tip[1][k] - quad_root[1][k]) for k in range(3)]
        c = math.dist(p0, p1)
        thk = thk_root + f * (thk_tip - thk_root)
        prof = prof_seg(thk / max(c, 1e-6), 0.0, 1.0, af_n)
        d = [(p1[k] - p0[k]) / max(c, 1e-9) for k in range(3)]
        nrm = [0.0, 0.0, 0.0]
        nrm[axis] = 1.0
        loop = []
        for px, py in prof:
            loop.append(tuple(p0[k] + d[k] * c * px + nrm[k] * c * py for k in range(3)))
        rings.append(loop)
    return tube(name, mat, rings)


def fin(cfg):
    """Seitenflosse. Der MiG-23M-Riss zeigt die grosse Rueckenflosse davor; die ML/MLD hat sie
    NICHT (belegt), also wird sie nicht gebaut — die eine bewusste Abweichung vom Riss."""
    yt_le, zt = G.y_side(G.kFinTipLePx[0]), G.z_side(G.kFinTipLePx[1])
    yt_te = G.y_side(G.kFinTipTePx[0])
    zr = G.z_side(G.kFinRootPx)
    yr_te = G.y_side(G.kFinRootTePx)
    yr_le = G.y_side(G.kFinTipLePx[0] + (G.kFinRootPx - G.kFinTipLePx[1]) / G.kFinLeSlope)
    root = ((0.0, yr_le, zr), (0.0, yr_te, zr))
    tip = ((0.0, yt_le, zt), (0.0, yt_te, zt))
    hinge = 1.0 - G.kRudderChordFrac
    def cut(a, b, f):
        return tuple(a[k] + f * (b[k] - a[k]) for k in range(3))
    out = [_plate("fin", "skin", (root[0], cut(root[0], root[1], hinge)),
                  (tip[0], cut(tip[0], tip[1], hinge)),
                  G.kFinThickRoot, G.kFinThickTip, max(3, cfg["span"] // 2), cfg["af"], axis=0)]
    out.append(_plate("ctl.rudder.skin", "skin", (cut(root[0], root[1], hinge), root[1]),
                      (cut(tip[0], tip[1], hinge), tip[1]),
                      G.kFinThickRoot * 0.6, G.kFinThickTip * 0.7,
                      max(3, cfg["span"] // 3), max(10, cfg["af"] // 2), axis=0))
    # Bremsschirmverkleidung. Sie laeuft NACH ACHTERN und endet auf dem am Riss gemessenen
    # hintersten Zellenpunkt — die erste Fassung baute ein Rohr nach VORN und liess das
    # Flugzeug 0.16 m zu kurz enden. Querschnitt flach (breiter als hoch), s. kChuteHalfW/H.
    n = max(6, ((max(6, cfg["seg"] // 5)) // 2) * 2)   # gerade -> jeder Punkt hat sein Spiegelbild
    zc = G.z_side(2389.5)
    rings = []
    for f, s in ((0.0, 0.25), (0.22, 0.92), (0.62, 1.0), (0.88, 0.62), (1.0, 0.06)):
        y = G.kChuteRootY + f * (G.kChuteTipY - G.kChuteRootY)
        rings.append([(s * G.kChuteHalfW * math.cos(TAU * i / n), y,
                       zc + s * G.kChuteHalfH * math.sin(TAU * i / n)) for i in range(n)])
    out.append(tube("fin.chute", "skin", rings))
    if cfg["detail"] >= 2:
        # UNTER der Flossenspitze: die veroeffentlichte Standhoehe misst bis zur Flosse, und
        # eine gesetzte Antenne darf die geprueften 4.82 m nicht verschieben (gemessen 4.98).
        out.append(box("antenna.fin", "dielectric",
                       (-0.03, yt_te + 0.02, zt - 0.14), (0.03, yt_le - 0.10, zt - 0.005)))
    return out


def taileron(cfg):
    """Vollbewegliches Hoehenleitwerk mit Anhedral (Steuerbord). Es traegt Nick- UND
    Rollkommando — "Two tailerons controlled pitch and roll" [WEB]. Die Hinterkante der Spitze
    ist der HINTERSTE Punkt des ganzen Flugzeugs und wird deshalb auf die am Riss gemessene
    Heckstation gelegt (Seitenriss 66 px) statt frei gesetzt."""
    a = math.radians(G.kTailAnhedralDeg)
    xr = 0.30
    xt = abs(G.x_plan(G.kTailTipPx[1]))
    zr = G.z_side(G.kTailZPx)
    root = ((xr, G.y_plan(G.kTailRootLePx[0]), zr),
            (xr, G.y_plan(G.kTailRootTePx[0]), zr))
    dz = (xt - xr) * math.tan(a)
    # kTailTipPx ist die SPITZEN-VORDERKANTE, die Sehne laeuft von dort nach ACHTERN. Runde 1
    # legte sie mittig um diesen Punkt und schob die Spitze damit eine halbe Sehne nach vorn.
    y_tip = G.y_plan(G.kTailTipPx[0])
    tip = ((xt, y_tip, zr + dz), (xt, y_tip - G.kTailTipChord, zr + dz))
    return _plate("ctl.taileron.r.skin", "skin", root, tip,
                  G.kTailThkRoot, G.kTailThkTip, max(3, cfg["span"] // 2), cfg["af"], axis=2)


def ventral(cfg):
    """Bauchflosse. Sie klappt beim Ausfahren des Fahrwerks zur Seite — belegt, und deshalb
    ein eigener Knoten mit Scharnier auf der Rumpfunterkante."""
    y0, y1 = G.kVentralY
    zb = G.z_side(2612.0)
    d = G.kVentralDepth
    t = G.kVentralThk
    prof = [(y1, zb + 0.05), (y1 - 0.30, zb - d * 0.55), (y0 + 0.55, zb - d),
            (y0, zb - d + 0.10), (y0, zb + 0.02)]
    rings = []
    for x, sc in ((-t / 2, 1.0), (t / 2, 1.0)):
        rings.append([(x, y, z) for y, z in prof])
    return solid("ctl.ventral.skin", "skin", rings)


def canopy(cfg):
    """Kanzel: eigenes Glas, eigener Rahmen. Die MiG-23-Haube ist flach und in den Ruecken
    eingezogen — genau das unterscheidet sie von der Blase der F-16."""
    yf = G.y_side(G.kCanopyFrontPx)
    yb = G.y_side(G.kCanopyBowPx)
    yr = G.y_side(G.kCanopyRearPx)
    ztop = G.z_side(G.kCanopyTopPx)
    zsill = G.z_side(G.kCanopySillPx)
    w = G.kCanopyHalfW
    n = max(6, cfg["seg"] // 5)

    def arch(y, hw, ht, zs):
        pts = []
        for i in range(n + 1):
            t = math.pi * i / n
            pts.append((hw * math.cos(t), y, zs + ht * math.sin(t)))
        return pts

    stat = [(yf, 0.10, 0.02, zsill + 0.30), (yb, w * 0.86, ztop - zsill - 0.02, zsill),
            (yb - 0.55, w, ztop - zsill, zsill - 0.02),
            (yr + 0.35, w * 0.92, (ztop - zsill) * 0.86, zsill - 0.02),
            (yr, w * 0.55, (ztop - zsill) * 0.45, zsill)]
    rings = []
    for y, hw, ht, zs in stat:
        up = arch(y, hw, ht, zs)
        lo = [(p[0], y, zs - 0.02) for p in reversed(up[1:-1])]
        rings.append(up + lo)
    out = [tube("canopy.glass", "glass", rings)]
    if cfg["detail"] >= 1:
        # EIN RAHMEN IST EIN BAND, KEINE SCHEIBE. Die erste Fassung fuellte den Bogen und lag
        # damit zu 95 % im Glas — von der Durchdringungspruefung gemeldet. Jetzt laeuft der
        # Rahmen als schmaler Ring um die Haube herum: aussen 1.055, innen 1.005.
        for nm, (y, hw, ht, zs) in (("bow", stat[1]), ("rear", stat[3])):
            rings = []
            for i in range(n + 1):
                t = math.pi * i / n
                px, pz = hw * math.cos(t), zs + ht * math.sin(t)
                rx, rz = math.cos(t), math.sin(t)
                ln = math.hypot(rx, rz)
                rx, rz = rx / ln, rz / ln
                a, b = 0.030, 0.028
                rings.append([(px + rx * a, y + b, pz + rz * a),
                              (px + rx * a, y - b, pz + rz * a),
                              (px - rx * a * 0.2, y - b, pz - rz * a * 0.2),
                              (px - rx * a * 0.2, y + b, pz - rz * a * 0.2)])
            out.append(solid("canopy.frame.%s" % nm, "frame", rings))
    return out


def nozzle(cfg):
    """Schubduese als BECHER mit Blaettern: die Aussenhaut laeuft bis zur Austrittslippe,
    faltet dort nach innen und laeuft %.2f m nach vorn auf den engsten Querschnitt zu. Kein
    Deckel — die Duese hat Tiefe, und die erste Fassung hatte den Kanal versehentlich nach
    ACHTERN gelegt, was die Normpruefung am gemessenen Austrittsdurchmesser (0.73 statt
    1.25 m) sofort gefunden hat.""" % G.kNozzleDepth
    # n MUSS DURCH 4 TEILBAR SEIN: die Blattmodulation wechselt mit i, die Spiegelung bildet
    # i auf n/2 - i ab, also muss n/2 gerade sein. Bei n = 14 meldete die Symmetriepruefung
    # 280 Punkte ohne Partner — eine echte Asymmetrie, keine Rundung.
    n = max(8, (min(G.kNozzlePetals, cfg["seg"]) // 4) * 4)
    y_ex = G.y_side(G.kNozzleExitPx)
    r_ex = G.kNozzleExitDia / 2.0
    r_th = G.kNozzleThroatDia / 2.0
    rings = []
    for y, r, petal in ((y_ex + 0.95, r_ex * 0.90, 0.0), (y_ex + 0.30, r_ex * 0.985, 0.6),
                        (y_ex + 0.02, r_ex, 1.0), (y_ex, r_ex * 0.995, 1.0),
                        (y_ex + 0.02, r_ex * 0.955, 0.9),
                        (y_ex + G.kNozzleDepth * 0.5, r_th * 1.12, 0.4),
                        (y_ex + G.kNozzleDepth, r_th, 0.0)):
        loop = []
        for i in range(n):
            t = TAU * i / n
            mod = 1.0 + (0.014 if (i % 2 == 0) else -0.014) * petal
            loop.append((r * mod * math.cos(t), y, r * mod * math.sin(t)))
        rings.append(loop)
    return tube("nozzle", "nozzle", rings, smooth=False)


def _wheel(name, ctr, rad, width, n, mat="tyre"):
    """Rad. n wird auf ein Vielfaches von 4 gehoben und die Phase so gelegt, dass EIN PUNKT
    genau unten liegt: sonst steht der Reifen auf einer Sehne statt auf dem Scheitel, und die
    gemessene Standhoehe faellt um bis zu r(1-cos(pi/n)) zu klein aus — bei n = 6 waren das
    35 mm, und die Normpruefung hat sie gefunden."""
    n = max(8, ((n + 3) // 4) * 4)
    ph = -math.pi / 2.0
    rings = []
    for f, rr in ((0.0, rad * 0.62), (0.12, rad * 0.97), (0.5, rad), (0.88, rad * 0.97),
                  (1.0, rad * 0.62)):
        x = ctr[0] - width / 2.0 + f * width
        rings.append([(x, ctr[1] + rr * math.cos(ph + TAU * i / n),
                       ctr[2] + rr * math.sin(ph + TAU * i / n)) for i in range(n)])
    return tube(name, mat, rings)


def _rod(name, a, b, r0, r1, n, mat="frame"):
    d = np.array(b) - np.array(a)
    L = float(np.linalg.norm(d))
    if L < 1e-9:
        L = 1e-9
    d = d / L
    up = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(up, d))) > 0.95:
        up = np.array([0.0, 1.0, 0.0])
    u = np.cross(d, up)
    u = u / np.linalg.norm(u)
    v = np.cross(d, u)
    rings = []
    for f, r in ((0.0, r0), (1.0, r1)):
        c = np.array(a) + d * (L * f)
        rings.append([tuple(c + u * r * math.cos(TAU * i / n) + v * r * math.sin(TAU * i / n))
                      for i in range(n)])
    return tube(name, mat, rings, smooth=False)


def gear_nose(cfg):
    n = max(6, cfg["seg"] // 6)
    r = G.kTireNoseDia / 2.0
    zg = G.kGroundZ
    y = G.kNoseGearY
    top = (0.0, y, G.z_side(2600.0))
    axle = (0.0, y, zg + r)
    out = [_rod("gear.nose.strut", top, (0.0, y, zg + r + 0.30), 0.062, 0.048, n),
           _rod("gear.nose.piston", (0.0, y, zg + r + 0.34), axle, 0.040, 0.040, n)]
    for s, tag in ((-1.0, "l"), (1.0, "r")):
        out.append(_wheel("gear.nose.wheel.%s" % tag,
                          (s * (G.kTireNoseW * 0.75), y, zg + r), r, G.kTireNoseW, n))
    if cfg["detail"] >= 1:
        out.append(box("gear.door.nose", "skin",
                       (-0.20, y - 0.95, zg + r + 0.55), (0.20, y + 0.55, zg + r + 0.58)))
    return out


def gear_main(cfg):
    n = max(6, cfg["seg"] // 6)
    r = G.kTireMainDia / 2.0
    zg = G.kGroundZ
    y = G.kMainGearY
    xw = G.kTrack / 2.0
    pivot = (0.42, y + 0.72, G.z_side(2600.0))
    knee = (G.kTrack / 2.0 - 0.10, y + 0.30, zg + r + 0.42)
    axle = (xw, y, zg + r)
    out = [_rod("gear.main.r.strut", pivot, knee, 0.078, 0.058, n),
           _rod("gear.main.r.arm", knee, axle, 0.052, 0.046, n),
           _wheel("gear.main.r.wheel", (xw, y, zg + r), r, G.kTireMainW, n)]
    if cfg["detail"] >= 1:
        out.append(_rod("gear.main.r.drag", (0.30, y - 0.85, G.z_side(2600.0)),
                        knee, 0.040, 0.034, n))
        out.append(box("gear.door.main.r", "skin", (0.30, y - 0.62, zg + r + 0.50),
                       (0.98, y + 0.62, zg + r + 0.53)))
    if cfg["detail"] >= 2:
        # NEBEN dem Reifen, nicht darin: die Durchdringungspruefung mass 98.4 % des
        # Scherengliedes im Rad, und ein Glied im Reifen ist kein Glied.
        xs = xw - G.kTireMainW * 0.5 - 0.045
        out.append(_rod("gear.main.r.scissor", (xs, y + 0.16, zg + r + 0.30),
                        (xs, y - 0.10, zg + r + 0.10), 0.018, 0.018, max(4, n // 2)))
    return out


def stores_sym(cfg):
    """Koerper auf der Symmetrieebene: Kanonenwanne und Waermepeiler."""
    out = []
    zg = G.z_side(2612.0)
    n = max(8, ((cfg["seg"] // 6) + 1) // 2 * 2)
    rings = []
    for f, w, h in ((0.0, 0.06, 0.05), (0.15, 0.19, 0.15), (0.85, 0.20, 0.16), (1.0, 0.07, 0.06)):
        y = G.kGunY + G.kGunLen * (0.5 - f)
        rings.append([(w * math.cos(TAU * i / n), y, zg - 0.05 + h * math.sin(TAU * i / n))
                      for i in range(n)])
    out.append(tube("gun.gsh23", "frame", rings))
    if cfg["detail"] >= 1:
        rings = []
        for f, rr in ((0.0, 0.02), (0.25, 0.115), (0.8, 0.115), (1.0, 0.03)):
            y = G.kIrstY + 0.34 * (0.5 - f)
            zc = G.z_side(2600.0) - 0.10
            rings.append([(rr * math.cos(TAU * i / n), y, zc + rr * math.sin(TAU * i / n))
                          for i in range(n)])
        out.append(tube("sensor.irst", "dark", rings))
    return out


def stores_side(cfg):
    """Pylone und Bremsklappe, Steuerbord."""
    out = []
    for nm, x, y in (("glove", G.kPylonGloveX, G.kPylonGloveY),
                     ("fus", G.kPylonFusX, G.kPylonFusY)):
        zb = G.z_side(2607.0 if nm == "fus" else 2600.0)
        rings = []
        for xo in (-G.kPylonThk / 2.0, G.kPylonThk / 2.0):
            rings.append([(x + xo, y + G.kPylonChord * 0.5, zb),
                          (x + xo, y + G.kPylonChord * 0.36, zb - 0.34),
                          (x + xo, y - G.kPylonChord * 0.40, zb - 0.34),
                          (x + xo, y - G.kPylonChord * 0.5, zb)])
        out.append(solid("pylon.%s.r" % nm, "skin", rings))
    out.append(box("ctl.airbrake.r.skin", "skin",
                   (0.20, G.kAirbrakeY - 0.42, G.z_side(2600.0)),
                   (0.56, G.kAirbrakeY + 0.42, G.z_side(2585.0))))
    return out


# ================================================================ Knoten (Szenengraph)

def _rot_to(axis):
    """3x3-Matrix, die lokales +X auf `axis` dreht. Die Konvention dieses Assets:
    DIE HINGE-ACHSE IST DIE LOKALE +X DES KNOTENS, der Drehpunkt seine Herkunft."""
    a = np.array(_unit(axis))
    ref = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(a, ref))) > 0.95:
        ref = np.array([0.0, 1.0, 0.0])
    y = np.cross(ref, a)
    y = y / np.linalg.norm(y)
    z = np.cross(a, y)
    return np.stack([a, y, z], axis=1)


def node_table():
    """Die Gelenke: (Name, Elternteil, Drehpunkt, Achse, Grenzen in Grad, gebundener Wert).

    doc/assets.md §3: Knotennamen sind mit der Physik geteilt. Die Namen fuehren die
    F-16-Konvention fort (`ctl.*`, `gear.*`, `canopy`). Jede Grenze traegt ihre Quelle.
    """
    zp = G.kPivotZ
    fin_hinge_axis = _unit((0.0, -1.0 / G.kFinLeSlope, 1.0))
    zr = G.z_side(G.kFinRootPx)
    yr_te = G.y_side(G.kFinRootTePx)
    tail_axis_l = _unit((-1.0, 0.0, math.tan(math.radians(G.kTailAnhedralDeg))))
    tail_axis_r = _unit((1.0, 0.0, math.tan(math.radians(G.kTailAnhedralDeg))))
    zt = G.z_side(G.kTailZPx)
    y_tail = G.y_plan(G.kTailRootLePx[0]) - G.kTailHingeFrac * (
        G.y_plan(G.kTailRootLePx[0]) - G.y_plan(G.kTailRootTePx[0]))
    slat_axis = _unit((1.0, math.tan(math.radians(G.kPanelLeSweep)), 0.0))
    flap_axis = _unit((1.0, math.tan(math.radians(G.kPanelTeSweep)), 0.0))
    zg = G.kGroundZ
    return [
        dict(node="ctl.wingsweep.l", parent=None, origin=(-G.kPivotX, 0.0, zp),
             axis=(0.0, 0.0, 1.0), lo=0.0, hi=G.kSweepMax - G.kSweepMin,
             detents=[d - G.kSweepMin for d in G.kSweepDetents], bind="WingSweepDeg",
             source="[WEB] 16/45/72 Grad belegt; MLD zusaetzlich 33 Grad. 0 = gespreizt."),
        dict(node="ctl.wingsweep.r", parent=None, origin=(G.kPivotX, 0.0, zp),
             axis=(0.0, 0.0, -1.0), lo=0.0, hi=G.kSweepMax - G.kSweepMin,
             detents=[d - G.kSweepMin for d in G.kSweepDetents], bind="WingSweepDeg",
             source="[WEB] wie links, gespiegelte Achse."),
        dict(node="ctl.slat.l", parent="ctl.wingsweep.l",
             origin=(-(G.kPivotX + G.kPanelRootU + 0.05),
                     -G.panel_le_v(G.kPanelRootU + 0.05)
                     - G.kSlatChordFrac * G.panel_chord(G.kPanelRootU + 0.05), zp),
             axis=(-slat_axis[0], slat_axis[1], slat_axis[2]),
             lo=-G.kSlatMaxDeg, hi=0.0, bind="SlatNorm", source="[SET] 20 Grad, DEFECTS.md #8"),
        dict(node="ctl.slat.r", parent="ctl.wingsweep.r",
             origin=(G.kPivotX + G.kPanelRootU + 0.05,
                     -G.panel_le_v(G.kPanelRootU + 0.05)
                     - G.kSlatChordFrac * G.panel_chord(G.kPanelRootU + 0.05), zp),
             axis=slat_axis, lo=-G.kSlatMaxDeg, hi=0.0, bind="SlatNorm",
             source="[SET] 20 Grad, DEFECTS.md #8"),
        dict(node="ctl.flap.l", parent="ctl.wingsweep.l",
             origin=(-(G.kPivotX + G.kPanelRootU + 0.05),
                     -(G.panel_te_v(G.kPanelRootU + 0.05)
                       - G.kFlapChordFrac * G.panel_chord(G.kPanelRootU + 0.05)), zp),
             axis=(-flap_axis[0], flap_axis[1], flap_axis[2]),
             lo=0.0, hi=G.kFlapMaxDeg, bind="FlapNorm", source="[SET] 25 Grad, DEFECTS.md #8"),
        dict(node="ctl.flap.r", parent="ctl.wingsweep.r",
             origin=(G.kPivotX + G.kPanelRootU + 0.05,
                     -(G.panel_te_v(G.kPanelRootU + 0.05)
                       - G.kFlapChordFrac * G.panel_chord(G.kPanelRootU + 0.05)), zp),
             axis=flap_axis, lo=0.0, hi=G.kFlapMaxDeg, bind="FlapNorm",
             source="[SET] 25 Grad, DEFECTS.md #8"),
        dict(node="ctl.spoiler.l", parent="ctl.wingsweep.l",
             origin=(-(G.kPivotX + G.kSpoilerU[0]),
                     -(G.panel_le_v(G.kSpoilerU[0]) + 0.42 * G.panel_chord(G.kSpoilerU[0])),
                     zp + 0.02),
             axis=(-flap_axis[0], flap_axis[1], flap_axis[2]),
             lo=0.0, hi=G.kSpoilerMaxDeg, bind="RollCmd",
             source="[WEB] die MiG-23 rollt mit STOERKLAPPEN, nicht mit Querrudern; 45 Grad [SET]"),
        dict(node="ctl.spoiler.r", parent="ctl.wingsweep.r",
             origin=(G.kPivotX + G.kSpoilerU[0],
                     -(G.panel_le_v(G.kSpoilerU[0]) + 0.42 * G.panel_chord(G.kSpoilerU[0])),
                     zp + 0.02),
             axis=flap_axis, lo=0.0, hi=G.kSpoilerMaxDeg, bind="RollCmd",
             source="[WEB] wie links"),
        dict(node="ctl.taileron.l", parent=None, origin=(-0.30, y_tail, zt),
             axis=tail_axis_l, lo=-G.kTailMaxDeg, hi=G.kTailMaxDeg, bind="PitchCmd+RollCmd",
             source="[WEB] Nick UND Roll ueber dieselben Flaechen; +-20 Grad [SET]"),
        dict(node="ctl.taileron.r", parent=None, origin=(0.30, y_tail, zt),
             axis=tail_axis_r, lo=-G.kTailMaxDeg, hi=G.kTailMaxDeg, bind="PitchCmd+RollCmd",
             source="[WEB] wie links"),
        dict(node="ctl.rudder", parent=None,
             origin=(0.0, yr_te + (1.0 - G.kRudderChordFrac) * 0.0, zr),
             axis=fin_hinge_axis, lo=-G.kRudderMaxDeg, hi=G.kRudderMaxDeg, bind="YawCmd",
             source="[SET] +-25 Grad, DEFECTS.md #16"),
        dict(node="ctl.ventral", parent=None, origin=(0.0, G.kVentralY[1], G.z_side(2612.0)),
             axis=(0.0, 1.0, 0.0), lo=0.0, hi=G.kVentralFoldDeg, bind="GearPosition",
             source="[WEB] 'the fin hinged sideways when the landing gear was extended'; "
                    "90 Grad [SET]"),
        dict(node="ctl.airbrake.l", parent=None, origin=(-0.20, G.kAirbrakeY + 0.42,
                                                        G.z_side(2592.0)),
             axis=(-1.0, 0.0, 0.0), lo=0.0, hi=G.kAirbrakeMaxDeg, bind="SpeedbrakeNorm",
             source="[SET] 45 Grad, DEFECTS.md #17"),
        dict(node="ctl.airbrake.r", parent=None, origin=(0.20, G.kAirbrakeY + 0.42,
                                                         G.z_side(2592.0)),
             axis=(1.0, 0.0, 0.0), lo=0.0, hi=G.kAirbrakeMaxDeg, bind="SpeedbrakeNorm",
             source="[SET] 45 Grad, DEFECTS.md #17"),
        dict(node="canopy", parent=None, origin=(0.0, G.y_side(G.kCanopyRearPx),
                                                 G.z_side(G.kCanopySillPx)),
             axis=(1.0, 0.0, 0.0), lo=0.0, hi=G.kCanopyOpenDeg, bind="CanopyNorm",
             source="[BP] Scharnier hinten; 42 Grad [SET]"),
        dict(node="gear.nose", parent=None, origin=(0.0, G.kNoseGearY, G.z_side(2600.0)),
             axis=(1.0, 0.0, 0.0), lo=0.0, hi=95.0, bind="GearPosition",
             source="[SET] Einzug nach hinten, 95 Grad, DEFECTS.md #12"),
        dict(node="gear.main.l", parent=None, origin=(-0.42, G.kMainGearY + 0.72,
                                                      G.z_side(2600.0)),
             axis=(0.0, -1.0, 0.0), lo=0.0, hi=88.0, bind="GearPosition",
             source="[SET] Einzug nach innen, 88 Grad, DEFECTS.md #12"),
        dict(node="gear.main.r", parent=None, origin=(0.42, G.kMainGearY + 0.72,
                                                      G.z_side(2600.0)),
             axis=(0.0, 1.0, 0.0), lo=0.0, hi=88.0, bind="GearPosition",
             source="[SET] wie links"),
    ]


# ================================================================ Blender-Anbindung

kMat = {
    # [SET] Alle Materialien. Kein Anstrich hat eine Norm; die Werte sind PBR-plausible
    # Schaetzungen fuer die genannten Oberflaechen (DEFECTS.md #18).
    "skin":       dict(rgb=(0.615, 0.630, 0.615), rough=0.42, metal=0.55),
    "frame":      dict(rgb=(0.300, 0.310, 0.320), rough=0.55, metal=0.70),
    "dark":       dict(rgb=(0.030, 0.032, 0.035), rough=0.85, metal=0.05),
    "nozzle":     dict(rgb=(0.255, 0.235, 0.215), rough=0.62, metal=0.90),
    # Die Haube der MiG-23 ist stark getoent. alpha 0.20 war so klar, dass die Kanzel im
    # gerenderten Bild nur noch als zwei Buegel zu sehen war — ein Material, das das Bauteil
    # unsichtbar macht, ist fuer ein Sichtasset falsch, egal wie plausibel die Zahl klingt.
    "glass":      dict(rgb=(0.470, 0.560, 0.520), rough=0.040, metal=0.00, alpha=0.55),
    "tyre":       dict(rgb=(0.045, 0.045, 0.048), rough=0.92, metal=0.00),
    "dielectric": dict(rgb=(0.240, 0.255, 0.235), rough=0.60, metal=0.00),
}
kMatCollapse = {"frame": "skin", "dielectric": "skin", "nozzle": "dark"}     # L2/L3


def make_material(key):
    m = bpy.data.materials.new("%s_%s" % (kPrefix, key))
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    s = kMat[key]
    a = s.get("alpha", 1.0)
    b.inputs["Base Color"].default_value = (*s["rgb"], a)
    b.inputs["Roughness"].default_value = s["rough"]
    b.inputs["Metallic"].default_value = s["metal"]
    if a < 1.0:
        b.inputs["Alpha"].default_value = a
        m.blend_method = 'BLEND'
    m.use_backface_culling = True
    return m


def to_blender(me, mats, parent, inv):
    bm = bpy.data.meshes.new(me.name)
    bm.from_pydata(me.v, [], [list(f) for f in me.f])
    bm.validate(verbose=False)
    bm.polygons.foreach_set("use_smooth", [True] * len(bm.polygons))
    want = sum(len(f) for f in me.f)
    if len(bm.loops) == want and len(bm.polygons) == len(me.f):
        bm.normals_split_custom_set([n for poly in me.n for n in poly])
    else:
        print("  WARN %s: Blender hat das Netz umgebaut (%d/%d Ecken) -> keine eigenen Normalen"
              % (me.name, len(bm.loops), want))
    bm.materials.append(mats[me.mat])
    o = bpy.data.objects.new(me.name, bm)
    bpy.context.collection.objects.link(o)
    o.parent = parent
    if inv is not None:
        o.matrix_local = inv
    return o


def empty(name, parent, mat4):
    import mathutils
    e = bpy.data.objects.new(name, None)
    e.empty_display_size = 0.35
    bpy.context.collection.objects.link(e)
    if parent is not None:
        e.parent = parent
    e.matrix_local = mathutils.Matrix(mat4)
    return e


def _mat4(origin, axis):
    r = _rot_to(axis)
    m = np.eye(4)
    m[:3, :3] = r
    m[:3, 3] = origin
    return m


# ---------------------------------------------------------------- Zuordnung Koerper -> Knoten

def owner(name):
    """Welcher Gelenkknoten traegt diesen Koerper?"""
    for pre, nd in (("ctl.slat.l", "ctl.slat.l"), ("ctl.slat.r", "ctl.slat.r"),
                    ("ctl.flap.l", "ctl.flap.l"), ("ctl.flap.r", "ctl.flap.r"),
                    ("ctl.spoiler.l", "ctl.spoiler.l"), ("ctl.spoiler.r", "ctl.spoiler.r"),
                    ("wing.l", "ctl.wingsweep.l"), ("wing.r", "ctl.wingsweep.r"),
                    ("ctl.taileron.l", "ctl.taileron.l"), ("ctl.taileron.r", "ctl.taileron.r"),
                    ("ctl.rudder", "ctl.rudder"), ("ctl.ventral", "ctl.ventral"),
                    ("ctl.airbrake.l", "ctl.airbrake.l"), ("ctl.airbrake.r", "ctl.airbrake.r"),
                    ("canopy.", "canopy"),
                    ("gear.nose", "gear.nose"), ("gear.door.nose", "gear.nose"),
                    ("gear.main.l", "gear.main.l"), ("gear.door.main.l", "gear.main.l"),
                    ("gear.main.r", "gear.main.r"), ("gear.door.main.r", "gear.main.r")):
        if name.startswith(pre):
            return nd
    return None


def _pair(group, me):
    """Ein Steuerbord-Koerper und sein exaktes Spiegelbild."""
    return [(group, me), (group, mirror(me, mirror_name(me.name)))]


def build_bodies(cfg):
    """Die EINE Quelle: eine geordnete Liste (Gruppe, Netz). Reihenfolge fest -> Determinismus.
    Jede seitliche Baugruppe wird EINMAL gebaut und gespiegelt."""
    out = [("airframe", fuselage(cfg)), ("airframe", pitot(cfg))]
    out += _pair("airframe", glove(cfg))
    for m in inlet(cfg):
        out += _pair("airframe", m)
    out += _pair("wing", wing_panel(cfg))
    for m in wing_controls(cfg):
        out += _pair("wing", m)
    for m in fin(cfg):
        out.append(("tail", m))
    out += _pair("tail", taileron(cfg))
    out.append(("tail", ventral(cfg)))
    out.append(("airframe", nozzle(cfg)))
    for m in canopy(cfg):
        out.append(("airframe", m))
    for m in gear_nose(cfg):
        out.append(("gear", m))
    for m in gear_main(cfg):
        out += _pair("gear", m)
    for m in stores_sym(cfg):
        out.append(("stores", m))
    for m in stores_side(cfg):
        out += _pair("stores", m)
    return [(g, orient(m)) for g, m in out]


def build_lod(cfg, out_dir, keep_blend=False):
    import mathutils
    clock = {}
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bodies = build_bodies(cfg)
    clock['bau'] = time.time() - t0

    print("--- %s  seg=%d fus=%d span=%d af=%d detail=%d"
          % (cfg["name"], cfg["seg"], cfg["fus"], cfg["span"], cfg["af"], cfg["detail"]))
    fails, per_body = [], []
    for _, me in bodies:
        bad, st = check_body(me)
        per_body.append((me.name, st))
        if bad:
            fails.append("%s: %s" % (me.name, "; ".join(bad)))
    clock['selbstpruefung'] = time.time() - t0 - clock['bau']

    genus = sorted(set(s["genus"] for _, s in per_body))
    vol_sum = sum(s["volume"] for _, s in per_body)
    lo = np.min([np.array(m.bbox()[0]) for _, m in bodies], axis=0)
    hi = np.max([np.array(m.bbox()[1]) for _, m in bodies], axis=0)

    merged = Mesh("merged", None)
    for _, m in bodies:
        off = len(merged.v)
        merged.v += m.v
        merged.f += [tuple(i + off for i in f) for f in m.f]
        merged.n += m.n
    mv = signed_volume(merged)
    mlo, mhi = merged.bbox()
    d_vol = abs(mv - vol_sum)
    d_box = float(max(np.abs(np.array(mlo) - lo).max(), np.abs(np.array(mhi) - hi).max()))
    merge_ok = d_vol <= 1e-9 * max(abs(vol_sum), 1.0) and d_box <= 1e-9
    if not merge_ok:
        fails.append("merge: dV=%.3e  dBox=%.3e" % (d_vol, d_box))

    # ------------------------------------------------------------ erst BAUEN und AUSLIEFERN
    made = {}
    for key in sorted(kMat):
        eff = kMatCollapse.get(key, key) if cfg["single_mat"] else key
        if eff not in made:
            made[eff] = make_material(eff)
    mats = {k: made[kMatCollapse.get(k, k) if cfg["single_mat"] else k] for k in kMat}

    root = empty("%s_%s" % (kPrefix, cfg["name"]), None, np.eye(4))
    groups = {g: empty(g, root, np.eye(4)) for g in ("airframe", "wing", "tail", "gear", "stores")}
    # node_table() gibt JEDE Gelenkmatrix in WELTKOORDINATEN. Blenders matrix_local ist
    # relativ zum Elternteil, also muss die Elternwelt HERAUSGERECHNET werden. Runde 1
    # multiplizierte sie stattdessen HINEIN (world = m_par @ m), und weil der Koerper danach
    # inv(m) als lokale Matrix bekam, stand er um m_par^2 verschoben in der Datei: sechs
    # Klappenkoerper bis 3.3 m neben dem Flugzeug, 1.33 m unter der Aufstandsebene.
    nodes, invs, world_of = {}, {}, {}
    for nd in node_table():
        m = _mat4(nd["origin"], nd["axis"])
        p = nd["parent"]
        local = m if not p else (np.linalg.inv(world_of[p]) @ m)
        nodes[nd["node"]] = empty(nd["node"], nodes[p] if p else root, local)
        world_of[nd["node"]] = m
        invs[nd["node"]] = np.linalg.inv(m)

    tris = 0
    for g, me in bodies:
        nd = owner(me.name)
        parent = nodes[nd] if nd is not None else groups[g]
        inv = mathutils.Matrix(invs[nd].tolist()) if nd is not None else None
        to_blender(me, mats, parent, inv)
        tris += sum(len(f) - 2 for f in me.f)

    t1 = time.time()
    path = os.path.join(out_dir, "%s_%s.glb" % (kAsset, cfg["name"]))
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False,
                              export_yup=True, export_apply=False, export_normals=True,
                              export_texcoords=False, export_materials='EXPORT')
    clock['export'] = time.time() - t1

    # ------------------------------------------------------------ dann DIE DATEI messen
    t1 = time.time()
    node_world, scene_meshes = read_glb(path)
    by_name = {m.name: m for m in scene_meshes}
    scene = [(g, by_name[me.name]) for g, me in bodies if me.name in by_name]
    clock['rueckgelesen'] = time.time() - t1
    slo = np.min([np.array(m.bbox()[0]) for _, m in scene], axis=0)
    shi = np.max([np.array(m.bbox()[1]) for _, m in scene], axis=0)
    place, place_fails = check_placement(bodies, node_table(), node_world, scene_meshes)
    fails += place_fails
    print("  PLATZ %d Knoten, %d Koerper aus %s zurueckgelesen; schlechtester Knoten %.2e m / "
          "%.2e Grad (%s), schlechteste Koerperbox %.2e m (%s)  %s"
          % (place["nodes"], place["bodies"], os.path.basename(path),
             place["worst_node_origin_m"], place["worst_node_axis_deg"],
             place["worst_node_origin"], place["worst_body_bbox_m"], place["worst_body"],
             "OK" if place["passed"] else "DURCHGEFALLEN"))

    vol_of = {m.name: abs(s["volume"]) * 1e6 for (_, m), (_, s) in zip(bodies, per_body)}
    t1 = time.time()
    all_ov = check_overlap([(m.name, m) for _, m in scene], vol_of)
    clock['durchdringung'] = time.time() - t1
    joints, bounded, ov, tiny = [], [], [], []
    for a, b, cm3, nn, rel in all_ov:
        vmin = min(vol_of.get(a, 0.0), vol_of.get(b, 0.0))
        q = cm3 / max(vmin, 1e-9)
        row = (a, b, cm3, nn, rel, q)
        cap = joint_cap(a, b)
        if cap is not None and q <= cap:
            (bounded if nn == 0 else joints).append(row)
        elif cm3 >= kOverlapMinCm3:
            ov.append(row)
        else:
            tiny.append(row)
    ov_sum = sum(x[2] for x in ov)
    if ov:
        fails.append("Durchdringung: %d Koerperpaare, %.1f cm3" % (len(ov), ov_sum))
        for a, b, cm3, nn, rel, q in ov[:10]:
            print("  DURCHDRINGUNG %-26s x %-26s %9.1f cm3  n=%d  dV/V %.1f %%  Anteil %.1f %%"
                  % (a, b, cm3, nn, 100.0 * rel, 100.0 * q))
    print("  MERGE  V_teile %.6f  V_verschmolzen %.6f  dV %.2e  dBox %.2e  %s"
          % (vol_sum, mv, d_vol, d_box, "OK" if merge_ok else "ABWEICHUNG"))
    if tiny:
        print("  DURCHDRINGUNG %d detektor-positive Paare unter der Urteilsschwelle %.1f cm3 "
              "(zusammen %.2f cm3) — gemeldet, nicht geschluckt"
              % (len(tiny), kOverlapMinCm3, sum(x[2] for x in tiny)))
    print("  DURCHDRINGUNG %d ungewollt (%.1f cm3), %d erklaerte Anschluesse gemessen "
          "(%.0f cm3), %d durch den Schnittquader BEWIESEN  %s"
          % (len(ov), ov_sum, len(joints), sum(x[2] for x in joints), len(bounded),
             "OK" if not ov else "DURCHGEFALLEN"))

    t1 = time.time()
    norm_rows, norm_fails = check_norms(scene, cfg["name"])
    clock['norm'] = time.time() - t1
    judged = cfg["name"] == kLod[0]["name"]
    if judged:
        fails += norm_fails
    n_checked = sum(1 for r in norm_rows if r["passed"] is not None)
    print("  NORM %d Masse AM AUSGELIEFERTEN .glb gegen [PUB]/[BP] gemessen, %d ohne Bezug, "
          "%d ausserhalb  (%s)  %s"
          % (n_checked, len(norm_rows) - n_checked, len(norm_fails),
             "geurteilt" if judged else "nur gemessen, L0 urteilt",
             "OK" if not norm_fails else "ABWEICHUNG"))
    for f in norm_fails:
        print("  NORM %-9s %s" % ("FEHLER" if judged else "Naeherung", f))

    if fails:
        for f in fails:
            print("  PRUEFUNG FEHLER  %s" % f)
    print("  PRUEFUNG %d Koerper: dicht/Windung/Normalen/T-Stoesse/Verschweissung/Merge  %s"
          % (len(bodies), "OK" if not fails else "DURCHGEFALLEN (%d)" % len(fails)))

    print("  ZEIT  " + "  ".join("%s %.1f s" % (k, v) for k, v in clock.items()))
    used = sorted(set(mats[k].name for k in kMat))
    print("  ASSET %-3s %-24s %8d B %7d Tri %d Mat  x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]"
          % (cfg["name"], os.path.basename(path), os.path.getsize(path), tris, len(used),
             slo[0], shi[0], slo[1], shi[1], slo[2], shi[2]))
    if keep_blend:
        bpy.ops.wm.save_as_mainfile(
            filepath=os.path.join(out_dir, "%s_%s.blend" % (kAsset, cfg["name"])))
    return dict(lod=cfg["name"], file=os.path.basename(path), segments=cfg["seg"],
                triangles=tris, bytes=os.path.getsize(path), bodies=len(bodies),
                materials=used, checks_failed=fails, norms=norm_rows, genus=genus,
                placement=place, volume_sum_m3=round(vol_sum, 6),
                merge=dict(v_parts=round(vol_sum, 6), v_merged=round(mv, 6),
                           d_volume=d_vol, d_bbox=d_box, passed=bool(merge_ok)),
                overlaps=[dict(a=a, b=b, cm3=round(c, 2), grid=n, converged_rel=round(r, 4),
                               fraction=round(q, 4)) for a, b, c, n, r, q in ov],
                overlaps_below_verdict=dict(
                    threshold_cm3=kOverlapMinCm3, pairs=len(tiny),
                    cm3=round(sum(x[2] for x in tiny), 3),
                    pairs_listed=[dict(a=a, b=b, cm3=round(c, 4)) for a, b, c, _, _, _ in tiny]),
                joints=dict(pairs=len(joints), cm3=round(sum(x[2] for x in joints), 1),
                            bounded_pairs=len(bounded),
                            worst=[dict(a=a, b=b, fraction=round(q, 4), cap=joint_cap(a, b))
                                   for a, b, c, n, r, q in sorted(joints, key=lambda x: -x[5])[:8]],
                            note=("Absichtlich geteiltes Volumen an ANSCHLUESSEN. Ein Flugzeug "
                                  "ist kein Tank: ein Holm steckt im Rumpf. Die Kappe steht je "
                                  "Paar in kJoint; alles ausserhalb der Liste ist ein Defekt.")),
                bbox={k: [round(float(slo[i]), 4), round(float(shi[i]), 4)]
                      for i, k in enumerate("xyz")},
                size_m={k: round(float(shi[i] - slo[i]), 4) for i, k in enumerate("xyz")},
                _bodies=scene)


# ================================================================ Umschaltweiten und Sidecar

def switch_table(stats):
    """Umschaltweiten HERGELEITET: eine Stufe darf fallen, sobald das feinste Merkmal der
    NAECHSTEN unter etwa ein Pixel faellt. Treiber ist die Rundungsabweichung der Rumpf-n-Ecke
    (der einzige Fehler, der sich geschlossen rechnen laesst); was WEGFAELLT, steht als
    Diagnose daneben, und das Urteil faellt check_silhouette bei genau dieser Weite."""
    def area(me):
        a = 0.0
        for f in me.f:
            p = [np.array(me.v[i]) for i in f]
            for i in range(1, len(p) - 1):
                a += 0.5 * float(np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])))
        return a

    r_fus = max(w * G.kPxM for _, _, _, w in G.kFusStations)
    steps, run = [], 0.0
    for i, st in enumerate(stats):
        if i + 1 >= len(stats):
            steps.append(dict(lod=st["lod"], driver=None, feature_m=None, max_range_m=None,
                              drops=[], note="letzte Stufe, keine Umschaltweite"))
            continue
        cur = {m.name: m for _, m in st["_bodies"]}
        nxt = set(m.name for _, m in stats[i + 1]["_bodies"])
        lost = {k: area(cur[k]) for k in cur if k not in nxt}
        seg = stats[i + 1]["segments"]
        feat = G.ring_error(r_fus, seg)
        run = max(run, feat / G.kPixelAngle)
        steps.append(dict(lod=st["lod"], driver="Rundung n=%d am groessten Rumpfradius %.3f m"
                          % (seg, r_fus), feature_m=round(feat, 5), max_range_m=round(run, 1),
                          lost_bodies=len(lost),
                          lost_cauchy_m={k: round(math.sqrt(v / 4.0), 4)
                                         for k in sorted(lost, key=lambda x: -lost[x])[:6]
                                         for v in [lost[k]]},
                          drops=sorted(lost, key=lambda x: -lost[x])[:8]))
    return steps


def sidecar(out_dir, stats, sil, steps, seconds):
    nodes = node_table()
    doc = {
        "asset": kAsset,
        "name": "Mikoyan-Gurevich MiG-23MLD 'Flogger-K'",
        "unit_scale_m": 1.0,
        "origin": ("y = 0 ist die DREHZAPFENSTATION der Schwenkfluegel, z = 0 die "
                   "TRIEBWERKSACHSE, x = 0 die Symmetrieebene. Der Zapfen ist der einzige "
                   "Punkt des Risses, der in beiden Ansichten UND in beiden gezeichneten "
                   "Pfeilstellungen unabhaengig belegt ist. Die Aufstandsebene liegt bei "
                   "z = %.4f m." % G.kGroundZ),
        "axes": "glTF +Y oben / -Z vorwaerts; gebaut in +X rechts / +Y vorwaerts / +Z oben",
        "built_sweep_deg": G.kSweepDefault,
        "used_by": ("doc/modules/air/catalogue.md §mig23 (Zeile `mig23`), "
                    "sim/test/modules/air/FBAirAnchors.h, "
                    "mods/f22/doc/substitutions.md (alle acht MiG-27-Sorties)."),
        "sources": {
            "PUB": ("Brassey's world aircraft & systems directory 1996/97, S. 73-75, ueber "
                    "https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23 Abschnitt "
                    "'Specifications (MiG-23MLD)'. Dieselbe Quelle traegt die A1..A8-Anker "
                    "der Katalogzeile dieses Baums."),
            "BP": ("Dreiseitenriss MiG-23M, 4000 x 2785 px, mit Massstabsbalken. "
                   "https://drawingdatabase.com/wp-content/uploads/2014/03/"
                   "mikoyan-gurevich-mig-23-mpd-2.png "
                   "(Uebersicht https://drawingdatabase.com/mikoyan-gurevich-mig-23/). "
                   "Jede [BP]-Zahl in mig23_geometry.py nennt ihre Pixelkoordinate."),
            "WEB": ("https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23 — Schwenkstellungen "
                    "16/45/72 Grad und die vierte der MLD bei 33 Grad; Rollsteuerung ueber "
                    "Stoerklappen statt Querruder; klappbare Bauchflosse; Wegfall der "
                    "Rueckenflosse bei der ML."),
        },
        "calibration": {
            "m_per_px": G.kPxM,
            "source": ("DER MASSSTABSBALKEN DES RISSES: Strichmitten x = 100.0 und 1106.0, "
                       "1006.0 px fuer 5 m. Er ist der einzige Massstab dieses Risses, der "
                       "nicht von einer veroeffentlichten Zahl abhaengt, die er nachher "
                       "pruefen soll."),
            "m_per_px_from_swept_span": round(G.kPxMFromSwept, 8),
            "m_per_px_from_spread_span": round(G.kPxMFromSpread, 8),
            "why_not_the_spans": (
                "Runde 1 nahm die Anpassung an BEIDE veroeffentlichten Spannweiten. Das trug "
                "auf einer um 36 px falsch gelesenen Fluegelspitze (Zeile 2194 statt 2231.5; "
                "2194 ist die Fuge der Fluegelendkappe und laeuft mitten durch den Fluegel). "
                "Mit der richtigen Spitze verlangen die beiden Spannweiten Massstaebe, die "
                "%.2f %% auseinanderliegen — es gibt keinen, der beide traegt."
                % (100 * (G.kPxMFromSwept / G.kPxMFromSpread - 1.0))),
            "check_length_m": round(G.kLengthMeasured, 4),
            "check_length_rel": round(G.kLengthErrRel, 5),
            "check_note": ("Radomspitze (3408) bis hinterster Zellenpunkt (36.5, Spitze der "
                           "Bremsschirmverkleidung) gegen [PUB] 16.7 m. Mit dem Massstab aus "
                           "Runde 1 waeren es +0.57 %. Die 16.7 m sind OHNE Pitotrohr: mit "
                           "Rohr misst der Riss 17.842 m."),
        },
        "reference_dimensions_m": {
            "length_airframe": G.kLength,
            "length_overall_with_probe": round(G.kLengthOverall, 4),
            "span_spread": G.kSpanSpread,
            "span_swept": G.kSpanSwept,
            "height_on_gear": G.kHeight,
            "wing_area_spread_m2": G.kWingAreaSpread,
            "wing_area_swept_m2": G.kWingAreaSwept,
            "airfoil": "TsAGI SR-12S, 6.5 % Wurzel / 5.5 % Spitze [PUB]; die DICKE ist gebaut, "
                       "die Profilform ist NACA-symmetrisch [SET] (DEFECTS.md #14)",
        },
        "wing_kinematics": {
            "pivot_m": [round(G.kPivotX, 4), round(G.kPivotY, 4), round(G.kPivotZ, 4)],
            "tip_radius_m": round(G.kPanelTipR, 4),
            "tip_theta_deg_at_16": round(G.kPanelTipTheta, 4),
            "detents_deg": list(G.kSweepDetents),
            "le_sweep_at_16_deg": round(G.kPanelLeSweep, 3),
            "le_sweep_documented_deg": G.kLeSweepDocDeg,
            "te_sweep_at_16_deg": round(G.kPanelTeSweep, 3),
            "root_chord_m": round(G.kPanelRootChord, 4),
            "tip_chord_m": round(G.kPanelTipChord, 4),
            "exposed_area_both_m2": round(2 * G.kPanelExposedArea, 3),
            "derivation": (
                "Zapfenlage seitlich als Mittel ZWEIER kreisgefitteter Zapfensymbole "
                "(293.4 / 326.1 px von der Mittellinie — der Riss ist achtern verzogen, "
                "s. DEFECTS.md #11). r und theta0 GESCHLOSSEN aus den beiden "
                "veroeffentlichten Spannweiten geloest, beide werden dadurch exakt "
                "getroffen. Die PLANFORM kommt dagegen aus dem Riss: VK- und HK-Pfeilung "
                "sind Geradenfits an der GESPREIZTEN Fluegelhaelfte (Residuen 0.48 und "
                "0.57 px ueber 565 bzw. 1005 Zeilen), die Randbogensehne ist an der "
                "gemessenen Randbogenzeile 2231.5 abgegriffen. Die VK-Pfeilung faellt mit "
                "%.3f Grad heraus, die Literatur nennt 18 Grad 45' = 18.750."
                % G.kPanelLeSweep),
            "drawing_vs_published": (
                "Der Riss ist mit sich selbst konsistent: die VK-Geraden beider gezeichneter "
                "Stellungen haben denselben Zapfenabstand (209.4 gegen 208.3 px) und stehen "
                "55.570 statt 56.000 Grad zueinander. Er ist gleichmaessig zu gross — Zapfen "
                "plus Randbogenabstand liegt bei %.0f Grad %+.2f %% und bei %.0f Grad %+.2f %% "
                "ueber [PUB]. Der gezeichnete Randbogen liegt %.4f m vom Zapfen, die aus [PUB] "
                "geloeste Spitze bei %.4f m (%.2f %%). Gebaut wird [PUB]."
                % (G.kPanelCheckPct[0][0], G.kPanelCheckPct[0][1],
                   G.kPanelCheckPct[1][0], G.kPanelCheckPct[1][1],
                   G.kPanelTipUDrawn, G.kPanelLeTipU,
                   100 * (G.kPanelTipUDrawn / G.kPanelLeTipU - 1.0))),
        },
        "nodes": [dict(node=n["node"], parent=n["parent"],
                       origin=[round(float(c), 4) for c in n["origin"]],
                       axis=[round(float(c), 5) for c in n["axis"]],
                       limits_deg=[n["lo"], n["hi"]], detents_deg=n.get("detents"),
                       bind=n["bind"], source=n["source"]) for n in nodes],
        "node_convention": ("Die Gelenkachse IST die lokale +X des Knotens, der Drehpunkt seine "
                            "Herkunft — kein zweiter Datensatz daneben. Ein Knoten LIEST einen "
                            "publizierten Wert; er schreibt nie in die Simulation."),
        "lods": [{k: v for k, v in s.items() if not k.startswith("_")} for s in stats],
        "silhouette": sil,
        "switch": steps,
        "build": {"seconds": round(seconds, 1),
                  "determinism": "zweimal bauen, Bytes vergleichen (doc/assets.md §4)"},
        "open_defects": "DEFECTS.md",
    }
    p = os.path.join(out_dir, "%s.asset.json" % kAsset)
    with open(p, "w") as fh:
        json.dump(doc, fh, indent=2, ensure_ascii=True, sort_keys=False)
        fh.write("\n")
    print("SIDECAR %s" % p)


def main():
    t_start = time.time()
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--lod", default="")
    ap.add_argument("--blend", action="store_true")
    ap.add_argument("--refine-sil", action="store_true")
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    levels = [kLod[int(a.lod)]] if a.lod else kLod
    stats = [build_lod(c, a.out, a.blend) for c in levels]
    rc = 1 if any(s["checks_failed"] for s in stats) else 0
    if len(stats) > 1:
        steps = switch_table(stats)
        t0 = time.time()
        rng = [x["max_range_m"] or 0.0 for x in steps]
        sil, ok = check_silhouette(stats, rng)
        print("  ZEIT  silhouette %.1f s" % (time.time() - t0))
        rc = rc or (0 if ok else 1)
        if a.refine_sil:
            t0 = time.time()
            refine_silhouette(stats, rng, kSilRefine)
            print("  ZEIT  silhouette-verfeinerung %.1f s" % (time.time() - t0))
    else:
        sil, steps = dict(passed=None, rows=[]), []
    if len(stats) == len(kLod):
        sidecar(a.out, stats, sil, steps, time.time() - t_start)
    print("ERGEBNIS %s  (%.1f s)" % ("ALLES GRUEN" if rc == 0 else "DURCHGEFALLEN",
                                     time.time() - t_start))
    return rc


if __name__ == "__main__":
    sys.exit(main())

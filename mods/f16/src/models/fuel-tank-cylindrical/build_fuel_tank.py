#!/usr/bin/env python3
"""FlightBox — fuel-tank-cylindrical. Vier LOD-Stufen aus EINER parametrischen Quelle.

    /Applications/Blender.app/Contents/MacOS/Blender --background --python build_fuel_tank.py -- \
        --out mods/f16/src/models/fuel-tank-cylindrical [--lod 0] [--blend]

WARUM DREHKOERPER STATT ZYLINDER. Ein Zylinder ist die Schale und sonst nichts. Was diesen Tank
lesbar macht, sind die Anbauten mit ihren Normmassen: Kopfwinkel, Zwischen-Windring, umlaufende
Treppe, Podest an der Dachkante, Mannloecher. Alle Koerper entstehen aus geschlossenen (r,z)-Profilen,
die um die Achse gedreht werden — dieselbe Quelle fuer alle vier Stufen, nur groeber abgetastet.

WARUM KEINE TEXTUR. doc/render/visual-target.md §1: 60 GB/s Bandbreite gegen 2.5..4 TFLOPS ALU.
Bandbreite ist der Mangel, Rechenzeit der Ueberschuss. Ein Tank ist eine gestrichene Blechflaeche —
was ihn traegt, sind Kanten (Rundnaehte, Winkel, Ringe) und die sind GEOMETRIE, nicht Textur. Rost
und Beschriftung gehoeren in einen prozeduralen Shader des Renderers, nicht in eine Datei, die je
Bild ueber den Bus muss. Das Netz liefert vier PBR-Materialien ohne ein einziges Bild.

WARUM DIE SILHOUETTE NICHT SPRINGT. Jeder Ring bekommt nicht den Kreisradius, sondern den
Umkreisradius der gleich-UMFAENGIGEN n-Ecke (G.ring_radius). Nach Cauchy ist die ueber alle
Blickrichtungen gemittelte Schattenbreite eines konvexen Koerpers gleich Umfang/pi — gleicher Umfang
heisst also exakt gleiche mittlere Silhouettenbreite auf allen vier Stufen. Der Rest ist Restfehler
zweiter Ordnung, und der wird gemessen (check_silhouette).

KOORDINATEN. Blender (+X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m); der glTF-Export dreht auf
+Y-oben / -Z-vorwaerts. z = 0 ist das umgebende Gelaende.
"""
import argparse
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy                                        # noqa: E402
import numpy as np                                # noqa: E402
import fuel_tank_geometry as G                    # noqa: E402

TAU = math.pi * 2.0
EPS_WELD = 1.0e-5          # 10 um — unter jeder Fertigungstoleranz, ueber jedem float32-Rauschen

kAsset = "fuel-tank-cylindrical"
kPrefix = "fuel_tank"

# detail: 2 = alles, 1 = ohne Schrauben, 0 = Treppe als geschlossenes Band statt Einzelstufen.
#
# WARUM ALLE STUFEN DIESELBEN KOERPER BAUEN. Die Silhouette ist eine SUMME. Die F-16 hat das in
# Runde 4 teuer gelernt: 19 einzeln unsichtbare Koerper fielen zusammen weg und rissen 58 % aus der
# Frontsilhouette. Hier faellt deshalb kein Koerper, der den Umriss begrenzt — Treppe, Podest,
# Gelaender und Ringe stehen auf allen vier Stufen. Was faellt, sind Abtastung und Schrauben.
kLod = [
    dict(name="L0", seg=96, tube=12, detail=2, rails=True, single_mat=False),
    dict(name="L1", seg=48, tube=8, detail=2, rails=True, single_mat=False),
    dict(name="L2", seg=24, tube=6, detail=1, rails=True, single_mat=True),
    # L3 BEHAELT DIE GELAENDER — Runde 3 liess sie fallen, weil der Kritiker dafuer 0.389 % mass.
    # Mit dem Dachrandgelaender stimmt diese Rechnung nicht mehr: gemessen kostet das Fallenlassen
    # jetzt 0.30 pp (2.099 gegen 1.800 % bei L2->L3) aus einem Budget, das die Polygonteilung
    # allein schon zu 1.36 pp verbraucht. Bezahlt wird das mit 1100 Dreiecken auf der billigsten
    # Stufe. Was auf L3 faellt, ist Abtastung: seg 20, tube 4, ein Material.
    dict(name="L3", seg=20, tube=4, detail=1, rails=True, single_mat=True),
]


# ================================================================ Netzbau

class Mesh:
    """Rohnetz: Punkte, Vielecke, Eck-Normalen. Kein bpy, damit die Pruefungen exakt bleiben."""

    def __init__(self, name, mat):
        self.name = name
        self.mat = mat
        self.v = []
        self.f = []
        self.n = []          # je Vieleck eine Liste von Eck-Normalen

    def add(self, pts, normals):
        i0 = len(self.v)
        self.v += [tuple(p) for p in pts]
        self.f.append(tuple(range(i0, i0 + len(pts))))
        self.n.append([tuple(x) for x in normals])
        return self

    def bbox(self):
        a = np.array(self.v)
        return a.min(axis=0), a.max(axis=0)


def _seg_normal(p, q):
    """Aussennormale einer Profilkante (r,z), 90 Grad im Uhrzeigersinn gedreht."""
    dr, dz = q[0] - p[0], q[1] - p[1]
    ln = math.hypot(dr, dz)
    if ln < 1e-12:
        return None
    return (dz / ln, -dr / ln)


def profile_normals(prof):
    """Je Profilpunkt und angrenzender Kante eine 2D-Normale. Laengs des Profils wird NICHT
    gemittelt, quer dazu (um die Achse) schon.

    WARUM NICHT GEMITTELT — der zweite Befund am ersten Bild. Kein Segment dieses Profils naehert
    eine Kurve an: jedes IST eine Flaeche (Zylinder, Kegel, Kreisring). Wer trotzdem ueber den
    Knick mittelt, kippt die Normale an den ENDEN eines Segments — und ein Schalenschuss ist
    2.44 m lang. Die Interpolation zog daraus je Schuss einen 2-m-Helligkeitsverlauf; im ersten
    Bild lagen vier breite Baender auf der Schale, wo drei 60-mm-Schweissraupen sein sollten.
    Ein gerades Profilsegment hat eine KONSTANTE Normale. Das ist keine Naeherung, das ist die
    Flaeche.
    """
    m = len(prof)
    seg = [_seg_normal(prof[i], prof[(i + 1) % m]) for i in range(m)]
    out = []
    for i in range(m):
        a, b = seg[i - 1], seg[i]
        out.append((a or b, b or a))
    return seg, out


def revolve(name, mat, prof, n, phi0=0.0, phi1=TAU, r_corr=True):
    """Geschlossenes (r,z)-Profil um die z-Achse drehen. Punkte mit r=0 werden zu Polen kollabiert.

    r_corr: Radien auf die umfangsgleiche n-Ecke skalieren (s. Kopfkommentar).
    """
    me = Mesh(name, mat)
    full = abs((phi1 - phi0) - TAU) < 1e-12
    steps = n if full else max(2, int(round(n * (phi1 - phi0) / TAU)))
    k = (G.ring_radius(1.0, n) if r_corr else 1.0)
    phis = [phi0 + (phi1 - phi0) * j / steps for j in range(steps + 1)]
    cs = [(math.cos(p), math.sin(p)) for p in phis]
    if full:
        cs[-1] = cs[0]
    _, vn = profile_normals(prof)
    m = len(prof)

    def pt(i, j):
        r = prof[i][0] * k
        return (r * cs[j][0], r * cs[j][1], prof[i][1])

    def nrm(i, side, j):
        nr, nz = vn[i][side]
        return (nr * cs[j][0], nr * cs[j][1], nz)

    for i in range(m):
        a, b = i, (i + 1) % m
        ra, rb = prof[a][0], prof[b][0]
        if ra < 1e-9 and rb < 1e-9:
            continue
        for j in range(steps):
            j2 = j + 1
            if ra < 1e-9:            # Pol am Segmentanfang -> Dreieck
                # Die Ringkante MUSS (b_j2 -> b_j) lauten, wie im Viereckfall darunter — sonst
                # laufen Faecher und Vierecke gleichsinnig und der Koerper bekommt an jedem
                # Polring eine Randkante. Genau das war der erste Befund der Selbstpruefung.
                me.add([pt(a, j), pt(b, j2), pt(b, j)],
                       [nrm(b, 0, j), nrm(b, 0, j2), nrm(b, 0, j)])
            elif rb < 1e-9:
                me.add([pt(a, j), pt(a, j2), pt(b, j)],
                       [nrm(a, 1, j), nrm(a, 1, j2), nrm(a, 1, j)])
            else:
                me.add([pt(a, j), pt(a, j2), pt(b, j2), pt(b, j)],
                       [nrm(a, 1, j), nrm(a, 1, j2), nrm(b, 0, j2), nrm(b, 0, j)])
    if not full:
        # Stirnflaechen des Sektors, damit der Koerper geschlossen bleibt.
        # Die Seitenvierecke hinterlassen in der Ebene j=0 die Kante (b -> a), also GEGEN die
        # Profilrichtung; der Deckel dort muss sie mit (a -> b) schliessen, und der bei j=steps
        # umgekehrt. Runde 2 hatte beide vertauscht -> acht Randkanten je Sektor.
        for j, rev in ((0, False), (steps, True)):
            nx = (-cs[j][1], cs[j][0], 0.0)
            poly = [pt(i, j) for i in range(m) if prof[i][0] > 1e-9]
            if rev:
                poly = poly[::-1]
            else:
                nx = (-nx[0], -nx[1], 0.0)
            me.add(poly, [nx] * len(poly))
    return me


def _face_normal(pts):
    a = np.array(pts)
    nrm = np.zeros(3)
    for i in range(len(a)):
        p, q = a[i], a[(i + 1) % len(a)]
        nrm += np.cross(p, q)
    ln = np.linalg.norm(nrm)
    return (nrm / ln) if ln > 1e-12 else np.array([0.0, 0.0, 1.0])


def solid(name, mat, loops):
    """Prisma aus zwei deckungsgleich indizierten Ringen (loops[0] unten, loops[1] oben).

    ACHTUNG: Deckel und Boden werden als EIN Vieleck abgelegt und ueberall in diesem Skript als
    Faecher zerlegt (Pruefung, Rasterer, Blender). Das ist nur fuer KONVEXE Ringe richtig — ein
    Kreisringsektor ist es nicht. Solche Koerper entstehen deshalb ueber sweep(), dessen
    Querschnitt immer konvex ist.
    """
    me = Mesh(name, mat)
    lo, hi = loops
    k = len(lo)
    nb = _face_normal(lo[::-1])
    nt = _face_normal(hi)
    me.add(lo[::-1], [nb] * k)
    me.add(hi, [nt] * k)
    for i in range(k):
        j = (i + 1) % k
        q = [lo[i], lo[j], hi[j], hi[i]]
        nf = _face_normal(q)
        me.add(q, [nf] * 4)
    return me


def box(name, mat, corners_lo, corners_hi):
    return solid(name, mat, (corners_lo, corners_hi))


def arc_frames(r, phis, z_of=None):
    """Pfad laengs eines Bogens: Ursprung auf r, u = radial nach aussen, v = z."""
    out = []
    for p in phis:
        z = z_of(p) if z_of else 0.0
        out.append((np.array([r * math.cos(p), r * math.sin(p), z]),
                    np.array([math.cos(p), math.sin(p), 0.0]),
                    np.array([0.0, 0.0, 1.0])))
    return out


def normal_frames(pts):
    """Frenet-artiger Rahmen fuer einen geneigten Pfad: u = radial (steht immer senkrecht auf der
    Tangente einer Schraublinie um die z-Achse), v = T x u.

    WARUM NICHT EINFACH (radial, z). Der Querschnitt liegt in der von u und v aufgespannten Ebene.
    Steht diese Ebene nicht senkrecht auf dem Pfad, wird das Rohr geschert: bei 36.8 Grad
    Treppenneigung wuerde ein 42-mm-Handlauf 52 mm hoch. Bei einem SENKRECHTEN Pfad (Pfosten) faellt
    die Ebene sogar mit der Pfadrichtung zusammen — der Koerper hat dann Volumen null. Beides hat
    die Selbstpruefung gemeldet, bevor ein Bild existierte.
    """
    out = []
    n = len(pts)
    for i, p in enumerate(pts):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, n - 1)]
        t = np.array(b) - np.array(a)
        t = t / (np.linalg.norm(t) or 1.0)
        u = np.array([p[0], p[1], 0.0])
        nu = np.linalg.norm(u)
        u = (u / nu) if nu > 1e-9 else np.array([1.0, 0.0, 0.0])
        u = u - t * float(np.dot(u, t))
        u = u / (np.linalg.norm(u) or 1.0)
        out.append((np.array(p), u, np.cross(t, u)))
    return out


def sweep(name, mat, path, section, close_ends=True, round_section=False):
    """Querschnitt laengs eines Pfades ziehen.

    section: entweder EIN Querschnitt (Liste von (u,v)) oder einer je Pfadpunkt — letzteres
    braucht die Treppenwange, die unten flach auslaufen muss, um nach [API T.5-18 Pkt.10] frei
    vom Boden zu bleiben.
    round_section: der Querschnitt naehert einen Kreis an -> Eck-Normalen radial statt flaechig,
    damit ein Handlauf rund aussieht und nicht als Prisma.
    """
    me = Mesh(name, mat)
    secs = section if section and isinstance(section[0][0], (list, tuple)) else [section] * len(path)
    k = len(secs[0])
    rings, rnorm = [], []
    for (o, u_ax, v_ax), sec in zip(path, secs):
        o, u_ax, v_ax = np.array(o), np.array(u_ax), np.array(v_ax)
        rings.append([tuple(o + u * u_ax + v * v_ax) for u, v in sec])
        if round_section:
            rnorm.append([tuple((u * u_ax + v * v_ax) / (math.hypot(u, v) or 1.0)) for u, v in sec])
    for a in range(len(rings) - 1):
        for i in range(k):
            j = (i + 1) % k
            q = [rings[a][i], rings[a][j], rings[a + 1][j], rings[a + 1][i]]
            if round_section:
                me.add(q, [rnorm[a][i], rnorm[a][j], rnorm[a + 1][j], rnorm[a + 1][i]])
            else:
                nf = _face_normal(q)
                me.add(q, [nf] * 4)
    if close_ends:
        n0 = _face_normal(rings[0][::-1])
        me.add(rings[0][::-1], [n0] * k)
        n1 = _face_normal(rings[-1])
        me.add(rings[-1], [n1] * k)
    return me


def ngon(n, r, cx=0.0, cy=0.0, phase=0.0):
    return [(cx + r * math.cos(phase + TAU * i / n), cy + r * math.sin(phase + TAU * i / n))
            for i in range(n)]


def signed_volume(me):
    v = 0.0
    for f in me.f:
        p = [np.array(me.v[i]) for i in f]
        for i in range(1, len(p) - 1):
            v += float(np.dot(p[0], np.cross(p[i], p[i + 1])))
    return v / 6.0


def orient(me):
    """Windung deterministisch nach aussen drehen: negatives Divergenzintegral heisst umgestuelpt.

    Die Eck-Normalen werden danach NICHT pauschal negiert, sondern einzeln in den Halbraum ihrer
    Flaechennormale gedreht. Pauschales Negieren war falsch: round_section liefert die Normale
    absolut (vom Rohrmittelpunkt nach aussen), unabhaengig von der Windung. Bei einem Sweep mit
    linkshaendigem Rahmen (Podestgelaender: u = radial, v = z, Pfad tangential) standen Flaeche und
    Normale danach genau gegeneinander — von der Selbstpruefung mit cos = -0.966 gemeldet.
    """
    if signed_volume(me) < 0.0:
        me.f = [tuple(reversed(f)) for f in me.f]
        me.n = [list(reversed(nn)) for nn in me.n]
    for k, (f, nn) in enumerate(zip(me.f, me.n)):
        fn = _face_normal([me.v[i] for i in f])
        me.n[k] = [tuple(-np.array(x)) if float(np.dot(x, fn)) < 0.0 else x for x in nn]
    return me


# ================================================================ Selbstpruefungen (doc/assets.md §3.1)

def _edges(me):
    d = {}
    for fi, f in enumerate(me.f):
        for i in range(len(f)):
            d.setdefault((f[i], f[(i + 1) % len(f)]), []).append(fi)
    return d


def check_body(me):
    """Alle Invarianten eines Koerpers. Gibt eine Liste von Befunden zurueck (leer = sauber)."""
    bad = []
    v = np.array(me.v)

    # geschweisst: keine zwei verschiedenen Punkte naeher als EPS_WELD
    key = np.round(v / EPS_WELD).astype(np.int64)
    uniq, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    dup = int((cnt > 1).sum())

    # Windung und Rand: jede gerichtete Kante genau einmal, jede mit ihrer Gegenkante
    de = _edges(me)
    multi = [e for e, fs in de.items() if len(fs) > 1]
    # Kanten auf VERSCHWEISSTEN Indizes vergleichen: die Profile erzeugen je Vieleck eigene Punkte.
    wd = {}
    for e, fs in de.items():
        wd.setdefault((int(inv[e[0]]), int(inv[e[1]])), 0)
        wd[(int(inv[e[0]]), int(inv[e[1]]))] += len(fs)
    boundary = [e for e, c in wd.items() if (e[1], e[0]) not in wd]
    nonmanifold = [e for e, c in wd.items() if c != 1]
    if multi:
        bad.append("gerichtete Kante mehrfach: %d" % len(multi))
    if boundary:
        bad.append("Randkanten (Loch): %d" % len(boundary))
    if nonmanifold:
        bad.append("nicht-mannigfaltige Kanten: %d" % len(nonmanifold))

    # Euler: chi = V - E + F, Geschlecht g = (2-chi)/2 muss ganzzahlig und >= 0 sein
    nv, ne, nf = len(uniq), len(wd) // 2, len(me.f)
    chi = nv - ne + nf
    genus = (2 - chi) / 2.0
    if genus < 0 or abs(genus - round(genus)) > 1e-9:
        bad.append("Euler-Charakteristik %d -> Geschlecht %.1f" % (chi, genus))

    # Normalen: endlich, Einheitslaenge, Richtung stimmt mit der Windung ueberein
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

    # entartete Vielecke
    degen = 0
    for f in me.f:
        p = [np.array(me.v[i]) for i in f]
        a = 0.0
        for i in range(1, len(p) - 1):
            a += 0.5 * float(np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])))
        if a < 1e-12:
            degen += 1
    if degen:
        bad.append("entartete Vielecke: %d" % degen)

    # T-Stoesse: ein Punkt im INNEREN einer Kante, an der er nicht haengt
    pos = uniq.astype(np.float64) * EPS_WELD
    tj = 0
    seen = set()
    for (a, b) in wd:
        if (b, a) in seen:
            continue
        seen.add((a, b))
        p0, p1 = pos[a], pos[b]
        d = p1 - p0
        ln2 = float(np.dot(d, d))
        if ln2 < 1e-18:
            continue
        w = pos - p0
        t = (w @ d) / ln2
        m = (t > 1e-6) & (t < 1.0 - 1e-6)
        if not m.any():
            continue
        proj = p0 + np.outer(t[m], d)
        dist = np.linalg.norm(pos[m] - proj, axis=1)
        tj += int((dist < EPS_WELD * 2.0).sum())
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
    """Kreuzprodukt komponentenweise. Bitgleich zu np.cross, aber ohne dessen Umsortiererei —
    np.cross war im Durchdringungstor allein 2.2 von 10.4 Sekunden."""
    ax, ay, az = a[..., 0], a[..., 1], a[..., 2]
    bx, by, bz = b[..., 0], b[..., 1], b[..., 2]
    return np.stack([ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx], axis=-1)


def _cull(v, tri, lo, hi, d):
    """Nur die Dreiecke behalten, die der Strahlenschlauch der Probepunkte ueberhaupt treffen kann.

    Ohne das kostet ein Paar (Schale, Stufe) 3000 Dreiecke mal 1000 Punkte — der Lauf brach nach
    zwei Minuten ab. Der Schlauch ist der Probequader, laengs d bis zum Netzende gezogen.
    """
    tlo = np.minimum(np.minimum(v[tri[:, 0]], v[tri[:, 1]]), v[tri[:, 2]])
    thi = np.maximum(np.maximum(v[tri[:, 0]], v[tri[:, 1]]), v[tri[:, 2]])
    span = float(np.max(thi - tlo + (v.max(0) - v.min(0))))
    slo = np.minimum(lo, lo + d * span) - 1e-6
    shi = np.maximum(hi, hi + d * span) + 1e-6
    m = (thi >= slo).all(1) & (tlo <= shi).all(1)
    return tri[m]


def inside(pts, v, tri):
    """Punkte im Inneren eines geschlossenen Netzes: Strahl entlang +X, Kreuzungen zaehlen.

    Vektorisiert ueber ALLE Dreiecke, damit der Test je Koerper eine numpy-Runde kostet.
    """
    a, b, c = v[tri[:, 0]], v[tri[:, 1]], v[tri[:, 2]]
    e1, e2 = b - a, c - a
    # Schraege, feste Strahlrichtung: ein achsparalleler Strahl trifft bei einem Drehkoerper
    # reihenweise exakt auf Kanten und zaehlt dort doppelt oder gar nicht. Deterministisch, aber
    # falsch — in Runde 2 als 0.7 cm3 Phantom-Ueberlappung zwischen Pfosten und Kopfwinkel gesehen.
    d = kRayDir
    h = _cross(d, e2)
    det = (e1 * h).sum(1)
    ok = np.abs(det) > 1e-14
    inv = np.zeros_like(det)
    inv[ok] = 1.0 / det[ok]
    hit = np.zeros(len(pts), dtype=np.int64)
    chunk = max(1, int(4e6 // max(len(tri), 1)))
    for i0 in range(0, len(pts), chunk):
        s = pts[i0:i0 + chunk, None, :] - a[None, :, :]
        u = (s * h).sum(2) * inv
        q = _cross(s, e1)
        vv = (q * d).sum(2) * inv
        t = (e2 * q).sum(2) * inv
        m = ok & (u >= 0.0) & (u <= 1.0) & (vv >= 0.0) & (u + vv <= 1.0) & (t > 1e-9)
        hit[i0:i0 + chunk] = m.sum(1)
    return (hit % 2) == 1


# Geschweisste Verbindungen: hier teilen sich zwei Bauteile ABSICHTLICH Volumen, weil sie in
# Wirklichkeit ein Schweisspunkt sind — ein Gelaenderpfosten steckt im Holm, nicht daneben.
# Die Erlaubnis ist keine Blankovollmacht: sie gilt je Paar bis kJointCapCm3. Wer eine Naht
# groesser macht, faellt weiterhin durch. Alles, was NICHT in dieser Liste steht, ist ein Defekt.
kJoint = (("access.stair.post", "access.stair.stringer"),
          ("access.stair.post", "access.stair.rail"),
          ("access.stair.post", "access.stair.tread"),
          ("access.platform.post", "access.platform.rail"),
          ("access.platform.post", "access.platform.toeboard"),
          ("access.stair.post", "access.stair.band"),
          ("access.stair.rail", "access.platform.rail"),
          ("access.stair.rail", "access.platform.post"),
          ("access.stair.stringer", "access.platform.post"),
          ("access.platform.endrail", "access.platform.rail"),
          ("access.platform.endrail", "access.platform.post"),
          # Derselbe Schweisspunkt wie Podestpfosten x Podestholm, nur am Dachrand. Das ist keine
          # Erweiterung der Liste STATT besserer Geometrie: ein Gelaenderpfosten steckt im Holm.
          ("access.roofrail.post", "access.roofrail.rail"),
          ("tank.manhole.shell.bolt", "tank.manhole.shell.cover"),
          ("tank.manhole.roof.bolt", "tank.manhole.roof.cover"))
# Die Kappe je Schweisspunkt ist RELATIV, nicht 250 cm3 aus der Luft: ein Schweisspunkt darf
# hoechstens diesen Anteil des KLEINEREN Bauteils verzehren. Mehr heisst, die Koerper sind falsch
# gesetzt, nicht verschweisst. Der Anteil selbst ist [SET] — er skaliert aber mit dem Modell,
# waehrend eine absolute Zahl das nicht tut, und die schlechteste Quote wird gemeldet.
kJointFrac = 0.10

# [SET] Schwelle des URTEILS im Durchdringungstor, s. DEFECTS.md. Unterhalb davon ist ein
# Detektortreffer kein Defekt, sondern eine Beruehrung an der Messaufloesung. Die Schwelle
# ENTFERNT nichts aus dem Bericht — sie entscheidet nur, ob eine Zeile den Bau durchfallen laesst.
kOverlapMinCm3 = 0.05

# [SET] Schweissraupe der Rundnaehte, s. DEFECTS.md. Sie steht ueber die Wand hinaus und muss
# deshalb auch beim Freischnitt der Treppenstufen mitgerechnet werden.
kBeadH, kBeadW = 0.003, 0.014

# Schraege, feste Strahlrichtung fuer den Einschlusstest: ein achsparalleler Strahl trifft bei
# einem Drehkoerper reihenweise exakt auf Kanten und zaehlt dort doppelt oder gar nicht.
kRayDir = np.array([1.0, 0.0137, 0.0071])
kRayDir = kRayDir / np.linalg.norm(kRayDir)


def is_joint_pair(a, b):
    """Ist dieses Paar als Schweisspunkt ERKLAERT? Reine Namensfrage."""
    return any((a.startswith(x) and b.startswith(y)) or (a.startswith(y) and b.startswith(x))
               for x, y in kJoint)


def is_joint(a, b, cm3, vmin_cm3):
    return cm3 <= kJointFrac * vmin_cm3 and is_joint_pair(a, b)


def _seg_hits_tri(p0, p1, v, tri):
    """Kreuzt IRGENDEINE der Strecken p0->p1 irgendein Dreieck? Moeller-Trumbore, t in [0,1]."""
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
        q = _cross(sv, e1[None, :, :])
        w = (d[:, None, :] * q).sum(2) * inv
        t = (e2[None, :, :] * q).sum(2) * inv
        m = ok & (u >= 0) & (u <= 1) & (w >= 0) & (u + w <= 1) & (t > 1e-9) & (t < 1.0 - 1e-9)
        if m.any():
            return True
    return False


def _body_edges(me):
    e = set()
    for f in me.f:
        for i in range(len(f)):
            a, b = f[i], f[(i + 1) % len(f)]
            e.add((a, b) if a < b else (b, a))
    return np.array(sorted(e), dtype=np.int64)


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


def check_overlap(bodies, vol_cm3, n0=12, nmax=48, rtol=0.05):
    """doc/assets.md §3.1 "mesh merge", in der Fassung, die etwas MESSEN kann.

    ZWEI STUFEN, weil eine Rasterprobe kein Detektor ist. Runde 2 lieferte samples=10 aus und las
    fuer (Ringmauer, Treppenband) 0.00 cm3 — wahr sind elf Liter. Ein Gitter kann eine duenne
    Durchdringung ganz verfehlen und eine sehr kleine erfinden.
      1. DETEKTOR, exakt: kreuzt eine Kante des einen Koerpers ein Dreieck des anderen (oder liegt
         ein Punkt des einen ganz im anderen)? Beides ist ein Ja/Nein ohne Abtastfehler.
      2. SCHAETZER, konvergent: nur wo der Detektor Ja sagt, wird das Volumen bestimmt und die
         Aufloesung verdoppelt, bis sich der Wert um weniger als rtol aendert. Gemeldet wird der
         Wert MIT seiner Konvergenz, nie ohne.
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
            c1 = t1[((np.maximum.reduce([v1[t1[:, k]] for k in range(3)]) >= lo).all(1)
                     & (np.minimum.reduce([v1[t1[:, k]] for k in range(3)]) <= hi).all(1))]
            c2 = t2[((np.maximum.reduce([v2[t2[:, k]] for k in range(3)]) >= lo).all(1)
                     & (np.minimum.reduce([v2[t2[:, k]] for k in range(3)]) <= hi).all(1))]
            def _emask(v, e):
                a, b = v[e[:, 0]], v[e[:, 1]]
                return ((np.maximum(a, b) >= lo).all(1) & (np.minimum(a, b) <= hi).all(1))

            m1, m2 = _emask(v1, e1), _emask(v2, e2)
            touch = (_seg_hits_tri(v1[e1[m1, 0]], v1[e1[m1, 1]], v2, c2)
                     or _seg_hits_tri(v2[e2[m2, 0]], v2[e2[m2, 1]], v1, c1))
            if not touch:
                # Vollstaendige Umschliessung: keine Kante kreuzt, aber ein Punkt liegt drin.
                cand = v1[(v1 >= lo).all(1) & (v1 <= hi).all(1)][:64]
                if not len(cand) or not inside(cand, v2, _cull(v2, t2, lo, hi, kRayDir)).any():
                    continue
            # STUFE 1.5, und sie ist STRENGER als der Schaetzer, nicht schwaecher: das gemeinsame
            # Volumen kann den Schnittquader nicht ueberschreiten. Liegt schon dieser Quader unter
            # der Kappe eines ERKLAERTEN Schweisspunktes, ist die Kappe bewiesen statt geschaetzt,
            # und die teure Gitterprobe entfaellt. Fuer alles, was NICHT in kJoint steht — also
            # jeden moeglichen Defekt — wird weiterhin immer gemessen.
            box = float(np.prod(hi - lo)) * 1e6
            vmin = min(vol_cm3.get(n1, 0.0), vol_cm3.get(n2, 0.0))
            if box <= kJointFrac * vmin and is_joint_pair(n1, n2):
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
    # NICHTS WIRD MEHR GESCHLUCKT. Runde 3 warf hier jeden Treffer unter min_cm3 weg, und der
    # Bericht las sich, als haette der Detektor nichts gefunden — auf L3 verschwanden so 57
    # detektor-positive Paare. Der Detektor ist EXAKT; was er findet, verlaesst diese Funktion.
    # min_cm3 bleibt die Schwelle des URTEILS (unbelegt, s. DEFECTS.md) und wird beim Aufrufer
    # angewandt, sichtbar und mit Zahl.
    return sorted(hits, key=lambda x: -x[2])


# ================================================================ Normpruefung AM GEBAUTEN NETZ

class Norms:
    """Jede Normzahl wird am fertigen Netz NACHGEMESSEN, nicht an den Eingaben nachgerechnet.

    WARUM DIESE KLASSE EXISTIERT. Bis Runde 3 stand im Bauskript genau ein assert, und der lautete
    sinngemaess `assert (a + b) - b >= a` — eine Substitution, die nicht fehlschlagen KANN. Er
    meldete die lichte Podestbreite als eingehalten; am Netz waren es 618.0 statt der gerechneten
    610.0 mm, weil der Abzug eine Fussleiste mitzaehlte, die nach aussen sitzt. Norm gehalten, aber
    aus Versehen — und das war das sechste Mal in diesem Baum, dass eine Pruefung BERICHTET, ohne
    zu MESSEN.

    Die Regel, die daraus folgt und die dieses Skript ab jetzt durchzieht:
        Eine Pruefung in einem Bauskript misst am gebauten Netz, nie an den Eingaben.

    Es steht deshalb KEIN ausfuehrbares `assert` mehr in diesem Baum — weder hier noch in
    fuel_tank_geometry. Nicht aus Abneigung gegen das Schluesselwort, sondern weil eine
    Bandpruefung mehr kann: sie haelt beim ersten Verstoss nicht an, sondern misst alles und legt
    JEDE Zeile mit Wert, Grenze und Klausel im Sidecar ab; sie faellt nicht weg, wenn jemand mit
    -O startet; und sie unterscheidet das URTEIL (L0) vom blossen Messen (L1..L3). Ein Verstoss
    setzt den Rueckgabewert des Baus auf 1, genau wie ein gerissener assert es taete.

    Aus dem Netz kommen Punkte. Was daraus abgeleitet wird, ist entweder reine Geometrie (Radius,
    Bogen, Hoehendifferenz) oder benennt seine Umrechnung — z. B. die Ringkorrektur k(n), wobei n
    aus der Zahl der gebauten Traufenecken GEZAEHLT wird und nicht aus der Konfiguration kommt.
    """

    def __init__(self, bodies, lod):
        self.b = {m.name: m for _, m in bodies}
        self.lod = lod
        self.rows = []
        self.fails = []

    # -------------------------------------------------- Netzzugriff
    def has(self, pre):
        return any(n.startswith(pre) for n in self.b)

    def pick(self, pre):
        return [(n, self.b[n]) for n in sorted(self.b) if n.startswith(pre)]

    @staticmethod
    def cyl(me):
        """(Radius, Azimut, Hoehe) aller Punkte eines Koerpers."""
        v = np.array(me.v)
        return np.hypot(v[:, 0], v[:, 1]), np.arctan2(v[:, 1], v[:, 0]), v[:, 2]

    def polygon_n(self):
        """Umfangsteilung AUS DEM NETZ: die Zahl verschiedener Azimute des Traufenrings."""
        r, a, z = self.cyl(self.b["tank.roof"])
        rim = r > r.max() - 1e-6
        return len(np.unique(np.round(a[rim], 6)))

    # -------------------------------------------------- Buchfuehrung
    def band(self, key, value, lo, hi, unit, rule):
        # 1e-6 in der Einheit der Zeile: bei Millimeterzeilen ein Nanometer, bei Meterzeilen ein
        # Mikrometer. Beides liegt unter jeder Fertigungstoleranz und ueber dem Rauschen, das
        # Wurzeln und Divisionen an einer aus Punkten zurueckgerechneten Groesse hinterlassen —
        # mit 1e-9 fiel eine exakt eingehaltene 5-mm-Wand durch.
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
    """Alle Normmasse am gebauten Netz. Gibt (Zeilen, Fehler) zurueck.

    GEMESSEN WIRD AUF JEDER STUFE, GEURTEILT WIRD AUF L0 (s. build_lod). Der Grund ist keine
    Bequemlichkeit: L0 IST der Koerper, L1..L3 sind Naeherungen davon, deren Abweichung absichtlich,
    beziffert und vom Silhouettentor begrenzt ist. Eine Fertigungsnorm auf ein 20-Eck anzuwenden
    hiesse, die Fernsicht ueber die Bauform entscheiden zu lassen. Die Messwerte der groberen
    Stufen stehen trotzdem vollstaendig im Sidecar — verschwiegen wird nichts.
    """
    N = Norms(bodies, lod)
    n_poly = N.polygon_n()
    k = G.ring_radius(1.0, n_poly)          # Umkreiskorrektur der GEZAEHLTEN Polygonteilung
    N.note("polygon.segments", n_poly, "-", "aus dem Traufenring gezaehlt")

    # ---------------------------------------------------------- Schale, und daraus der Windring
    # DAS SCHALENPROFIL WIRD AUS DEM NETZ ZURUECKGEWONNEN, nicht in Baender geteilt. Ein Band
    # zwischen zwei Hoehen enthaelt gar keine Punkte — die Wand eines Schusses ist EINE gerade
    # Strecke ohne Zwischenpunkte — und wer das Band bis an die Stossfuge zieht, greift den
    # dickeren Schuss darunter und die Schweissraupe mit ab. Genau das tat die erste Fassung
    # dieser Pruefung: sie mass fuer jeden Schuss 6 mm statt 5 und rechnete daraus H1 = 14.92 statt
    # 9.46 m — ein Messfehler, der die Windpruefung LEICHTER machte.
    shell = N.b["tank.shell"]
    r, a, z = N.cyl(shell)
    r_in = float(r.min()) / k
    z0, z1 = float(z.min()), float(z.max())
    # Die Profilkanten kommen aus den FLAECHEN, nicht aus einer Sortierung. An jeder Stossfuge
    # liegen zwei Punkte auf derselben Hoehe (dicker Schuss darunter, duenner darueber); eine
    # Sortierung nach z stellt sie in beliebiger Reihenfolge auf und zerreisst die Waende. Die
    # Umdrehung erzeugt zwischen zwei Profilpunkten ein Viereck — dessen Kante bei Azimut 0 IST
    # die Profilkante.
    walls = set()
    for f in shell.f:
        pts = [(shell.v[i], i) for i in f]
        at0 = [p for p, i in pts if abs(math.atan2(p[1], p[0])) < 1e-9]
        for p in at0:
            for q in at0:
                rp, rq = math.hypot(p[0], p[1]), math.hypot(q[0], q[1])
                # Die INNENwand ist auch eine senkrechte Profilkante ueber die volle Hoehe; sie
                # hat keine Dicke und wuerde t_thin auf null (numerisch: -1e-10) druecken.
                if q[2] - p[2] > 0.5 and abs(rq - rp) < 1e-9 and rp > float(r.min()) + 1e-6:
                    walls.add((round(rp, 9), round(p[2], 9), round(q[2], 9)))
    walls = sorted(walls, key=lambda w: w[1])
    thk = [w[0] / k - r_in for w in walls]
    # Schussbreite = Abstand der Wandanfaenge; der naechste Wandanfang IST die Stossfuge.
    wid = [(walls[c + 1][1] if c + 1 < len(walls) else z1) - walls[c][1]
           for c in range(len(walls))]
    nc = len(walls)
    N.band("shell.courses", float(nc), 1.0, None, "Schuesse",
           "[API 5.6.1.2] aus den senkrechten Profilkanten des Netzes gezaehlt")
    t_thin = min(thk)
    d_nom = 2.0 * (r_in + thk[0] / 2.0)         # [API 5.6.1.1 Note 1] Mittellinie von Schuss 1
    N.band("shell.diameter", d_nom, G.kDiameter - 5e-4, G.kDiameter + 5e-4, "m",
           "[SIZE] 48 ft = 14.6304 m, Mittellinie des untersten Schusses [API 5.6.1.1 Note 1]")
    N.band("shell.thickness.bottom", thk[0] * 1000.0, 6.0, None, "mm",
           "[API 5.6.1.1 Fussnote 4] unterster Schuss >= 6 mm bei 3.2 m < D < 15 m")
    N.band("shell.thickness.thinnest", t_thin * 1000.0, 5.0, None, "mm",
           "[API 5.6.1.1 Tabelle, SI] D < 15 m -> 5 mm")
    N.band("shell.course_width", min(wid) * 1000.0, 1800.0, None, "mm",
           "[API 5.6.1.2] Schalenbleche mindestens 1800 mm Nennbreite")
    # [API 5.9.7.1 / 5.9.7.2 b] mit den GEMESSENEN Dicken und Breiten nachgerechnet.
    t_mm = t_thin * 1000.0
    h1 = 9.47 * t_mm * (t_mm / d_nom) ** 1.5 * (190.0 / G.kWindSpeedKmh) ** 2
    w_tr = sum(w * (t_thin / t) ** 2.5 for w, t in zip(wid, thk))
    N.note("wind.H1", h1, "m", "[API 5.9.7.1, SI] aus gemessener Dicke und gemessenem Durchmesser")
    N.note("wind.transformed_shell", w_tr, "m", "[API 5.9.7.2 a] aus gemessenen Schussbreiten")
    N.band("wind.girder_required", 1.0 if w_tr > h1 else 0.0, 0.0, 0.0, "0/1",
           "[API 5.9.7.2 b] Ring erst wenn transformierte Schale > H1 (V = %.0f km/h [5.2.1 k])"
           % G.kWindSpeedKmh)
    N.band("wind.girder_bodies", float(sum(1 for n in N.b if "girder" in n or "windring" in n)),
           0.0, 0.0, "Koerper", "kein Zwischenring gefordert -> keiner gebaut")

    # ---------------------------------------------------------- Dach
    rr, ar, zr = N.cyl(N.b["tank.roof"])
    rim = rr > rr.max() - 1e-6
    r_rim, z_rim, z_apex = float(rr[rim].mean()) / k, float(zr[rim].max()), float(zr.max())
    N.band("roof.slope", (z_apex - z_rim) / r_rim, 1.0 / 16.0, None, "-",
           "[API 5.10.4.1] Neigung 1:16 oder steiler; Traufenradius um k(n=%d) entzerrt" % n_poly)

    # ---------------------------------------------------------- Fundament und Bodenblech
    rw, _, zw = N.cyl(N.b["foundation.ringwall"])
    N.band("ringwall.thickness", (float(rw.max()) - float(rw.min())) / k * 1000.0, 300.0, None,
           "mm", "[API B.4.2.2] Ringmauer mindestens 300 mm dick")
    N.band("ringwall.depth", -float(zw.min()), 0.600, None, "m",
           "[API B.4.2.2] Unterkante mindestens 0.6 m unter Gelaende")
    rb, _, _ = N.cyl(N.b["tank.bottom_plate"])
    sel = z <= z0 + 1e-9
    N.band("bottom_plate.projection", (float(rb.max()) - float(r[sel].max())) / k * 1000.0,
           50.0, None, "mm", "[API 5.4.2] mindestens 50 mm Ueberstand ausserhalb der Schale")

    # ---------------------------------------------------------- Treppe
    tre = N.pick("access.stair.tread.")
    if tre:
        top, lead, rmid, wdt = [], [], [], []
        for _, me in tre:
            rt, at, zt_ = N.cyl(me)
            top.append(float(zt_.max()))
            # Vorderkante = kleinster Azimut; der Bogen der Treppe kreuzt die -pi/pi-Naht nicht,
            # wenn man ihn stetig fortsetzt.
            aa = np.unwrap(np.sort(at))
            lead.append(float(aa.min()))
            rmid.append(0.5 * (float(rt.min()) + float(rt.max())))
            wdt.append(float(rt.max()) - float(rt.min()))
        order = np.argsort(top)
        top = np.array(top)[order]
        lead = np.unwrap(np.array(lead)[order])
        rmid = np.array(rmid)[order]
        rise = np.diff(top)
        run = np.diff(lead) * (0.5 * (rmid[1:] + rmid[:-1]))
        N.band("stair.rise.uniform", float(rise.max() - rise.min()) * 1000.0, None, 1.0, "mm",
               "[API T.5-18 Pkt.4] 'Rises shall be uniform throughout the height'")
        N.band("stair.rise", float(rise.mean()) * 1000.0, None, 241.3, "mm",
               "[OSHA 1910.25(c)] Steigung hoechstens 9.5 in")
        N.band("stair.run", float(run.min()) * 1000.0, 241.3, None, "mm",
               "[API T.5-18 Pkt.4] >= 200 mm, [OSHA 1910.25(c)] >= 9.5 in = 241.3 mm")
        N.band("stair.2R_plus_r", float((2.0 * rise + run).mean()) * 1000.0, 610.0, 660.0, "mm",
               "[API T.5-18 Pkt.4] 2R + r zwischen 610 und 660 mm")
        ang = math.degrees(math.atan2(float(rise.mean()), float(run.mean())))
        N.band("stair.angle", ang, 30.0, 50.0, "Grad",
               "[API T.5-18 Pkt.3] hoechstens 50 Grad, [OSHA 1910.25(c)] mindestens 30 Grad")
        N.band("stair.width", min(wdt) * 1000.0, 710.0, None, "mm",
               "[API T.5-18 Pkt.2] Mindestbreite der Treppe 710 mm")
    else:
        N.skip("stair.*", "Stufe ohne Einzelstufen (detail=0)")

    posts = N.pick("access.stair.post.")
    if posts:
        base = []
        for _, me in posts:
            v = np.array(me.v)
            base.append(v[v[:, 2] <= v[:, 2].min() + 1e-9].mean(axis=0))
        base = np.array(base)
        d = np.linalg.norm(np.diff(base, axis=0), axis=1)
        N.band("stair.post_spacing", float(d.max()) * 1000.0, None, 2400.0, "mm",
               "[API T.5-18 Pkt.7] hoechstens 2400 mm, laengs der Neigung gemessen")
    else:
        N.skip("stair.post_spacing", "Stufe ohne Gelaender (rails=False)")

    if tre and "access.stair.rail.top" in N.b:
        rr2, ar2, zr2 = N.cyl(N.b["access.stair.rail.top"])
        ar2 = np.unwrap(ar2[np.argsort(ar2)])
        hs = []
        # Die letzten Stufen rampen auf Podesthoehe hoch [T.5-18 Pkt.6 'without offset'] und
        # gehoeren nicht in das 760..860-Band.
        for i in range(len(tre) - G.kStairRailRampSteps - 1):
            phi = float(np.unwrap(np.sort(N.cyl(tre[i][1])[1])).mean())
            _, aa, zz = N.cyl(N.b["access.stair.rail.top"])
            m = np.abs(np.unwrap(aa) - phi) < 0.4 * G.kStairDPhi
            if m.any():
                hs.append(float(zz[m].max()) - float(N.cyl(tre[i][1])[2].max()))
        if hs:
            N.band("stair.railing_height", min(hs) * 1000.0, 760.0, 860.0, "mm",
                   "[API T.5-18 Pkt.6] 760..860 mm senkrecht ueber der Stufenvorderkante")
            N.band("stair.railing_height_max", max(hs) * 1000.0, 760.0, 860.0, "mm",
                   "[API T.5-18 Pkt.6] dieselbe Grenze, oberer Rand der Messreihe")
    else:
        N.skip("stair.railing_height", "Stufe ohne Gelaender (rails=False)")

    # ---------------------------------------------------------- Podest
    fl = N.b["access.platform.floor"]
    rf, af, zf = N.cyl(fl)
    z_fl = float(zf.max())
    r_fi = float(rf.min())
    a_lo, a_hi = float(np.unwrap(np.sort(af)).min()), float(np.unwrap(np.sort(af)).max())
    # LICHTE Breite: der innerste Punkt IRGENDEINES anderen Koerpers, der ueber dem Podestboden
    # im Podestbogen steht, minus die Innenkante des Bodens. Das ist genau der Satz aus T.5-17
    # Pkt.2 "after making adjustments at all projections" — als Messung statt als Rechnung.
    # Die Breite eines Laufstegs ist ein QUERMASS. Am STIRNende schliesst ihn das Endgelaender
    # [T.5-17 Pkt.10], das radial ueber die ganze Breite laeuft — es ist die Wand, nicht ein
    # Vorsprung in den Weg. Beide Enden bleiben deshalb um eine Rohrbreite ausgespart; alles
    # dazwischen zaehlt.
    a_end = 1.5 * G.kRailTubeDia / r_fi
    clear, culprit = None, None
    for nm, me in N.b.items():
        if nm == fl.name:
            continue
        rq, aq, zq = N.cyl(me)
        aq = np.where(aq < a_lo - math.pi, aq + TAU, np.where(aq > a_hi + math.pi, aq - TAU, aq))
        m = ((zq >= z_fl - 1e-9) & (zq <= z_fl + G.kPlatformRailH + 1e-9)
             & (rq > r_fi + 1e-6) & (aq >= a_lo + a_end) & (aq <= a_hi - a_end))
        if not m.any():
            continue
        v = float(rq[m].min()) - r_fi
        if clear is None or v < clear:
            clear, culprit = v, nm
    N.band("platform.clear_width", clear * 1000.0, 610.0, None, "mm",
           "[API T.5-17 Pkt.2] 610 mm LICHT; engste Stelle am gebauten Netz gegen '%s'" % culprit)

    tb = N.b.get("access.platform.toeboard")
    if tb is not None:
        _, _, zt2 = N.cyl(tb)
        N.band("platform.toeboard_height", (float(zt2.max()) - float(zt2.min())) * 1000.0,
               75.0, None, "mm", "[API T.5-17 Pkt.5] Fussleiste mindestens 75 mm hoch")
        N.band("platform.toeboard_gap", (float(zt2.min()) - z_fl) * 1000.0, 0.0, 6.0, "mm",
               "[API T.5-17 Pkt.6] hoechstens 6 mm zwischen Bodenoberkante und Fussleiste")

    if "access.platform.rail.top" in N.b:
        _, _, zp = N.cyl(N.b["access.platform.rail.top"])
        N.band("platform.railing_height", (float(zp.max()) - z_fl) * 1000.0, 1070.0, 1070.0, "mm",
               "[API T.5-17 Pkt.4] 1070 mm, Oberkante [OSHA 1910.29(b)(1)]")
        _, _, zm = N.cyl(N.b["access.platform.rail.mid"])
        N.band("platform.midrail_height", (float(zm.max()) - z_fl) * 1000.0, 481.5, 588.5, "mm",
               "[API T.5-17 Pkt.7] etwa halbe Hoehe; [SET] 'etwa' = +-10 %")
        pp = N.pick("access.platform.post.")
        cen = np.array([np.array(me.v).mean(axis=0) for _, me in pp])
        d = np.linalg.norm(np.diff(cen, axis=0), axis=1)
        N.band("platform.post_spacing", float(d.max()) * 1000.0, None, 2400.0, "mm",
               "[API T.5-17 Pkt.8] groesster Pfostenabstand 2400 mm")
    else:
        N.skip("platform.railing_height", "Stufe ohne Gelaender (rails=False)")

    # ---------------------------------------------------------- Dachrandgelaender
    rp = N.pick("access.roofrail.post.")
    seats = []
    for _, me in rp:
        v = np.array(me.v)
        # GANZER Fussring, nicht ein Viertel der Punkte: ein unsymmetrischer Ausschnitt legt den
        # Schwerpunkt neben die Achse, und die Messung meldete daraufhin 0.28 mm Luft unter einem
        # Pfosten, der in Wirklichkeit auflag.
        seats.append(v[v[:, 2] <= v[:, 2].min() + 1e-9].mean(axis=0))
    seats = np.array(seats)
    d = np.linalg.norm(np.diff(seats, axis=0), axis=1)
    N.band("roofrail.post_spacing", float(d.max()) * 1000.0, None, 2400.0, "mm",
           "[API T.5-17 Pkt.8] groesster Pfostenabstand 2400 mm")
    N.band("roofrail.posts", float(len(rp)), 2.0, None, "Pfosten",
           "[DERIVED] ceil(Ringbogen / 2400 mm) + 1")
    _, _, zrt = N.cyl(N.b["access.roofrail.rail.top"])
    # Hoehe ueber der Dachflaeche, an der der Pfosten steht — beides am Netz.
    h_top = float(zrt.max()) - float(seats[:, 2].max())
    N.band("roofrail.height", h_top * 1000.0, 991.0, 1143.0, "mm",
           "[API T.5-17 Pkt.4] 1070 mm; [OSHA 1910.29(b)(1)] 42 in +- 3 in ueber der Standflaeche")
    # Steht jeder Pfosten wirklich AUF dem Dach? Abstand Fusspunkt zur gebauten Dachflaeche.
    gap = 0.0
    for s in seats:
        phi = math.atan2(s[1], s[0])
        gap = max(gap, abs(float(s[2]) - _roof_z(dict(seg=n_poly), phi, math.hypot(s[0], s[1]))))
    N.band("roofrail.seat_gap", gap * 1000.0, 0.0, 0.1, "mm",
           "[DERIVED] Pfostenfuss auf der gebauten Dachflaeche, nicht davor und nicht darin")

    # Der Ring laesst genau den Podestbogen aus und sonst nichts.
    ar_ring = np.concatenate([np.arctan2(np.array(me.v)[:, 1], np.array(me.v)[:, 0])
                              for _, me in rp])
    N.band("roofrail.coverage", float(np.ptp(np.unwrap(np.sort(ar_ring)))) * 180.0 / math.pi,
           (TAU - (a_hi - a_lo)) * 180.0 / math.pi - 2.0, 360.0, "Grad",
           "[OSHA 1910.28(b)(1)(i)] ungeschuetzte Kante -> Ring ueber den ganzen Umfang ausser Podest")
    return N.rows, N.fails


def flatten(bodies):
    """Punktfeld und Dreiecksindizes einer LOD-Stufe — EINMAL je Stufe, nicht je Ansicht.

    WARUM DAS DIE HALBE BAUZEIT WAR. Runde 3 baute die Dreiecksliste in jeder einzelnen Ansicht neu
    auf, in einer Python-Schleife ueber alle Vielecke aller Koerper. Bei 13 000 Dreiecken und
    2 x 361 Ansichten je Paar sind das zehn Millionen Schleifendurchlaeufe, und sie kosteten mehr
    als das Rastern selbst: gemessen 57.8 ms je Ansicht bei 1280 px, davon 11.5 ms Rasterung.
    Die Kosten haengen deshalb kaum an der Aufloesung — 81 px kosteten 16.2 ms, also 28 % von
    1280 px bei 0.4 % der Pixel. Wer das fuer eine Aufloesungsfrage haelt, optimiert am falschen
    Ende. Hier passiert es einmal; jede Ansicht ist danach reines numpy.
    """
    vs, ts, off = [], [], 0
    for me in bodies:
        vs.append(np.asarray(me.v, dtype=np.float64))
        for f in me.f:
            for i in range(1, len(f) - 1):
                ts.append((f[0] + off, f[i] + off, f[i + 1] + off))
        off += len(me.v)
    return np.concatenate(vs), np.asarray(ts, dtype=np.int64)


def raster(flat, u_ax, v_ax, res, ctr, size):
    """Orthografische Alpha-Maske ohne Renderer, zeilenweise und VOLL vektorisiert.

    Bitgleich zu der Fassung, die je Ansicht neu aufbaute (gegengeprueft ueber L0 und L3 bei
    324 und 1280 px), nur ohne deren Python-Schleife.
    """
    V, T = flat
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
    # bincount statt np.add.at: dieselben ganzzahligen Summen, aber np.add.at laeuft ungepuffert
    # und war allein ein Drittel der Rasterzeit.
    w = res * (res + 1)
    diff = (np.bincount(r_ * (res + 1) + a_, minlength=w)
            - np.bincount(r_ * (res + 1) + b_ + 1, minlength=w))
    return np.cumsum(diff[:w].reshape(res, res + 1), axis=1)[:, :res] > 0


def _views(k):
    """k Azimute plus die Draufsicht, als (Name, Winkel, u, v).

    k MUSS TEILERFREMD ZU JEDER POLYGONZAHL DER LEITER SEIN, sonst misst das Tor sich selbst.
    Runde 3 nahm 90 -> 180 -> 360 und hielt an, sobald sich der schlechteste Wert um weniger als
    0.02 pp aenderte. Bei L2->L3 lieferten 180, 360 UND 720 Azimute exakt 1.5631 % am exakt
    gleichen Azimut 54.000 Grad — nicht weil der Wert konvergiert war, sondern weil ggT(k, 20) = 20
    ist: das 20-Eck hat 18 Grad Periode, und 1 Grad, 2 Grad und 0.5 Grad tasten alle DIESELBEN
    18 Phasen ab. Bei 1440 Azimuten sprang der Wert um +0.031 pp, bei 0.05 Grad Raster auf 1.5938 %.
    Die "Konvergenz" war ein Alias. 361 = 19^2 ist teilerfremd zu 96, 48, 24 und 20, also wandert
    die Phase ueber alle Facetten.
    """
    out = []
    for j in range(k):
        a = TAU * j / k
        out.append(("az%08.4f" % math.degrees(a), a,
                    np.array([-math.sin(a), math.cos(a), 0.0]), np.array([0.0, 0.0, 1.0])))
    out.append(("top", -1.0, np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0])))
    return out


# ================================================================ Die Baugruppen

def shell(cfg):
    """Schale: vier Schuesse mit Rundnaehten, geschlossenes Rohr."""
    z0, z1 = G.kTankBaseZ, G.kTankBaseZ + G.kShellHeight
    ri = G.kShellInnerR
    bead_h, bead_w = kBeadH, kBeadW
    out = []
    for c in range(G.kCourses):
        ro = G.shell_outer(c)
        zb, zt = z0 + c * G.kCourseHeight, z0 + (c + 1) * G.kCourseHeight
        # Der erste Rundstoss ist ein DICKENSPRUNG (Fussnote 4): aussen springt die Wand um
        # 1.59 mm nach innen, weil die Schuesse innen buendig gestossen werden. Die uebrigen
        # Stoesse sind dickengleich — dort DARF kein Punkt doppelt stehen, sonst entstehen
        # Segmente der Laenge null und daraus entartete Vielecke. Genau das meldete die
        # Selbstpruefung: 192 entartete Vielecke, 576 nicht-mannigfaltige Kanten.
        if not out or abs(out[-1][0] - ro) > 1e-12:
            out.append((ro, zb))
        if cfg["detail"] >= 1 and c + 1 < G.kCourses:
            out += [(ro, zt - bead_w), (ro + bead_h, zt - bead_w * 0.4),
                    (ro + bead_h, zt + bead_w * 0.4)]
        if abs(out[-1][0] - ro) > 1e-12 or abs(out[-1][1] - zt) > 1e-12:
            out.append((ro, zt))
    prof = out + [(ri, z1), (ri, z0)]
    return revolve("tank.shell", "paint", prof, cfg["seg"])


def vertical_seams(cfg):
    """Senkrechte Stossnaehte der Schuesse.

    [API 5.1.5.2 b] "Vertical joints in adjacent shell courses shall not be aligned, but shall be
    offset from each other a minimum distance of 5t" — die Naehte EXISTIEREN also, und sie sind
    versetzt. Runde 2 modellierte nur die waagerechten. Das war nicht sparsam, sondern
    inkonsistent: beide Nahtscharen sind gleich gross, und das Nahtraster ist das Merkmal, an dem
    man einen geschweissten Lagertank ueberhaupt erkennt.
    """
    out = []
    if cfg["detail"] < 2:
        return out
    z0 = G.kTankBaseZ
    # Wo ein Verstaerkungsblech sitzt, laeuft keine Naht durch — das Blech deckt sie ab. Und der
    # oberste Schuss endet unter dem senkrechten Schenkel des Kopfwinkels.
    blocked = ((G.kManholePhi, G.kTankBaseZ + G.kShellManholeZ, G.kShellManholeReinf),
               (G.kNozzlePhi, G.kTankBaseZ + G.kNozzleZ, G.kNozzleReinfDia))
    for c in range(G.kCourses):
        zb, zt = z0 + c * G.kCourseHeight, z0 + (c + 1) * G.kCourseHeight
        if c == G.kCourses - 1:
            zt -= G.kTopAngleLeg
        for k in range(G.kPlatesPerCourse):
            phi = TAU * (k + (c % 2) * G.kSeamOffsetFrac) / G.kPlatesPerCourse
            if any(abs(((phi - bp + math.pi) % TAU) - math.pi) * G.kRadius < bd
                   and zb - bd < bz < zt + bd for bp, bz, bd in blocked):
                continue
            hw = 0.5 * G.kBeadW / G.kRadius
            ro = _shell_wall_max(cfg, phi - hw, phi + hw, 0.5 * (zb + zt), bead=False)
            prof = [(ro, zb + G.kBeadW), (ro + G.kBeadH, zb + G.kBeadW),
                    (ro + G.kBeadH, zt - G.kBeadW), (ro, zt - G.kBeadW)]
            out.append(revolve("tank.seam.v.%d.%d" % (c, k), "paint", prof, cfg["seg"],
                               phi - hw, phi + hw, r_corr=False))
    return out


def bottom_plate(cfg):
    """[API 5.4.2] "at least a 50 mm (2 in.) width will project outside the shell". Runde 1 baute
    25 mm und fuehrte die Klausel als ungelesen — sie ist Text, sie war nur nie aufgeschlagen."""
    # Das Blech LIEGT AUF der Ringmauerkrone, es steckt nicht darin: Runde 2 mass 53 Liter
    # Beton und Stahl am selben Ort, weil kTankBaseZ die Ringmauerkrone WAR und das Blech
    # darunter gebaut wurde.
    z0 = G.kRingwallRise
    ro = G.kShellOuterMax + G.kBottomPlateProj
    ri = G.kRadius - 0.150
    t = G.kBottomPlateThk
    prof = [(ri, z0), (ro, z0), (ro, z0 + t), (ri, z0 + t)]
    return revolve("tank.bottom_plate", "paint", prof, cfg["seg"])


def top_angle(cfg):
    """Kopfwinkel 50x50x6 [API 5.1.5.9 e]. Waagerechter Schenkel nach AUSSEN — die Klausel schreibt
    das nur fuer gedaemmte Tanks zwingend vor; hier gewaehlt, weil das Dachblech darauf aufliegt."""
    z1 = G.kTankBaseZ + G.kShellHeight
    ro = G.shell_outer(G.kCourses - 1)
    L, t = G.kTopAngleLeg, G.kTopAngleThk
    prof = [(ro, z1 - L), (ro + t, z1 - L), (ro + t, z1),
            (ro + L, z1), (ro + L, z1 + t), (ro, z1 + t)]
    return revolve("tank.top_angle", "paint", prof, cfg["seg"])


def roof(cfg):
    """Getragenes Kegeldach, Neigung 1:16 [API 5.10.4.1], Blech 3/16 in [API 5.10.5 Anm.]."""
    s = G.kRoofSlope
    tv = G.kRoofThk * math.sqrt(1.0 + s * s)
    # UNTERSEITE auf die Oberkante des Kopfwinkels, nicht die Oberseite: sonst taucht das
    # Dachblech um seine eigene Dicke in den Winkel ein (in Runde 2 gemessen).
    z1 = _roof_top_z()
    rrim = _roof_rim()
    apex = z1 + rrim * s
    prof = [(rrim, z1), (0.0, apex), (0.0, apex - tv), (rrim, z1 - tv)]
    return revolve("tank.roof", "paint", prof, cfg["seg"])


def _roof_rim():
    return G.shell_outer(G.kCourses - 1) + G.kTopAngleLeg


def _roof_top_z():
    """Hoehe der Dach-OBERSEITE an der Traufe: Kopfwinkel-Oberkante plus Blechdicke."""
    tv = G.kRoofThk * math.sqrt(1.0 + G.kRoofSlope ** 2)
    return G.kTankBaseZ + G.kShellHeight + G.kTopAngleThk + tv


def _roof_z(cfg, phi, r):
    """Hoehe der GEBAUTEN Dachflaeche im Abstand r und Azimut phi.

    Das Dach ist ein Faecher aus ebenen Dreiecken von der Spitze zu den Randecken. Entlang eines
    Strahls vom Achsenpunkt aus verlaeuft z deshalb linear von der Spitze bis zur Traufe, und die
    Traufe liegt beim POLYGONradius, nicht beim Kreisradius.
    """
    rim = _roof_rim()
    z_rim = _roof_top_z()
    apex = z_rim + rim * G.kRoofSlope
    rr = G.poly_radius(rim, cfg["seg"], phi)
    return apex + (r / rr) * (z_rim - apex)


def _shell_dir(phi):
    return (math.cos(phi), math.sin(phi), 0.0)


def _radial_frame(phi):
    """(Ursprungsachse, u = tangential, v = z) fuer einen radial stehenden Sweep."""
    return (-math.sin(phi), math.cos(phi), 0.0), (0.0, 0.0, 1.0)


def _shell_wall(cfg, phi, z):
    """Radius der WIRKLICH GEBAUTEN Wand an dieser Stelle — Schussdicke und Umfangskorrektur.

    Ein Anbau, der stur beim wahren Kreisradius sitzt, haengt bei grobem n vor der Wand oder steckt
    darin (bei n=12: +84 / -170 mm). Runde 1 hatte das nicht gemessen.
    """
    c = min(int((z - G.kTankBaseZ) / G.kCourseHeight), G.kCourses - 1)
    return G.poly_radius(G.shell_outer(max(c, 0)), cfg["seg"], phi)


def _shell_wall_max(cfg, phi_a, phi_b, z, bead=True):
    """Groesster Wandradius ueber einen Azimutbereich — inklusive der Polygonecken darin.

    Eine Stufe ueberdeckt bei n=96 etwa die halbe Facette; wer nur an EINEM Azimut misst, laesst
    die Wand am Facettenende bis 4.9 mm durch den Trittbelag treten. Runde 2 mass genau das:
    15 Stufen mit zusammen 373 cm3 in der Schale.
    """
    a = TAU / cfg["seg"]
    ks = range(int(math.floor(phi_a / a)), int(math.ceil(phi_b / a)) + 1)
    cand = [phi_a, phi_b] + [k * a for k in ks if phi_a <= k * a <= phi_b]
    r = max(_shell_wall(cfg, p, z) for p in cand)
    # Beide Nahtscharen stehen ueber die Wand. Die waagerechte trifft nur einzelne Hoehen, die
    # SENKRECHTE laeuft ueber die ganze Schusshoehe und kann in jeden Azimutbereich fallen — also
    # wird die Nahthoehe generell mitgerechnet. 3 mm kosten kein Dreieck und kein Pixel.
    r += kBeadH if bead else 0.0
    # Die Innenkante der Stufe ist eine GERADE Sehne ueber [phi_a, phi_b]; ihr tiefster Punkt
    # liegt r*cos(dphi/2) von der Achse. Liegt eine Polygonecke der Schale dazwischen, sticht die
    # Wand durch die Sehne — 0.01 mm reichen dafuer, und genau das blieb in Runde 2 uebrig.
    return r / math.cos(0.5 * (phi_b - phi_a))


def shell_patch(name, mat, cfg, phi, z, radius, thk, m=None):
    """Gewalztes rundes Blech AUF der Schale, kein flacher Deckel davor.

    Runde 2 schob die flache Scheibe mit _shell_wall_max() so weit nach aussen, bis sie nirgends
    mehr schnitt — damit schwebte sie 2.7 bis 13.1 mm vor der Wand. Ein Verstaerkungsblech LIEGT
    an; es ist auf den Schalenradius gewalzt. Hier wird ein Polarnetz in der Tangentialebene auf
    die GEBAUTE Polygonwand abgebildet, aussen um die Blechdicke versetzt.
    """
    m = m or max(12, cfg["seg"] // 6)
    k = 2
    rings = []
    for i in range(k + 1):
        rr = radius * i / k
        ring = []
        for jj in range(m):
            th = TAU * jj / m
            u, v = rr * math.cos(th), rr * math.sin(th)
            a = phi + u / G.kRadius
            w = _shell_wall(cfg, a, z + v)
            ring.append((a, z + v, w))
        rings.append(ring)
    me = Mesh(name, mat)

    def pt(a, zz, w, off):
        return ((w + off) * math.cos(a), (w + off) * math.sin(a), zz)

    def nrm(a):
        return (math.cos(a), math.sin(a), 0.0)

    for face, off, flip in ((0, thk, False), (1, 0.0, True)):
        c = rings[0][0]
        for i in range(k):
            for jj in range(m):
                j2 = (jj + 1) % m
                if i == 0:
                    q = [pt(*c, off), pt(*rings[1][jj], off), pt(*rings[1][j2], off)]
                else:
                    q = [pt(*rings[i][jj], off), pt(*rings[i + 1][jj], off),
                         pt(*rings[i + 1][j2], off), pt(*rings[i][j2], off)]
                me.add(q[::-1] if flip else q, [nrm(phi)] * len(q))
    for jj in range(m):
        j2 = (jj + 1) % m
        a, b = rings[k][jj], rings[k][j2]
        me.add([pt(*a, 0.0), pt(*b, 0.0), pt(*b, thk), pt(*a, thk)],
               [(math.cos(a[0]), math.sin(a[0]), 0.0)] * 4)
    return me


def shell_manhole(cfg, phi):
    """Schalenmannloch DN 600 [API T.5-5a]: Verstaerkungsblech, Hals, Deckel, Schrauben."""
    out = []
    # Wie bei den Stufen: das Blech ist eine flache SEHNE ueber seinen Winkelbereich, die Wand
    # dahinter waechst zu den Polygonecken hin. Bei n=16 liegen weder 300 noch 345 Grad auf einer
    # Ecke — Runde 2 mass dort 1521 cm3 Blech in der Wand.
    _zm = G.kTankBaseZ + G.kShellManholeZ
    _h = G.kNozzleReinfDia * 1.55 / 2.0 / G.kRadius
    ro = _shell_wall_max(cfg, phi - _h, phi + _h, _zm)
    d = np.array(_shell_dir(phi))
    z = G.kTankBaseZ + G.kShellManholeZ
    ctr = np.array([0.0, 0.0, z])
    u, v = _radial_frame(phi)
    u, v = np.array(u), np.array(v)
    nseg = max(8, cfg["seg"] // 4)

    def disc(name, mat, r, off0, off1):
        p0 = ctr + d * (ro + off0)
        p1 = ctr + d * (ro + off1)
        return sweep(name, mat, [(p0, u, v), (p1, u, v)], ngon(nseg, r), round_section=True)

    out.append(shell_patch("tank.manhole.shell.pad", "paint", cfg, phi, _zm,
                           G.kShellManholeReinf / 2.0, 0.010))
    out.append(disc("tank.manhole.shell.neck", "paint", G.kShellManholeDia / 2.0, 0.010, 0.150))
    out.append(disc("tank.manhole.shell.cover", "paint", G.kShellManholeCover / 2.0, 0.150, 0.172))
    if cfg["detail"] >= 2:
        rb = 0.768 / 2.0                       # Lochkreis [API T.5-5a, Db = 768 mm]
        for k in range(G.kShellManholeBolts):
            a = TAU * k / G.kShellManholeBolts
            c = ctr + d * (ro + 0.172) + u * (rb * math.cos(a)) + v * (rb * math.sin(a))
            out.append(sweep("tank.manhole.shell.bolt.%02d" % k, "dark",
                             [(c, u, v), (c + d * 0.018, u, v)], ngon(6, 0.016)))
    return out


def cone_pad(name, mat, cfg, phi, r_mid, dia, thk):
    """Gewalztes Blech AUF der Kegelflaeche: ein Sektor des Drehkoerpers, kein flacher Deckel."""
    half = 0.5 * dia / r_mid
    r0, r1 = r_mid - dia / 2.0, r_mid + dia / 2.0

    def z_at(rr):
        return _roof_z(cfg, 0.0, rr)             # Kegelhoehe; der Azimut faellt heraus

    prof = [(r0, z_at(r0)), (r1, z_at(r1)), (r1, z_at(r1) + thk), (r0, z_at(r0) + thk)]
    return revolve(name, mat, prof, cfg["seg"], phi - half, phi + half)


def roof_manhole(cfg, phi):
    """Dachmannloch DN 500 [API T.5-13a]. Achse steht IMMER senkrecht (Bild 5-16)."""
    out = []
    r = G.kRoofManholeR
    c = np.array([r * math.cos(phi), r * math.sin(phi), _roof_z(cfg, phi, r)])
    u, v = np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0])
    nseg = max(8, cfg["seg"] // 4)

    def disc(name, mat, rad, z0, z1_):
        return sweep(name, mat, [(c + np.array([0, 0, z0]), u, v),
                                 (c + np.array([0, 0, z1_]), u, v)], ngon(nseg, rad),
                     round_section=True)

    # Das Verstaerkungsblech LIEGT AUF dem Kegel — als flache Scheibe kann es das nicht: ueber
    # 1.05 m Sehne bei r = 4.02 m sind das 34 mm Stich, und die Selbstpruefung mass die
    # Durchdringung (1071 cm3). Es wird deshalb als KEGELSEKTOR gebaut, also als gewalztes Blech.
    out.append(cone_pad("tank.manhole.roof.pad", "paint", cfg, phi, r,
                        G.kRoofManholeReinf, 0.010))
    z_pad = _roof_z(cfg, phi, r - G.kRoofManholeReinf / 2.0) + 0.010
    out.append(sweep("tank.manhole.roof.neck", "paint",
                     [(np.array([c[0], c[1], z_pad]), u, v),
                      (np.array([c[0], c[1], z_pad + 0.142]), u, v)],
                     ngon(nseg, G.kRoofManholeDia / 2.0), round_section=True))
    out.append(sweep("tank.manhole.roof.cover", "paint",
                     [(np.array([c[0], c[1], z_pad + 0.142]), u, v),
                      (np.array([c[0], c[1], z_pad + 0.160]), u, v)],
                     ngon(nseg, G.kRoofManholeCover / 2.0), round_section=True))
    if cfg["detail"] >= 2:
        rb = 0.597 / 2.0                       # [API T.5-13a, DB = 597 mm]
        for k in range(G.kRoofManholeBolts):
            a = TAU * k / G.kRoofManholeBolts
            p = np.array([c[0] + rb * math.cos(a), c[1] + rb * math.sin(a), z_pad + 0.160])
            out.append(sweep("tank.manhole.roof.bolt.%02d" % k, "dark",
                             [(p, u, v), (p + np.array([0, 0, 0.016]), u, v)], ngon(6, 0.014)))
    return out


def vent(cfg, phi):
    """Freiatmer. Groesse [SET] — API Std 2000 wurde nicht gerechnet (DEFECTS.md #5)."""
    r = G.kVentR
    c = np.array([r * math.cos(phi), r * math.sin(phi), _roof_z(cfg, phi, r)])
    u, v = np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0])
    nseg = max(8, cfg["seg"] // 4)
    # Groesserer Radius heisst auf einem Kegel TIEFER — der Fuss muss auf den HOECHSTEN Punkt
    # seines Fusskreises, sonst steckt die Rohrwand auf der Innenseite im Dachblech.
    z_foot = _roof_z(cfg, phi, r - G.kVentDia / 2.0)
    c = np.array([c[0], c[1], z_foot])
    pipe = sweep("tank.vent.pipe", "paint",
                 [(c, u, v), (c + np.array([0, 0, G.kVentHeight]), u, v)],
                 ngon(nseg, G.kVentDia / 2.0), round_section=True)
    top = c + np.array([0.0, 0.0, G.kVentHeight])
    cap = revolve("tank.vent.cap", "dark",
                  [(G.kVentCapDia / 2.0, top[2] + 0.030), (0.0, top[2] + 0.110),
                   (0.0, top[2] + 0.098), (G.kVentCapDia / 2.0, top[2] + 0.018)],
                  nseg)
    cap.v = [(x + top[0], y + top[1], z) for x, y, z in cap.v]
    return [pipe, cap]


def nozzle(cfg, phi):
    """Bodenstutzen NPS 6 [API T.5-6a]: Achse 306 mm ueber dem Boden, Flanschspiegel 200 mm frei."""
    out = []
    z = G.kTankBaseZ + G.kNozzleZ
    _h = G.kNozzleReinfDia / 2.0 / G.kRadius
    ro = _shell_wall_max(cfg, phi - _h, phi + _h, z)
    d = np.array(_shell_dir(phi))
    ctr = np.array([0.0, 0.0, z])
    u, v = _radial_frame(phi)
    u, v = np.array(u), np.array(v)
    nseg = max(8, cfg["seg"] // 4)

    def disc(name, mat, rad, o0, o1):
        return sweep(name, mat, [(ctr + d * (ro + o0), u, v), (ctr + d * (ro + o1), u, v)],
                     ngon(nseg, rad), round_section=True)

    out.append(shell_patch("tank.nozzle.outlet.pad", "paint", cfg, phi, z,
                           G.kNozzleReinfDia / 2.0, 0.010))
    out.append(disc("tank.nozzle.outlet.neck", "paint", G.kNozzleOD / 2.0, 0.010, G.kNozzleProj))
    out.append(disc("tank.nozzle.outlet.flange", "paint", G.kNozzleFlangeDia / 2.0,
                    G.kNozzleProj, G.kNozzleProj + G.kNozzleFlangeThk))
    return out


def ringwall(cfg):
    """Betonringmauer [API B.4.2.2]: 300 mm dick, Mittellinie = Nenndurchmesser des Tanks."""
    ri = G.kRingwallCLDia / 2.0 - G.kRingwallWidth / 2.0
    ro = G.kRingwallCLDia / 2.0 + G.kRingwallWidth / 2.0
    # [API B.4.2.2] "the bottom of the ringwall ... shall be located 0.6 m (2 ft) below the lowest
    # adjacent finish grade". Runde 1 setzte -0.15 m; auf geneigtem Gelaende tritt der Ring dann
    # bergab aus dem Boden. Der vergrabene Teil kostet kein einziges Dreieck mehr.
    zb = G.kRingwallBottom
    prof = [(ri, zb), (ro, zb), (ro, G.kRingwallRise), (ri, G.kRingwallRise)]
    return revolve("foundation.ringwall", "concrete", prof, cfg["seg"])


# ---------------------------------------------------------------- Treppe und Podest

def _stair_phi(i):
    return G.kStairPhi0 + i * G.kStairDPhi


def _pt(r, phi, z):
    return np.array([r * math.cos(phi), r * math.sin(phi), z])


def _ringwall_ro():
    return G.kRingwallCLDia / 2.0 + G.kRingwallWidth / 2.0


def _stair_z(i):
    """Oberkante der Stufe i ueber Gelaende. i = 0 wird auf die erste Stufe geklemmt: Wange und
    Gelaender beginnen dort, nicht eine Stufenhoehe tiefer im Fundament."""
    return G.kStairBaseZ + max(i, 1) * G.kStairRise


def _stringer_depth(z):
    """[API T.5-18 Pkt.10] "the ends of the stringers shall be clear of the ground". Die Wange
    laeuft unten deshalb flach aus statt unter das Gelaende zu tauchen. Freimass [SET] 50 mm."""
    return max(0.020, min(G.kStringerH, z - 0.050))


def stair(cfg):
    """Umlaufende Treppe. Steigung/Auftritt/Winkel/Pfostenabstand stehen alle in
    fuel_tank_geometry aus [API T.5-18] und [OSHA 1910.25(c)]."""
    out = []
    ri, ro = G.kStairInnerR, G.kStairOuterR
    dphi = G.kStairDPhi
    nose = G.kStairNosing / G.kStairMidR
    n = G.kStairSteps
    # Die OBERSTE Stufe ist das Podest — sie wird nicht noch einmal als Stufe gebaut. Runde 1 tat
    # das und schob den Trittbelag durch den senkrechten Schenkel des Kopfwinkels.
    n_tread = n - 1

    if cfg["detail"] >= 1:
        for i in range(n_tread):
            z = _stair_z(i + 1)
            # Unterhalb der Ringmauerkrone muss die Stufe AN der Ringmauer vorbei, nicht durch
            # sie hindurch (Runde 2 gemessen: Stufe 0 mit 1207 cm3 im Beton).
            pa, pb = _stair_phi(i) - nose, _stair_phi(i + 1)
            rin = (_shell_wall_max(cfg, pa, pb, z) if z - G.kStairTreadThk >= G.kRingwallRise
                   else _ringwall_ro() + 0.010)
            lo = [_pt(rin, pa, z - G.kStairTreadThk), _pt(ro, pa, z - G.kStairTreadThk),
                  _pt(ro, pb, z - G.kStairTreadThk), _pt(rin, pb, z - G.kStairTreadThk)]
            hi = [_pt(rin, pa, z), _pt(ro, pa, z), _pt(ro, pb, z), _pt(rin, pb, z)]
            out.append(box("access.stair.tread.%02d" % i, "steel", lo, hi))
    else:
        # L3: die Stufen verschmelzen zum geschlossenen Band. AUSSENKANTE und Oberkante bleiben
        # exakt die der Einzelstufen, also bleibt die Silhouette in Auf- und Grundriss stehen.
        zs = [_stair_z(i) for i in range(n + 1)]
        # Wie die Einzelstufen muss auch das Band an der Ringmauer vorbei, nicht hindurch: der
        # Kritiker mass hier elf Liter Treppe im Beton, die Runde 2 mit samples=10 als 0.00 las.
        ri_band = max(ri, _ringwall_ro() + 0.010)
        rin = [ri_band if zs[i] - G.kStringerH < G.kRingwallRise else ri for i in range(n + 1)]
        path = [(_pt(rin[i], _stair_phi(i), zs[i]),
                 np.array([math.cos(_stair_phi(i)), math.sin(_stair_phi(i)), 0.0]),
                 np.array([0.0, 0.0, 1.0])) for i in range(n + 1)]
        # Die AUSSENkante bleibt die der Einzelstufen — sie traegt die Silhouette. Runde 3 hatte
        # hier eine feste Breite ro - ri_band gerechnet und damit ein 553 statt 710 mm breites
        # Band gebaut; im Grundriss war das der groesste Einzelposten der XOR-Flaeche.
        secs = [[(0.0, 0.0), (ro - rin[i], 0.0), (ro - rin[i], -_stringer_depth(zs[i])),
                 (0.0, -_stringer_depth(zs[i]))] for i in range(n + 1)]
        out.append(sweep("access.stair.band", "steel", path, secs))

    if cfg["detail"] >= 1:
        # Aussenwange: Rechteckquerschnitt unter der Stufenlinie entlanggezogen.
        zs = [_stair_z(i) for i in range(n + 1)]
        pts = [_pt(ro + G.kStringerThk / 2.0, _stair_phi(i), zs[i]) for i in range(n + 1)]
        t = G.kStringerThk / 2.0
        secs = [[(-t, 0.0), (t, 0.0), (t, -_stringer_depth(z)), (-t, -_stringer_depth(z))]
                for z in zs]
        out.append(sweep("access.stair.stringer", "steel", normal_frames(pts), secs))

    # Handlauf: nur aussen. [API T.5-18 Pkt.9] fordert den inneren erst ab 200 mm Abstand zur
    # Schale; die innere Wange liegt AUF der Schale, also entfaellt er.
    rr = ro + G.kStringerThk
    if not cfg["rails"]:
        return out

    def rail_h(i, h_low, h_high):
        """[API T.5-18 Pkt.6] "shall join the platform handrail WITHOUT OFFSET". Treppengelaender
        860 mm ueber der Stufenvorderkante, Podestgelaender 1070 mm ueber dem Podest — am
        Uebergang koennen sie nicht gleich hoch sein, also rampt der Treppenlauf ueber die letzten
        kStairRailRampSteps Stufen hoch. Runde 1 liess dort einen 210-mm-Absatz stehen."""
        k = G.kStairRailRampSteps
        f = min(max((i - (n - k)) / float(k), 0.0), 1.0)
        return h_low + (h_high - h_low) * f

    # [OSHA 1910.29(b)(1)] misst die OBERKANTE des Holms, also liegt die Rohrachse einen
    # Rohrradius tiefer. Runde 2 legte die Achse auf das Sollmass und baute 21 mm zu hoch.
    _r = G.kRailTubeDia / 2.0
    for tag, h, ph in (("top", G.kStairRailH - _r, G.kPlatformRailH - _r),
                       ("mid", G.kStairRailH / 2.0 - _r, G.kPlatformRailH / 2.0 - _r)):
        pts = [_pt(rr, _stair_phi(i), _stair_z(i) + rail_h(i, h, ph))
               for i in range(n + 1)]
        out.append(sweep("access.stair.rail.%s" % tag, "steel", normal_frames(pts),
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))

    # Der oberste Treppenpfosten und der erste Podestpfosten stehen am SELBEN Ort — es ist
    # derselbe Pfosten. Runde 2 baute beide und liess sie ineinanderstecken (579 cm3 bei L2).
    # Der Podestpfosten bleibt, weil er das Podestgelaender traegt.
    step = n / float(G.kStairPosts - 1)
    for k in range(G.kStairPosts - 1):
        i = int(round(k * step))
        p = _stair_phi(i)
        z = _stair_z(i)
        u = np.array([math.cos(p), math.sin(p), 0.0])
        v = np.array([-math.sin(p), math.cos(p), 0.0])
        out.append(sweep("access.stair.post.%02d" % k, "steel",
                         [(_pt(rr, p, z - _stringer_depth(z)), u, v),
                          (_pt(rr, p, z + rail_h(i, G.kStairRailH, G.kPlatformRailH)), u, v)],
                         ngon(cfg["tube"], G.kPostDia / 2.0), round_section=True))
    return out


def _platform_span(cfg):
    """(Innenradius, Aussenradius, Anfangs-, Endazimut) des Podests — die EINE Quelle dafuer.

    Das Dachrandgelaender braucht denselben Bogen (es laesst genau ihn aus), und zwei Rechnungen
    desselben Bogens waeren zwei Gelegenheiten, ihn verschieden zu bekommen.
    """
    # Dieselbe Sehnenregel wie bei den Stufen: der Podestboden ist ein Polygonzug, seine
    # Innenkante muss die Ecken des Kopfwinkels ausserhalb lassen.
    ri = G.ring_radius(_roof_rim(), cfg["seg"]) / math.cos(math.pi / cfg["seg"])
    # [API T.5-17 Pkt.2] "minimum width of the walkway shall be 610 mm, AFTER MAKING ADJUSTMENTS
    # AT ALL PROJECTIONS". Runde 2 baute 610 mm Blech und kam nach Fussleiste und Pfosten auf
    # 586 mm licht. Die Blechbreite folgt deshalb aus der lichten Breite plus dem, was hineinragt.
    #
    # HIER STAND BIS RUNDE 3 EIN assert, DER NICHTS PRUEFEN KONNTE: er rechnete
    # width - intrude >= kPlatformClear, nachdem width zwei Zeilen darueber als
    # kPlatformClear + intrude gesetzt worden war — eine Substitution, kein Mass. Am Netz waren es
    # 618.0 mm statt der gerechneten 610.0, weil intrude die Fussleiste mitzaehlt, die per
    # arc_frames(ro, ...) nach AUSSEN sitzt und gar nicht in den Laufsteg ragt. Die Norm war
    # gehalten, aber aus Versehen. Gemessen wird jetzt am gebauten Netz (check_norms).
    intrude = G.kPlatformToeThk + G.kPostDia / 2.0
    ro = ri + G.kPlatformClear + intrude
    p0 = _stair_phi(G.kStairSteps)
    return ri, ro, p0, p0 + G.kPlatformArc / ((ri + ro) / 2.0)


def platform(cfg):
    """Podest an der Dachkante [API 5.8.10 c] mit Gelaender nach [API T.5-17]."""
    out = []
    ri, ro, p0, p1 = _platform_span(cfg)
    width = ro - ri
    z = G.kPlatformZ
    nsec = max(3, int(round(cfg["seg"] * (p1 - p0) / TAU)) + 1)
    phis = [p0 + (p1 - p0) * i / nsec for i in range(nsec + 1)]

    out.append(sweep("access.platform.floor", "steel", arc_frames(ri, phis, lambda p: z),
                     [(0.0, 0.0), (width, 0.0), (width, -0.030), (0.0, -0.030)]))
    out.append(sweep("access.platform.toeboard", "steel", arc_frames(ro, phis, lambda p: z),
                     [(0.0, 0.0), (G.kPlatformToeThk, 0.0),
                      (G.kPlatformToeThk, G.kPlatformToeH), (0.0, G.kPlatformToeH)]))

    if not cfg["rails"]:
        return out
    for tag, h in (("top", G.kPlatformRailH - G.kRailTubeDia / 2.0),
                   ("mid", G.kPlatformRailH / 2.0 - G.kRailTubeDia / 2.0)):
        out.append(sweep("access.platform.rail.%s" % tag, "steel",
                         arc_frames(ro, phis, lambda p, h=h: z + h),
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))

    # [API T.5-17 Pkt.8] misst den Abstand ZWISCHEN DEN PFOSTEN, und die stehen am Gelaender, also
    # am AUSSENradius — nicht auf der Laufsteg-Mittellinie, aus der kPlatformArc kommt. Runde 3
    # teilte 2400 mm Mittellinienbogen und baute damit zwei Pfosten in 2490 mm Abstand; gemessen
    # hat das erst check_norms. Der Bogen am Holm ist ro/r_mittel mal laenger.
    npost = max(2, int(math.ceil((p1 - p0) * ro / G.kPlatformPostMax)) + 1)
    for k in range(npost):
        p = p0 + (p1 - p0) * k / (npost - 1)
        u = np.array([math.cos(p), math.sin(p), 0.0])
        v = np.array([-math.sin(p), math.cos(p), 0.0])
        out.append(sweep("access.platform.post.%02d" % k, "steel",
                         [(_pt(ro, p, z), u, v),
                          (_pt(ro, p, z + G.kPlatformRailH), u, v)],
                         ngon(cfg["tube"], G.kPostDia / 2.0), round_section=True))

    # [API T.5-17 Pkt.10] "Handrails shall be on both sides of the platform but shall be
    # discontinued where necessary for access." Die INNENseite ist die Tankwand — Pkt.11 derselben
    # Tabelle behandelt sie als schliessende Flaeche ("any space wider than 150 mm BETWEEN THE TANK
    # AND THE PLATFORM should be floored"). Offen blieb in Runde 1 die STIRNseite: das ferne Ende
    # war ein 0.61 m breiter Abgrund ohne Gelaender. Das nahe Ende ist der Treppenzugang und bleibt
    # nach demselben Satz offen.
    for tag, h in (("top", G.kPlatformRailH - G.kRailTubeDia / 2.0),
                   ("mid", G.kPlatformRailH / 2.0 - G.kRailTubeDia / 2.0)):
        # Der Pfad laeuft RADIAL, also muss der Querschnitt in der tangential/z-Ebene liegen.
        # Mit u = radial faellt die Querschnittsebene mit der Pfadrichtung zusammen und der
        # Koerper hat Volumen null — derselbe Fehler wie bei den Pfosten in Runde 1.
        u = np.array([-math.sin(p1), math.cos(p1), 0.0])
        out.append(sweep("access.platform.endrail.%s" % tag, "steel",
                         [(_pt(ri, p1, z + h), u, np.array([0.0, 0.0, 1.0])),
                          (_pt(ro, p1, z + h), u, np.array([0.0, 0.0, 1.0]))],
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))
    return out


def roof_rail(cfg):
    """Umlaufendes Dachrandgelaender — Pflicht ueber [API 5.8.10 a] -> [OSHA 1910.28(b)(1)(i)],
    bemasst nach [API T.5-17] Pkt. 4 / 7 / 8. Die Herleitung steht in fuel_tank_geometry.

    ES STEHT AUF ALLEN VIER STUFEN, auch auf L3, wo Treppen- und Podestgelaender fallen. Der
    Grund ist gemessen, nicht gefuehlt: gegen den Himmel ist dieser Ring die kontrastreichste
    Linie des Koerpers, und sein Fehlen kostet 0.968 % Silhouette auf 62 m gegen die 0.389 %, mit
    denen die anderen Gelaender auf L3 fallen durften. Was auf L3 faellt, ist die Abtastung des
    Rohrquerschnitts (cfg["tube"] = 4), nicht der Ring.

    Der Pfad folgt der GEBAUTEN Polygontraufe, nicht dem Kreis: an den Polygonecken liegt die
    Traufe aussen, in den Kantenmitten innen. Ein Ring auf dem Kreisradius haette bei n = 20 an
    den Kantenmitten 91 mm ueber der Dachkante gestanden.
    """
    out = []
    _, _, p0, p1 = _platform_span(cfg)
    a = TAU / cfg["seg"]
    # Alle Polygonecken im Ringbogen plus die beiden Enden — dazwischen ist die Traufe gerade,
    # also gibt eine Sehne zwischen aufeinanderfolgenden Stuetzpunkten sie exakt wieder.
    corners = [k * a for k in range(int(math.floor(p1 / a)) + 1,
                                    int(math.ceil((p0 + TAU) / a)))]
    phis = [p1] + [p for p in corners if p1 < p < p0 + TAU] + [p0 + TAU]

    def seat(phi):
        """Pfostenachse und Dachoberflaeche darunter, beides am GEBAUTEN Netz gerechnet."""
        r = G.poly_radius(_roof_rim(), cfg["seg"], phi) - G.kRoofRailInset
        return r, _roof_z(cfg, phi, r)

    for tag, h in (("top", G.kRoofRailH - G.kRailTubeDia / 2.0),
                   ("mid", G.kRoofRailH / 2.0 - G.kRailTubeDia / 2.0)):
        pts = []
        for p in phis:
            r, z = seat(p)
            pts.append(_pt(r, p, z + h))
        out.append(sweep("access.roofrail.rail.%s" % tag, "steel", normal_frames(pts),
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))

    n = G.kRoofRailPosts
    for k in range(n):
        p = p1 + (p0 + TAU - p1) * k / (n - 1)
        r, z = seat(p)
        u = np.array([math.cos(p), math.sin(p), 0.0])
        v = np.array([-math.sin(p), math.cos(p), 0.0])
        out.append(sweep("access.roofrail.post.%02d" % k, "steel",
                         [(_pt(r, p, z), u, v), (_pt(r, p, z + G.kRoofRailH), u, v)],
                         ngon(cfg["tube"], G.kPostDia / 2.0), round_section=True))
    return out


# ================================================================ Blender-Anbindung

kMat = {
    # [SET] Alle vier Materialien. Ein Anstrich hat keine Norm; die Werte sind PBR-plausible
    # Messwerte fuer die genannten Oberflaechen (DEFECTS.md #7).
    "paint":    dict(rgb=(0.640, 0.655, 0.660), rough=0.55, metal=0.00),
    "steel":    dict(rgb=(0.520, 0.530, 0.545), rough=0.42, metal=0.85),
    "concrete": dict(rgb=(0.420, 0.415, 0.400), rough=0.92, metal=0.00),
    "dark":     dict(rgb=(0.055, 0.055, 0.060), rough=0.70, metal=0.20),
}
kMatCollapse = {"steel": "paint", "dark": "paint"}      # L2/L3

# Was auf einer groberen Stufe ERSETZT statt weggelassen wird (s. switch_table).
kSubstitute = (("access.stair.tread.", "access.stair.band"),
               ("access.stair.stringer", "access.stair.band"))


def make_material(key):
    m = bpy.data.materials.new("%s_%s" % (kPrefix, key))
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    s = kMat[key]
    b.inputs["Base Color"].default_value = (*s["rgb"], 1.0)
    b.inputs["Roughness"].default_value = s["rough"]
    b.inputs["Metallic"].default_value = s["metal"]
    m.use_backface_culling = True
    return m


def to_blender(me, mats, parent):
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
    return o


def group(name, parent=None):
    e = bpy.data.objects.new(name, None)
    e.empty_display_size = 0.5
    bpy.context.collection.objects.link(e)
    if parent:
        e.parent = parent
    return e


def build_bodies(cfg):
    """Die EINE Quelle: eine geordnete Liste (Gruppe, Netz). Reihenfolge ist fest -> Determinismus."""
    # [SET] Umfangslagen der Anbauten. Sie sind so gewaehlt, dass Mannloch, Stutzen und Treppenfuss
    # einander nicht durchdringen; keine Quelle schreibt sie vor (DEFECTS.md #8).
    phi_mh, phi_noz = G.kManholePhi, G.kNozzlePhi
    phi_rmh, phi_vent = G.kRoofManholePhi, G.kVentPhi

    out = [("foundation", ringwall(cfg)),
           ("tank", shell(cfg)),
           ("tank", bottom_plate(cfg)),
           ("tank", top_angle(cfg)),
           ("tank", roof(cfg))]
    for m in vertical_seams(cfg):
        out.append(("tank", m))
    for m in shell_manhole(cfg, phi_mh):
        out.append(("tank", m))
    for m in roof_manhole(cfg, phi_rmh):
        out.append(("tank", m))
    for m in vent(cfg, phi_vent):
        out.append(("tank", m))
    for m in nozzle(cfg, phi_noz):
        out.append(("tank", m))
    for m in stair(cfg):
        out.append(("access", m))
    for m in platform(cfg):
        out.append(("access", m))
    for m in roof_rail(cfg):
        out.append(("access", m))
    return [(g, orient(m)) for g, m in out]


def build_lod(cfg, out_dir, keep_blend=False):
    clock = {}
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bodies = build_bodies(cfg)
    clock['bau'] = time.time() - t0

    print("--- %s  seg=%d tube=%d detail=%d" % (cfg["name"], cfg["seg"], cfg["tube"], cfg["detail"]))
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

    # WIRKLICH verschmelzen und WIRKLICH vergleichen (doc/assets.md §3.1). Runde 1 meldete nur
    # zwei Zahlen nebeneinander und verglich nie.
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

    vol_of = {m.name: abs(s["volume"]) * 1e6 for (_, m), (_, s) in zip(bodies, per_body)}
    t1 = time.time()
    all_ov = check_overlap([(m.name, m) for _, m in bodies], vol_of)
    clock['durchdringung'] = time.time() - t1
    joints, bounded, ov, tiny = [], [], [], []
    for a, b, cm3, nn, rel in all_ov:
        vmin = min(vol_of.get(a, 0.0), vol_of.get(b, 0.0))
        row = (a, b, cm3, nn, rel, cm3 / max(vmin, 1e-9))
        if is_joint(a, b, cm3, vmin):
            (bounded if nn == 0 else joints).append(row)
        elif cm3 >= kOverlapMinCm3:
            ov.append(row)
        else:
            tiny.append(row)
    ov_sum = sum(x[2] for x in ov)
    if ov:
        fails.append("Durchdringung: %d Koerperpaare, %.2f cm3" % (len(ov), ov_sum))
        for a, b, cm3, nn, rel, q in ov[:8]:
            print("  DURCHDRINGUNG %-28s x %-28s %9.2f cm3  n=%d  dV/V %.1f %%"
                  % (a, b, cm3, nn, 100.0 * rel))
    print("  MERGE  V_teile %.6f  V_verschmolzen %.6f  dV %.2e  dBox %.2e  %s"
          % (vol_sum, mv, d_vol, d_box, "OK" if merge_ok else "ABWEICHUNG"))
    if tiny:
        print("  DURCHDRINGUNG %d detektor-positive Paare unter der Urteilsschwelle %.2f cm3 "
              "(zusammen %.4f cm3, groesstes %.4f cm3) — gemeldet, nicht geschluckt"
              % (len(tiny), kOverlapMinCm3, sum(x[2] for x in tiny),
                 max(x[2] for x in tiny)))
    jq = max([x[5] for x in joints], default=0.0)
    bq = max([x[5] for x in bounded], default=0.0)
    print("  DURCHDRINGUNG %d ungewollt (%.2f cm3), %d Schweissnaehte gemessen (%.1f cm3, "
          "groesste Quote %.1f %% von %.0f %% erlaubt), %d durch den Schnittquader BEWIESEN "
          "(obere Schranke, groesste Quote %.1f %%)  %s"
          % (len(ov), ov_sum, len(joints), sum(x[2] for x in joints), 100.0 * jq,
             100.0 * kJointFrac, len(bounded), 100.0 * bq,
             "OK" if not ov else "DURCHGEFALLEN"))

    t1 = time.time()
    norm_rows, norm_fails = check_norms(bodies, cfg["name"])
    clock['norm'] = time.time() - t1
    judged = cfg["name"] == kLod[0]["name"]
    if judged:
        fails += norm_fails
    n_checked = sum(1 for r in norm_rows if r["passed"] is not None)
    print("  NORM %d Masse am gebauten Netz gegen API 650 / OSHA gemessen, %d ohne Bezug, "
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

    made = {}
    for key in sorted(kMat):
        eff = kMatCollapse.get(key, key) if cfg["single_mat"] else key
        if eff not in made:
            made[eff] = make_material(eff)
    mats = {k: made[kMatCollapse.get(k, k) if cfg["single_mat"] else k] for k in kMat}

    root = group("%s_%s" % (kPrefix, cfg["name"]))
    groups = {g: group(g, root) for g in ("foundation", "tank", "access")}
    tris = 0
    for g, me in bodies:
        to_blender(me, mats, groups[g])
        tris += sum(len(f) - 2 for f in me.f)

    t1 = time.time()
    path = os.path.join(out_dir, "%s_%s.glb" % (kAsset, cfg["name"]))
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False,
                              export_yup=True, export_apply=False, export_normals=True,
                              export_texcoords=False, export_materials='EXPORT')

    clock['export'] = time.time() - t1
    print("  ZEIT  " + "  ".join("%s %.1f s" % (k, v) for k, v in clock.items()))
    used = sorted(set(mats[k].name for k in kMat))
    print("  ASSET %-3s %-34s %8d B %7d Tri %d Mat  x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]"
          % (cfg["name"], os.path.basename(path), os.path.getsize(path), tris, len(used),
             lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    if keep_blend:
        bpy.ops.wm.save_as_mainfile(
            filepath=os.path.join(out_dir, "%s_%s.blend" % (kAsset, cfg["name"])))
    return dict(lod=cfg["name"], file=os.path.basename(path), segments=cfg["seg"],
                triangles=tris, bytes=os.path.getsize(path), bodies=len(bodies),
                materials=used, checks_failed=fails, norms=norm_rows,
                genus=genus, volume_sum_m3=round(vol_sum, 6),
                merge=dict(v_parts=round(vol_sum, 6), v_merged=round(mv, 6),
                           d_volume=d_vol, d_bbox=d_box, passed=bool(merge_ok)),
                overlaps=[dict(a=a, b=b, cm3=round(c, 3), grid=n,
                               converged_rel=round(r, 4)) for a, b, c, n, r, _ in ov],
                overlaps_below_verdict=dict(
                    threshold_cm3=kOverlapMinCm3, pairs=len(tiny),
                    cm3=round(sum(x[2] for x in tiny), 4),
                    largest_cm3=round(max([x[2] for x in tiny], default=0.0), 4),
                    note=('Der Detektor ist exakt und findet auch Beruehrungen weit unter der '
                          'Messaufloesung des Schaetzers. Runde 3 warf sie weg (auf L3 waren es '
                          '57 Paare) und der Bericht las sich wie ein sauberer Befund. Sie stehen '
                          'jetzt hier; die Schwelle selbst ist weiterhin [SET], s. DEFECTS.md.'),
                    pairs_listed=[dict(a=a, b=b, cm3=round(c, 5)) for a, b, c, _, _, _ in tiny]),
                weld_joints=dict(pairs=len(joints), cm3=round(sum(x[2] for x in joints), 2),
                                 cap_fraction=kJointFrac, worst_fraction=round(jq, 4),
                                 bounded_pairs=len(bounded),
                                 bounded_cm3_upper=round(sum(x[2] for x in bounded), 2),
                                 bounded_worst_fraction_upper=round(bq, 4),
                                 bounded_note=(
                                     'Fuer diese Paare ist die Kappe BEWIESEN statt geschaetzt: '
                                     'das gemeinsame Volumen kann den Schnittquader nicht '
                                     'ueberschreiten, und schon der liegt unter der Kappe. Die '
                                     'genannten cm3 sind deshalb obere Schranken, keine Messwerte '
                                     '— sie werden NICHT mit den gemessenen verrechnet.'),
                                 note=('Absichtlich geteiltes Volumen an Schweisspunkten. '
                                       'Die Erlaubnis gilt je Paar bis zur Kappe; alles '
                                       'ausserhalb der Liste kJoint ist ein Defekt.')),
                bbox={k: [round(float(lo[i]), 4), round(float(hi[i]), 4)]
                      for i, k in enumerate("xyz")},
                size_m={k: round(float(hi[i] - lo[i]), 4) for i, k in enumerate("xyz")},
                _bodies=bodies)


# Azimutzahl des Tors. Teilerfremd zu 96 / 48 / 24 / 20 (361 = 19^2), s. _views.
kSilAzimuths = 361
# Der Schaetzer sucht ein SUPREMUM ueber den Azimut, und ein endliches Raster findet es nie ganz:
# XOR(Azimut) ist nicht glatt. Gemessen ueber ein 90-Grad-Fenster um den schlechtesten Azimut von
# L2->L3 bei 0.05 Grad (1801 Proben): sup 1.5938 %, benachbarte Proben unterscheiden sich im Mittel
# um 0.023 pp und im schlimmsten Fall um 0.315 pp. Ein 1-Grad-Raster findet davon 1.5681 %, also
# 0.026 pp zu wenig. Dieser FEHLBETRAG wird dem gemessenen Wert vor dem Urteil zugeschlagen,
# statt so zu tun, als sei das Raster der Grenzwert.
kSilSupShortfallPp = 0.03
# Grob zuerst: jedes Paar wird bei einem Viertel der Aufloesung ueberflogen und nur dann voll
# gemessen, wenn der grobe Wert die halbe Grenze reisst. Gemessen an diesem Koerper liegt der
# grobe Wert IMMER ueber dem feinen (Verhaeltnisse 1.71 / 1.78 / 1.95 / 3.02), er ist also eine
# obere Schranke; die halbe Grenze laesst darueber hinaus einen Faktor 2 Luft.
kSilCoarse = 4
# ... aber NUR bei teuren Paaren. Eine Rasterung hat einen festen Sockel (Projektion und
# Dreiecksindizierung, ~3 ms) und einen aufloesungsabhaengigen Teil; gemessen kostet sie 3.0 ms bei
# 81 px, 10.1 ms bei 325 px und 46.2 ms bei 1280 px. Unter etwa 500 px ist ein Ueberflug bei einem
# Viertel der Aufloesung also nicht viermal billiger, sondern nur dreimal — und wenn das Paar
# danach doch fein gemessen wird, war er reine Zugabe. Ueberflogen wird deshalb erst, wo das
# Verhaeltnis gross genug ist, dass sich der Fehlschlag lohnt.
kSilScreenAbove = 500


def check_silhouette(stats, ranges, limit=2.0):
    """Silhouetten-Tor OHNE Renderer.

    Gemessen wird bei der Aufloesung, die die Umschaltweite hergibt (res = Koerpergroesse /
    (Weite * Pixelwinkel), gedeckelt auf die 1280 px des Zielbildes) — nicht feiner, als die Stufe
    je benutzt wird. Das kumulierte Paar L0->L3 zaehlt im Urteil MIT.

    WAS RUNDE 3 HIER FALSCH MACHTE, in einem Satz: sie hielt ein Alias fuer eine Konvergenz
    (s. _views) und bezahlte fuer die zwei nutzlosen Sprossen 90 und 180 die Haelfte der Bauzeit.
    Statt einer Leiter steht hier eine feste, teilerfremde Azimutzahl und ein benannter, gemessener
    Zuschlag fuer das, was ein endliches Raster am Supremum vorbeilaeuft.
    """
    lo = np.array([stats[0]["bbox"][k][0] for k in "xyz"])
    hi = np.array([stats[0]["bbox"][k][1] for k in "xyz"])
    ctr = (lo + hi) / 2.0
    size = float((hi - lo).max()) * 1.06
    names = [s["lod"] for s in stats]
    flat = [flatten([m for _, m in s["_bodies"]]) for s in stats]
    views = _views(kSilAzimuths)
    pairs = list(zip(range(len(names) - 1), range(1, len(names)))) + [(0, len(names) - 1)]
    cache, rows, worst, worst_row = {}, [], 0.0, None

    def mask(lv, ang, u, v, res):
        key = (lv, round(ang, 9), res)
        if key not in cache:
            cache[key] = raster(flat[lv], u, v, res, ctr, size)
        return cache[key]

    def sweep(a, b, res):
        w, wv = 0.0, None
        for tag, ang, u, v in views:
            ma, mb = mask(a, ang, u, v, res), mask(b, ang, u, v, res)
            pct = 100.0 * int(np.logical_xor(ma, mb).sum()) / max(int(ma.sum()), 1)
            if pct > w:
                w, wv = pct, tag
        return w, wv

    for a, b in pairs:
        rng = ranges[a] if b == a + 1 else ranges[len(names) - 2]
        res = int(max(48, min(1280, round(size / max(rng * G.kPixelAngle, 1e-9)))))
        res_c = max(48, res // kSilCoarse)
        screen = res > kSilScreenAbove and res_c < res
        w, wv = sweep(a, b, res_c if screen else res)
        fine = not screen or w + kSilSupShortfallPp >= 0.5 * limit
        if fine and screen:
            w, wv = sweep(a, b, res)
        judged = w + kSilSupShortfallPp
        rows.append(dict(pair="%s->%s" % (names[a], names[b]), res=res if fine else res_c,
                         res_full=res, range_m=round(rng), azimuths=kSilAzimuths,
                         worst_pct=round(w, 4), judged_pct=round(judged, 4), worst_view=wv,
                         escalated=bool(fine),
                         note=("volle Aufloesung" if fine else
                               "grob gemessen und angenommen: obere Schranke unter der halben "
                               "Grenze, die feine Messung kann nur kleiner sein")))
        if judged > worst:
            worst, worst_row = judged, (names[a], names[b], wv, res if fine else res_c, w)
        print("  SIL %s->%s  %4d px (%4d m)  %d Azimute  schlechtester %-12s %5.3f %% "
              "(+%.2f pp Raster = %5.3f %%)  %-5s %s"
              % (names[a], names[b], res if fine else res_c, round(rng), kSilAzimuths, wv, w,
                 kSilSupShortfallPp, judged, "fein" if fine else "grob",
                 "OK" if judged <= limit else "UEBER GRENZE"))
    ok = worst <= limit
    print("  SIL Grenze %.2f %%  schlechteste Zeile %s->%s %s bei %d px = %.3f %% + %.2f pp  %s"
          % (limit, worst_row[0], worst_row[1], worst_row[2], worst_row[3], worst_row[4],
             kSilSupShortfallPp, "BESTANDEN" if ok else "DURCHGEFALLEN"))
    return dict(limit_pct=limit, passed=bool(ok), azimuths=kSilAzimuths,
                sup_shortfall_pp=kSilSupShortfallPp, coarse_divisor=kSilCoarse,
                worst=dict(pair="%s->%s" % (worst_row[0], worst_row[1]), view=worst_row[2],
                           res=worst_row[3], pct=round(worst_row[4], 4),
                           judged_pct=round(worst, 4)),
                method=(
                    "%d Azimute plus Draufsicht, fest. Die Zahl ist teilerfremd zu jeder "
                    "Polygonteilung der Leiter (96/48/24/20); mit 180/360/720 lieferte L2->L3 "
                    "dreimal exakt 1.5631 %% am selben Azimut, weil ggT(k,20)=20 dieselben 18 "
                    "Facettenphasen abtastet — die Verdopplungsleiter der Runde 3 mass ein Alias "
                    "und nannte es Konvergenz. Auf den gemessenen Wert kommt ein Rasterzuschlag "
                    "von %.2f pp: bei 0.05 Grad Raster liegt das Supremum von L2->L3 bei 1.5938 %%, "
                    "ein 1-Grad-Raster findet 1.5681 %%. Jedes Paar wird zuerst bei 1/%d der "
                    "Aufloesung ueberflogen; nur wer die halbe Grenze reisst, wird voll gemessen "
                    "(grob war an diesem Koerper stets die obere Schranke, Faktor 1.7 bis 3.0)."
                    % (kSilAzimuths, kSilSupShortfallPp, kSilCoarse)),
                rows=rows), ok


def switch_table(stats):
    """Umschaltweiten HERGELEITET — und die MESSUNG loest die Naeherung ab, wo es eine gibt.

    Runde 1/2 nahmen als Treiber das Maximum aus (a) der Rundungsabweichung der naechsten n-Ecke
    und (b) der Cauchy-Merkmalsgroesse sqrt(A/4) des groessten WEGGELASSENEN Koerpers. (b) ist eine
    Naeherung fuer "wann sieht man das Fehlen", und sie ist bei duennen, langen Koerpern grob
    falsch: laesst L3 die Handlaeufe fallen, meldet sie 918 m Umschaltweite, waehrend die gemessene
    Silhouettenaenderung ein Fuenftel der Grenze betraegt. Eine Naeherung, neben der eine Messung
    steht, hat keine Stimme mehr.

    Also: die Weite kommt aus (a), dem einzigen Treiber, der sich geschlossen rechnen laesst.
    (b) bleibt als DIAGNOSE stehen — sie sagt, WAS faellt und wie gross es nach Cauchy ist — und
    das Urteil faellt check_silhouette bei genau dieser Weite.
    """
    def area(me):
        a = 0.0
        for f in me.f:
            p = [np.array(me.v[i]) for i in f]
            for i in range(1, len(p) - 1):
                a += 0.5 * float(np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])))
        return a

    steps, run = [], 0.0
    for i, st in enumerate(stats):
        if i + 1 >= len(stats):
            steps.append(dict(lod=st["lod"], driver=None, feature_m=None, max_range_m=None,
                              drops=[], replaced=[], note="letzte Stufe, keine Umschaltweite"))
            continue
        cur = {m.name: m for _, m in st["_bodies"]}
        nxt = set(m.name for _, m in stats[i + 1]["_bodies"])
        gone = [k for k in cur if k not in nxt]
        rep = [(k, r) for k in gone for pre, r in kSubstitute if k.startswith(pre) and r in nxt]
        repd = set(k for k, _ in rep)
        lost = {k: area(cur[k]) for k in gone if k not in repd}
        seg = stats[i + 1]["segments"]
        feat = G.ring_error(G.kRadius, seg)
        run = max(run, feat / G.kPixelAngle)
        steps.append(dict(lod=st["lod"], driver="rundung n=%d" % seg, feature_m=round(feat, 5),
                          max_range_m=round(run), lost_bodies=len(lost),
                          lost_cauchy_m={k: round(math.sqrt(v / 4.0), 4)
                                         for k in sorted(lost, key=lambda x: -lost[x])[:6]
                                         for v in [lost[k]]},
                          drops=sorted(lost, key=lambda x: -lost[x])[:8],
                          replaced=sorted(set("%s -> %s" % (p, r) for p, r in
                                              ((pre, r) for k, r in rep
                                               for pre, rr in kSubstitute
                                               if k.startswith(pre) and rr == r)))))
    return steps


def sidecar(out_dir, stats, sil, steps):
    doc = {
        "asset": kAsset,
        "name": "Vertical cylindrical fixed-roof storage tank, API 650, 10 310 bbl (48 ft x 32 ft)",
        "unit_scale_m": 1.0,
        "origin": ("Tankachse = z-Achse; z = 0 ist das UMGEBENDE GELAENDE, nicht die Tankunterkante. "
                   "Der Tank steht auf einem Betonring, der %.2f m aus dem Boden ragt "
                   "[API 650 Bild B-1]. Ein Platzierer setzt den Nullpunkt auf das Terrain."
                   % G.kRingwallRise),
        "axes": "glTF +Y oben / -Z vorwaerts; gebaut in +X rechts / +Y vorwaerts / +Z oben",
        "used_by": ("doc/asset-inventory.md #1 — das einzige Objekt, das alle vier Titel platzieren: "
                    "F22 FUELTANK (Grundsatz, jede Mission) · Comanche Typ 3 (Missionsziel in 4 von "
                    "10) · Armored Fist Betankungsdepot · Delta Force oil tank 3008."),
        "sources": {
            "API": ("API Standard 650, 11. Auflage (2007). "
                    "https://law.resource.org/pub/us/cfr/ibr/002/api.650.2007.pdf — jede Zahl mit "
                    "Klausel- oder Tabellennummer, aus dieser Datei gelesen."),
            "SIZE": ("Normgroessentabelle API-Tanks, Zeile 10 310 bbl = 48'-0\" x 32'-0\". "
                     "https://www.piping-designer.com/index.php/disciplines/mechanical/"
                     "stationary-equipment/88-tank/1527-api-tank-size"),
            "OSHA": ("29 CFR 1910.25 Standard stairs — von API 650 5.8.10 a ausdruecklich "
                     "mitverbindlich erklaert. "
                     "https://www.osha.gov/laws-regs/regulations/standardnumber/1910/1910.25"),
        },
        "reference_dimensions_m": {
            "diameter": round(G.kDiameter, 4),
            "shell_height": round(G.kShellHeight, 4),
            "overall_height": round(G.kTankBaseZ + G.kShellHeight + G.kTopAngleThk
                                    + G.kRoofRise, 4),
            "capacity_bbl_nominal": G.kCapacityBbl,
            "capacity_bbl_geometric": round(G.kCapacityCheckBbl, 1),
            "capacity_check": ("pi/4 D^2 H = %.2f m3 = %.1f bbl gegen den Nenninhalt %.0f bbl der "
                               "Tabelle: %+.3f %%. Das ist der Grund, der Tabelle zu glauben."
                               % (G.kVolume, G.kCapacityCheckBbl, G.kCapacityBbl,
                                  G.kCapacityErrRel * 100.0)),
            "courses": G.kCourses,
            "course_height": round(G.kCourseHeight, 4),
            "course_derivation": ("[API 5.6.1.2] fordert 1800 mm Mindestblechbreite. Die Normhoehen "
                                  "16/24/32/40/48 ft sind ALLE Vielfache von 8 ft = 2438 mm — die "
                                  "kleinste Walzbreite ueber der Grenze, die jede von ihnen ohne "
                                  "Rest teilt. 32 ft / 8 ft = 4 Schuesse."),
            "shell_thickness_by_course": [round(t, 4) for t in G.kCourseThk],
            "shell_thickness_source": (
                "[API 5.6.1.1, Tabelle, SI-SPALTE] D < 15 m -> 5 mm; [FUSSNOTE 4] derselben "
                "Tabelle: 'For diameters less than 15 m (50 ft) but greater than 3.2 m (10.5 ft), "
                "the nominal thickness of the lowest shell course shall not be less than 6 mm "
                "(1/4 in.)'. D = 14.6304 m liegt im Band, also ist Schuss 1 dicker. Gelesen wird "
                "die SI-Spalte, weil der Eigner das Projekt metrisch entschieden hat — API 650 "
                "fuehrt zwei Regelsaetze, und 3/16 in sind 4.7625 mm, nicht 5 mm. Probe mit der "
                "1-Meter-Methode [API 5.6.3.2, SI] und Sd = %.0f MPa fuer A 36M [API T.5-2a]: die "
                "Mindestdicke regiert fuer jeden Inhalt mit G <= %.4f, also fuer jedes "
                "Erdoelprodukt und sogar fuer Wasser. Das Netz braucht KEINE Produktdichte."
                % (G.kAllowStressMPa, G.kGoverningSg)),
            "vertical_seams": (
                "%d Bleche je Schuss, zwischen benachbarten Schuessen um ein halbes Blech "
                "versetzt. [API 5.1.5.2 b] fordert den Versatz ('shall be offset from each other "
                "a minimum distance of 5t' = 30 mm), also EXISTIEREN die Naehte; die Blechlaenge "
                "legt die Norm nicht fest ([SET], 7.66 m Bogen). Runde 2 modellierte nur die "
                "waagerechten Naehte — beide Scharen sind gleich gross, und das Nahtraster ist "
                "das Merkmal, an dem man einen geschweissten Lagertank erkennt."
                % G.kPlatesPerCourse),
            "roof_slope": "1:16 [API 5.10.4.1]",
            "roof_rise": round(G.kRoofRise, 5),
            "top_angle": ("50 x 50 x 6 mm [API 5.1.5.9 e] — D = %.4f m liegt im Band "
                          "11 m < D <= 18 m." % G.kDiameter),
            "wind_girder": (
                "KEINER. [API 5.9.7.1, SI] H1 = 9.47 t (t/D)^1.5 = %.4f m mit t = Dicke des "
                "obersten Schusses; [API 5.9.7.2] transformierte Schale %.5f m. 5.9.7.3 "
                "fordert einen Zwischenring erst, wenn die transformierte Hoehe H1 "
                "UEBERSTEIGT — sie tut es nicht. Runde 2 baute einen, weil sie die Schale "
                "in Zoll rechnete: dort ist die transformierte Schale 27.897 ft gegen H1 "
                "27.466 ft und der Ring Pflicht. Das Bauteil existierte nur als Folge der "
                "Einheitenmischung." % (G.kWindH1, G.kShellTransposed)),
            "ringwall_width": G.kRingwallWidth,
            "ringwall_source": ("[API B.4.2.2] 'The ringwall shall not be less than 300 mm thick. "
                                "The centerline diameter of the ringwall should equal the nominal "
                                "diameter of the tank.'"),
        },
        "stair": {
            "steps": G.kStairSteps,
            "rise": round(G.kStairRise, 5),
            "run": round(G.kStairRun, 5),
            "angle_deg": round(math.degrees(G.kStairAngle), 3),
            "width": G.kStairWidth,
            "wrap_deg": round(math.degrees(G.kStairWrap), 2),
            "posts": G.kStairPosts,
            "handrail_outer_only": True,
            "handrail_measure": G.kRailMeasure,
            "derivation": (
                "Zwei Schritte, beide belegt. (1) Aus den Zeilen der [API T.5-19a] mit 2R + r = 610 "
                "die STEILSTE nehmen, die OSHA 1910.25(c) haelt: r = 610 - 2R >= 241.3 mm erzwingt "
                "R <= 184.35 mm, also die gedruckte Zeile R = 180 mm / r = 250 mm / 35 deg 45 min. "
                "(2) N = aufgerundet(Steighoehe / R) fuer gleiche Steigungen [T.5-18 Pkt.4], dann R "
                "zurueck auf Steighoehe / N: N = %d, R = %.2f mm, r = %.2f mm, Winkel %.2f Grad, "
                "Umschlingung %.1f Grad. Runde 1 klemmte 2R + r stur auf die untere Bandgrenze und "
                "landete bei 182.79 / 244.41 mm — keine Zeile der Tabelle. Die STEIGHOEHE ist seit "
                "Runde 4 die Schalenhoehe und nicht mehr die Hoehe ueber Gelaende: [T.5-18 Pkt.10] "
                "sagt 'from the bottom of the tank', und am Gelaende beginnend lag die unterste "
                "Stufe unter der Ringmauerkrone und wurde auf 553 mm beschnitten."
                % (G.kStairSteps, G.kStairRise * 1000, G.kStairRun * 1000,
                   math.degrees(G.kStairAngle), math.degrees(G.kStairWrap))),
            "base_z": round(G.kStairBaseZ, 4),
            "width_rule": (
                "[API T.5-18 Pkt.2] 710 mm. Der Aussenradius folgt der GEBAUTEN Innenkante von L0 "
                "(Polygonecke + Schweissraupe, ueber die Sehne des Stufenbogens), nicht dem "
                "Kreis — bei festem Aussenradius blieben 703.6 mm."),
            "handrail_transition": (
                "[T.5-18 Pkt.6] verlangt den Anschluss an das Podestgelaender 'without offset'. "
                "Treppengelaender %.0f mm ueber der Stufenvorderkante, Podestgelaender %.0f mm "
                "ueber dem Podest — am Uebergang koennen sie nicht gleich hoch sein, also rampt "
                "der Lauf ueber die letzten %d Stufen hoch. Runde 1 liess dort 210 mm Absatz."
                % (G.kStairRailH * 1000, G.kPlatformRailH * 1000, G.kStairRailRampSteps)),
            "handrail_rule": ("[API T.5-18 Pkt.9] verlangt den INNEREN Handlauf runder Treppen "
                              "erst, wenn der Abstand Schale <-> Wange 200 mm uebersteigt. Die "
                              "innere Wange liegt auf der Schale [Pkt.10], also entfaellt er."),
        },
        "platform": {
            "required_by": "[API 5.8.10 c] — Podest an der Dachkante ist Pflicht, kein Zierat.",
            "clear_width": G.kPlatformClear,
            "clear_width_rule": (
                "[API T.5-17 Pkt.2] 610 mm LICHT, 'after making adjustments at all projections'. "
                "Runde 3 rechnete den Abzug und pruefte ihn mit einem assert, der eine "
                "Substitution war: width = clear + intrude, dann clear = width - intrude. Er "
                "konnte nicht fehlschlagen und uebersah, dass die Fussleiste nach AUSSEN sitzt und "
                "gar nicht hineinragt — am Netz waren es 618 statt 610 mm. Gemessen wird jetzt am "
                "gebauten Koerper: engster Punkt irgendeines Koerpers ueber dem Podestboden im "
                "Podestbogen, minus die Innenkante des Bodens (s. norms)."),
            "rail_height": G.kPlatformRailH,
            "toeboard": G.kPlatformToeH,
            "source": "[API T.5-17] Pkt. 2, 4, 5, 7, 8.",
        },
        "roof_rail": {
            "required_by": (
                "[API 5.8.10 a] erklaert OSHA 29 CFR 1910 Subpart D fuer Podeste, Laufstege und "
                "Treppen mitverbindlich. [OSHA 1910.28(b)(1)(i)]: eine ungeschuetzte Kante ab "
                "1.2 m ueber dem naechsten Niveau verlangt Absturzsicherung, zulaessig u. a. "
                "'guardrail systems'. Die Dachkante liegt %.2f m ueber Gelaende, und das Dach ist "
                "eine begehbare Flaeche (Dachmannloch, Entlueftung, Peilzugang). Runde 3 baute "
                "nur den 2.4-m-Bogen des Podests; das Fehlende kostete gemessen 1.293 / 1.441 / "
                "0.968 %% Silhouette bei 11 / 43 / 62 m — mehr als jede LOD-Stufe ausser L2->L3 "
                "und das 3.3-fache der 0.389 %%, mit denen Runde 3 die Gelaender auf L3 fallen "
                "liess. Ein Fehler gegen die WIRKLICHKEIT; die 2-%%-Schranke ist ein "
                "Selbstkonsistenz-Budget und deckt ihn nicht."
                % _roof_top_z()),
            "posts": G.kRoofRailPosts,
            "post_spacing_m": round(G.kRoofRailSpacing, 4),
            "post_spacing_rule": "[API T.5-17 Pkt.8] hoechstens 2400 mm",
            "height_m": G.kRoofRailH,
            "height_rule": ("[API T.5-17 Pkt.4] 1070 mm ueber der Standflaeche, gemessen als "
                            "Oberkante [OSHA 1910.29(b)(1)]; die Standflaeche ist hier das "
                            "Dachblech an der Traufe."),
            "midrail_rule": "[API T.5-17 Pkt.7] / [OSHA 1910.29(b)(2)] auf halber Hoehe",
            "path": ("folgt der GEBAUTEN Polygontraufe, nicht dem Kreis — auf dem Kreisradius "
                     "haette der Ring bei n = 20 an den Facettenmitten 91 mm ueber der Dachkante "
                     "gestanden. Die Pfostenachse liegt um einen Pfostenradius nach innen [SET], "
                     "damit die Pfosten ganz auf dem Blech stehen."),
            "toeboard": ("KEINE. [API T.5-17 Pkt.5] gilt dem Laufsteg, [OSHA 1910.29(k)] dem "
                         "Schutz vor herabfallenden Teilen ueber Arbeitsplaetzen [1910.28(c)]. "
                         "Unter der Dachkante steht niemand. [SET], s. DEFECTS.md."),
            "on_all_lods": ("ja, auch auf L3. Gegen den Himmel ist der Ring die kontrastreichste "
                            "Linie des Koerpers; ihn fallen zu lassen kostete mehr als die "
                            "Abtastung, die auf L3 ohnehin faellt."),
        },
        "norms": {
            "rule": ("Jede Normzahl wird AM GEBAUTEN NETZ nachgemessen, nie an den Eingaben "
                     "nachgerechnet. Runde 3 hatte im Bauskript genau einen assert, und der war "
                     "eine Substitution der Form (a+b)-b >= a — er konnte nicht fehlschlagen. "
                     "Jetzt sind es %d Masse aus Punktkoordinaten: Radien, Boegen, "
                     "Hoehendifferenzen, Umfangsteilung aus dem Traufenring GEZAEHLT."
                     % sum(1 for r in stats[0]["norms"] if r["passed"] is not None)),
            "judged_on": ("L0. L1..L3 werden vollstaendig gemessen und hier abgelegt, aber nicht "
                          "geurteilt: L0 IST der Koerper, die groberen Stufen sind Naeherungen "
                          "davon, deren Abweichung das Silhouettentor begrenzt. Eine "
                          "Fertigungsnorm auf ein 20-Eck anzuwenden hiesse, die Fernsicht ueber "
                          "die Bauform entscheiden zu lassen."),
            "found_this_round": [
                "stair.width 553 mm gegen 710 mm [T.5-18 Pkt.2] — die unterste Stufe wich der "
                "Ringmauer aus. Behoben: die Treppe beginnt am TANKBODEN, wie [T.5-18 Pkt.10] es "
                "woertlich sagt ('from the bottom of the tank'), nicht am Gelaende.",
                "stair.width 703.6 mm nach dieser Korrektur — der Aussenradius stand fest, "
                "waehrend die Innenkante auf der gebauten Wand sitzt (Polygonecke + Raupe). "
                "Behoben: der Aussenradius folgt der gebauten Innenkante von L0.",
                "platform.post_spacing 2490 mm gegen 2400 mm [T.5-17 Pkt.8] — die Pfostenzahl kam "
                "aus dem Bogen der Laufsteg-MITTELLINIE, die Pfosten stehen aber am Aussenholm.",
            ],
        },
        "moving_nodes": ("keine. Der Tank hat kein Gelenk — die Mannlochdeckel sind geschraubt, "
                         "nicht gelagert. Die Knoten sind trotzdem hierarchisch benannt "
                         "(foundation / tank / access), damit ein Renderer Baugruppen einzeln "
                         "schalten und ein spaeterer Schadensfall sie einzeln ersetzen kann."),
        "materials": {
            "count": len(kMat),
            "textures": 0,
            "decision": ("KEINE Textur, absichtlich. doc/render/visual-target.md §1: 60 GB/s "
                         "Bandbreite gegen 2.5..4 TFLOPS — Bandbreite ist der Mangel. Was diesen "
                         "Koerper lesbar macht (Rundnaehte, Kopfwinkel, Windring, Treppe), ist "
                         "GEOMETRIE. Rost und Beschriftung gehoeren in einen prozeduralen Shader "
                         "des Renderers, nicht in eine Datei, die je Bild ueber den Bus muss."),
            "collapse_L2_L3": ("steel und dark fallen auf paint zusammen: ab %d m ist der "
                               "Farbunterschied unter einem Pixel breit, und es spart zwei "
                               "Zeichenaufrufe. concrete bleibt eigen — der Wertunterschied "
                               "Beton/Anstrich traegt die Standflaeche noch weit draussen."
                               % round(G.lod_range(G.kLodSegments[2]))),
        },
        "self_checks": {
            "spec": "doc/assets.md §3.1",
            "watertight": ("je Koerper: keine Randkante, jede Kante genau zweimal (einmal je "
                           "Richtung), Euler-Charakteristik ergibt ein ganzzahliges Geschlecht "
                           ">= 0. Die Ringkoerper haben Geschlecht 1, das ist die Sollmessung."),
            "winding": "jede gerichtete Kante genau einmal; ihre Gegenkante existiert.",
            "normals": ("endlich, Einheitslaenge auf 1e-6, jede Eck-Normale im selben Halbraum "
                        "wie die Flaechennormale; Volumenintegral je Koerper > 0 (aussen)."),
            "welded": "keine zwei verschiedenen Punkte naeher als %.0e m." % EPS_WELD,
            "t_junctions": ("kein Punkt im Inneren einer Kante, an der er nicht haengt "
                            "(Toleranz 2 x Schweissabstand)."),
            "merge": ("Alle Koerper werden WIRKLICH zu einem Netz verschmolzen; verglichen werden "
                      "Volumen und Huellquader gegen die Teile (dV < 1e-9 relativ, dBox < 1e-9). "
                      "Runde 1 meldete nur zwei Zahlen nebeneinander und verglich nie."),
            "interpenetration": (
                "ZUSATZ zu §3.1, weil der Merge-Test ihn nicht leisten kann: das Divergenzintegral "
                "ist additiv, ob sich zwei Koerper ueberlappen oder nicht. Geprueft wird deshalb "
                "paarweise auf gemeinsames INNENvolumen (Huellquader sieben, Schnittquader "
                "abtasten, beidseitig einschliessen per Strahltest mit schraeger, fester "
                "Richtung). Runde 1 hatte 47 Paare mit geteiltem Volumen, darunter eine "
                "Treppenstufe quer durch den Windring — von nichts gemessen. Ausgenommen sind nur "
                "die in kJoint benannten SCHWEISSPUNKTE, und auch die nur bis zu einem "
                "Anteil %.2f des kleineren Bauteils. Wo schon der SCHNITTQUADER unter dieser "
                "Kappe liegt, ist sie bewiesen statt geschaetzt und die Gitterprobe entfaellt — "
                "das halbiert die Torzeit und ist strenger, nicht schwaecher. Was der Detektor "
                "unterhalb der Urteilsschwelle %.2f cm3 findet, wird GEMELDET "
                "(overlaps_below_verdict), nicht mehr verworfen." % (kJointFrac, kOverlapMinCm3)),
            "lod_continuity": (
                "Ausgeglichene Ringkorrektur (Flaeche gegen Umfang) plus MESSUNG: %d Azimute, "
                "teilerfremd zu jeder Polygonteilung der Leiter, bei der Aufloesung, die die "
                "Umschaltweite hergibt, mit einem gemessenen Rasterzuschlag von %.2f pp. Runde 2 "
                "tastete 13 Azimute ab (1.964 %%, wahr 2.378 %%); Runde 3 verdoppelte bis zur "
                "'Konvergenz' und mass dabei ein ALIAS — 180, 360 und 720 Azimute lieferten fuer "
                "L2->L3 dreimal exakt denselben Wert am selben Azimut, weil ggT(k,20)=20 ist. Das "
                "kumulierte Paar zaehlt im Urteil MIT." % (kSilAzimuths, kSilSupShortfallPp)),
            "determinism": ("zweimal bauen, Bytes vergleichen — s. acceptance.rebuild_identical."),
        },
        "silhouette": sil,
        "lod_rule": ("Die Umschaltweite kommt aus der Rundungsabweichung der naechsten n-Ecke: "
                     "sie faellt unter ein Pixel bei R = ring_error / Pixelwinkel, Pixelwinkel = "
                     "60 Grad / 1280 px = %.4e rad [doc/render/visual-target.md §1, 720p30]. Die "
                     "Cauchy-Merkmalsgroesse sqrt(A/4) weggelassener Koerper ist NUR noch Diagnose "
                     "(lod_switch.lost_cauchy_m): sie meldete fuer die Handlaeufe 918 m, waehrend "
                     "die gemessene Silhouettenaenderung ein Bruchteil der Grenze ist. Wo eine "
                     "Messung steht, hat die Naeherung keine Stimme. Die Weiten sind UNTERE "
                     "Schranken; ein Renderer darf frueher schalten und zahlt den Fehler."
                     % G.kPixelAngle),
        "lod_switch": steps,
        "lods": [{k: v for k, v in s.items() if not k.startswith("_")} for s in stats],
        "acceptance": {
            "reference": ("API Std 650 fuer jede Bauteilabmessung; die Normgroessentabelle fuer "
                          "Durchmesser und Hoehe; OSHA 1910.25 als zweites Regelwerk fuer die "
                          "Treppe. Kein Foto, kein Augenmass."),
            "tolerance": ("Jede Hauptabmessung ist EXAKT die Zahl ihrer SI-Tabellenspalte. "
                          "Die einzige Abweichung im Bau ist die Ringkorrektur, und die ist "
                          "beabsichtigt und beziffert (s. lod_switch.feature_m)."),
            "checks_all_green": all(not s["checks_failed"] for s in stats),
            "rebuild_identical": ("Gemessen: drei Laeufe in drei verschiedene Ausgabeverzeichnisse "
                                  "liefern vier bytegleiche .glb und eine bytegleiche .asset.json "
                                  "(cmp und sha256). Nachstellen: build_fuel_tank.py --out A, "
                                  "--out B, dann cmp A/* B/*."),
            "bbox_grows_with_lod": (
                "Der Huellquader waechst mit groeberer Teilung — das ist die Ringkorrektur, kein "
                "Massfehler. Die n-Ecke liegt an den Ecken aussen und in den Kantenmitten innen; "
                "der Nennradius bleibt 7.3152 m. Der Korrekturfaktor gleicht Flaechen- und "
                "Umfangsfehler gegeneinander aus (je %+.4f / %+.4f %% bei n = %d), statt einen "
                "auf null zu zwingen: das Tor misst XOR-FLAECHE, und in der Draufsicht ist "
                "Flaeche kein Umfang."
                % (100 * G.ring_balance(G.kLodSegments[-1])[0],
                   100 * G.ring_balance(G.kLodSegments[-1])[1], G.kLodSegments[-1])),
            "open_defects": "DEFECTS.md",
        },
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
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    levels = [kLod[int(a.lod)]] if a.lod else kLod
    stats = [build_lod(c, a.out, a.blend) for c in levels]
    rc = 1 if any(s["checks_failed"] for s in stats) else 0
    if len(stats) > 1:
        steps = switch_table(stats)
        t0 = time.time()
        sil, ok = check_silhouette(stats, [x["max_range_m"] or 0.0 for x in steps])
        # Die Bauzeit steht auf stdout und NICHT im Sidecar: sie ist eine Eigenschaft der
        # Maschine, nicht des Koerpers, und wuerde die Bytegleichheit zweier Laeufe zerstoeren.
        print("  ZEIT  silhouette %.1f s" % (time.time() - t0))
        rc = rc or (0 if ok else 1)
    else:
        sil, steps = dict(passed=None, rows=[]), []
    if len(stats) == len(kLod):
        sidecar(a.out, stats, sil, steps)
    print("ERGEBNIS %s  (%.1f s)" % ("ALLES GRUEN" if rc == 0 else "DURCHGEFALLEN",
                                     time.time() - t_start))
    return rc


if __name__ == "__main__":
    sys.exit(main())

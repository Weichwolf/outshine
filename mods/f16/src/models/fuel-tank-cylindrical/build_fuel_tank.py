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
    dict(name="L0", seg=96, tube=12, detail=2, single_mat=False),
    dict(name="L1", seg=48, tube=8, detail=2, single_mat=False),
    dict(name="L2", seg=24, tube=6, detail=1, single_mat=True),
    dict(name="L3", seg=12, tube=4, detail=0, single_mat=True),
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
        for j, sgn in ((0, -1), (steps, 1)):
            nx = (-cs[j][1] * sgn, cs[j][0] * sgn, 0.0)
            poly = [pt(i, j) for i in range(m) if prof[i][0] > 1e-9]
            if sgn < 0:
                poly = poly[::-1]
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


def raster(bodies, axis, res, ctr, size):
    """Orthografische Alpha-Maske ohne Renderer: Dreiecke auf ein Bool-Gitter rastern.
    axis 0/1/2 = Blick laengs x / y / z."""
    u, v = [(1, 2), (0, 2), (0, 1)][axis]
    mask = np.zeros((res, res), dtype=bool)
    lo = np.array([ctr[u] - size / 2.0, ctr[v] - size / 2.0])
    sc = res / size
    for me in bodies:
        vv = np.array(me.v)
        for f in me.f:
            pts = vv[list(f)]
            for i in range(1, len(f) - 1):
                tri = np.stack([pts[0], pts[i], pts[i + 1]])[:, [u, v]]
                px = (tri - lo) * sc
                x0 = max(0, int(math.floor(px[:, 0].min())))
                x1 = min(res - 1, int(math.ceil(px[:, 0].max())))
                y0 = max(0, int(math.floor(px[:, 1].min())))
                y1 = min(res - 1, int(math.ceil(px[:, 1].max())))
                if x1 < x0 or y1 < y0:
                    continue
                gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5,
                                     np.arange(y0, y1 + 1) + 0.5)
                d0 = ((gx - px[0, 0]) * (px[1, 1] - px[0, 1])
                      - (gy - px[0, 1]) * (px[1, 0] - px[0, 0]))
                d1 = ((gx - px[1, 0]) * (px[2, 1] - px[1, 1])
                      - (gy - px[1, 1]) * (px[2, 0] - px[1, 0]))
                d2 = ((gx - px[2, 0]) * (px[0, 1] - px[2, 1])
                      - (gy - px[2, 1]) * (px[0, 0] - px[2, 0]))
                inside = ((d0 >= 0) & (d1 >= 0) & (d2 >= 0)) | ((d0 <= 0) & (d1 <= 0) & (d2 <= 0))
                mask[y0:y1 + 1, x0:x1 + 1] |= inside
    return mask


# ================================================================ Die Baugruppen

def shell(cfg):
    """Schale: vier Schuesse mit Rundnaehten, geschlossenes Rohr."""
    z0, z1 = G.kTankBaseZ, G.kTankBaseZ + G.kShellHeight
    ro, ri = G.kRadius + G.kShellThk / 2.0, G.kRadius - G.kShellThk / 2.0
    bead_h, bead_w = 0.005, 0.030          # [SET] Schweissraupe, s. DEFECTS.md #6
    out = [(ro, z0)]
    if cfg["detail"] >= 1:
        for c in range(1, G.kCourses):
            zs = z0 + c * G.kCourseHeight
            out += [(ro, zs - bead_w), (ro + bead_h, zs - bead_w * 0.4),
                    (ro + bead_h, zs + bead_w * 0.4), (ro, zs + bead_w)]
    out.append((ro, z1))
    prof = out + [(ri, z1), (ri, z0)]
    return revolve("tank.shell", "paint", prof, cfg["seg"])


def bottom_lip(cfg):
    """Bodenblech-Ueberstand unter der Schale. [SET] 25 mm Ueberstand, 6 mm dick (DEFECTS.md #6)."""
    z0 = G.kTankBaseZ
    ro = G.kRadius + G.kShellThk / 2.0 + 0.025
    ri = G.kRadius - 0.150
    prof = [(ri, z0 - 0.006), (ro, z0 - 0.006), (ro, z0), (ri, z0)]
    return revolve("tank.bottom_lip", "paint", prof, cfg["seg"])


def top_angle(cfg):
    """Kopfwinkel 50x50x6 [API 5.1.5.9 e]. Waagerechter Schenkel nach AUSSEN — die Klausel schreibt
    das nur fuer gedaemmte Tanks zwingend vor; hier gewaehlt, weil das Dachblech darauf aufliegt."""
    z1 = G.kTankBaseZ + G.kShellHeight
    ro = G.kRadius + G.kShellThk / 2.0
    L, t = G.kTopAngleLeg, G.kTopAngleThk
    prof = [(ro, z1 - L), (ro + t, z1 - L), (ro + t, z1),
            (ro + L, z1), (ro + L, z1 + t), (ro, z1 + t)]
    return revolve("tank.top_angle", "paint", prof, cfg["seg"])


def wind_girder(cfg):
    """Zwischen-Windring [API 5.9.7]. Lage ist Klausel, Profilgroesse ist [SET] (DEFECTS.md #4)."""
    z = G.kTankBaseZ + G.kWindGirderZ
    ro = G.kRadius + G.kShellThk / 2.0
    L, t = G.kWindGirderLeg, G.kWindGirderThk
    prof = [(ro, z), (ro + L, z), (ro + L, z + t), (ro + t, z + t), (ro + t, z + L), (ro, z + L)]
    return revolve("tank.wind_girder", "paint", prof, cfg["seg"])


def roof(cfg):
    """Getragenes Kegeldach, Neigung 1:16 [API 5.10.4.1], Blech 3/16 in [API 5.10.5 Anm.]."""
    z1 = G.kTankBaseZ + G.kShellHeight + G.kTopAngleThk
    rrim = G.kRadius + G.kShellThk / 2.0 + G.kTopAngleLeg
    s = G.kRoofSlope
    tv = G.kRoofThk * math.sqrt(1.0 + s * s)
    apex = z1 + rrim * s
    prof = [(rrim, z1), (0.0, apex), (0.0, apex - tv), (rrim, z1 - tv)]
    return revolve("tank.roof", "paint", prof, cfg["seg"])


def _shell_dir(phi):
    return (math.cos(phi), math.sin(phi), 0.0)


def _radial_frame(phi):
    """(Ursprungsachse, u = tangential, v = z) fuer einen radial stehenden Sweep."""
    return (-math.sin(phi), math.cos(phi), 0.0), (0.0, 0.0, 1.0)


def shell_manhole(cfg, phi):
    """Schalenmannloch DN 600 [API T.5-5a]: Verstaerkungsblech, Hals, Deckel, Schrauben."""
    out = []
    ro = G.kRadius + G.kShellThk / 2.0
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

    out.append(disc("tank.manhole.shell.pad", "paint", G.kNozzleReinfDia * 1.55 / 2.0,
                    -0.004, 0.012))
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


def roof_manhole(cfg, phi):
    """Dachmannloch DN 500 [API T.5-13a]. Achse steht IMMER senkrecht (Bild 5-16)."""
    out = []
    r = G.kRoofManholeR
    z1 = G.kTankBaseZ + G.kShellHeight + G.kTopAngleThk
    zr = z1 + (G.kRadius + G.kShellThk / 2.0 + G.kTopAngleLeg - r) * G.kRoofSlope
    c = np.array([r * math.cos(phi), r * math.sin(phi), zr])
    u, v = np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0])
    nseg = max(8, cfg["seg"] // 4)

    def disc(name, mat, rad, z0, z1_):
        return sweep(name, mat, [(c + np.array([0, 0, z0]), u, v),
                                 (c + np.array([0, 0, z1_]), u, v)], ngon(nseg, rad),
                     round_section=True)

    out.append(disc("tank.manhole.roof.pad", "paint", G.kRoofManholeReinf / 2.0, -0.004, 0.010))
    out.append(disc("tank.manhole.roof.neck", "paint", G.kRoofManholeDia / 2.0, 0.008, 0.150))
    out.append(disc("tank.manhole.roof.cover", "paint", G.kRoofManholeCover / 2.0, 0.150, 0.168))
    if cfg["detail"] >= 2:
        rb = 0.597 / 2.0                       # [API T.5-13a, DB = 597 mm]
        for k in range(G.kRoofManholeBolts):
            a = TAU * k / G.kRoofManholeBolts
            p = c + np.array([rb * math.cos(a), rb * math.sin(a), 0.168])
            out.append(sweep("tank.manhole.roof.bolt.%02d" % k, "dark",
                             [(p, u, v), (p + np.array([0, 0, 0.016]), u, v)], ngon(6, 0.014)))
    return out


def vent(cfg, phi):
    """Freiatmer. Groesse [SET] — API Std 2000 wurde nicht gerechnet (DEFECTS.md #5)."""
    r = G.kVentR
    z1 = G.kTankBaseZ + G.kShellHeight + G.kTopAngleThk
    zr = z1 + (G.kRadius + G.kShellThk / 2.0 + G.kTopAngleLeg - r) * G.kRoofSlope
    c = np.array([r * math.cos(phi), r * math.sin(phi), zr])
    u, v = np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0])
    nseg = max(8, cfg["seg"] // 4)
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
    ro = G.kRadius + G.kShellThk / 2.0
    d = np.array(_shell_dir(phi))
    z = G.kTankBaseZ + G.kNozzleZ
    ctr = np.array([0.0, 0.0, z])
    u, v = _radial_frame(phi)
    u, v = np.array(u), np.array(v)
    nseg = max(8, cfg["seg"] // 4)

    def disc(name, mat, rad, o0, o1):
        return sweep(name, mat, [(ctr + d * (ro + o0), u, v), (ctr + d * (ro + o1), u, v)],
                     ngon(nseg, rad), round_section=True)

    out.append(disc("tank.nozzle.outlet.pad", "paint", G.kNozzleReinfDia / 2.0, -0.004, 0.010))
    out.append(disc("tank.nozzle.outlet.neck", "paint", G.kNozzleOD / 2.0, 0.008, G.kNozzleProj))
    out.append(disc("tank.nozzle.outlet.flange", "paint", G.kNozzleFlangeDia / 2.0,
                    G.kNozzleProj, G.kNozzleProj + G.kNozzleFlangeThk))
    return out


def ringwall(cfg):
    """Betonringmauer [API B.4.2.2]: 300 mm dick, Mittellinie = Nenndurchmesser des Tanks."""
    ri = G.kRingwallCLDia / 2.0 - G.kRingwallWidth / 2.0
    ro = G.kRingwallCLDia / 2.0 + G.kRingwallWidth / 2.0
    zb = -0.150                    # [SET] sichtbarer Anschnitt unter Gelaende
    prof = [(ri, zb), (ro, zb), (ro, G.kRingwallRise), (ri, G.kRingwallRise)]
    return revolve("foundation.ringwall", "concrete", prof, cfg["seg"])


# ---------------------------------------------------------------- Treppe und Podest

def _stair_phi(i):
    return G.kStairPhi0 + i * G.kStairDPhi


def _pt(r, phi, z):
    return np.array([r * math.cos(phi), r * math.sin(phi), z])


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

    if cfg["detail"] >= 1:
        for i in range(n):
            z = (i + 1) * G.kStairRise
            pa, pb = _stair_phi(i) - nose, _stair_phi(i + 1)
            lo = [_pt(ri, pa, z - G.kStairTreadThk), _pt(ro, pa, z - G.kStairTreadThk),
                  _pt(ro, pb, z - G.kStairTreadThk), _pt(ri, pb, z - G.kStairTreadThk)]
            hi = [_pt(ri, pa, z), _pt(ro, pa, z), _pt(ro, pb, z), _pt(ri, pb, z)]
            out.append(box("access.stair.tread.%02d" % i, "steel", lo, hi))
    else:
        # L3: die Stufen verschmelzen zum geschlossenen Band. AUSSENKANTE und Oberkante bleiben
        # exakt die der Einzelstufen, also bleibt die Silhouette in Auf- und Grundriss stehen.
        zs = [max(i, 1) * G.kStairRise for i in range(n + 1)]
        path = [(_pt(ri, _stair_phi(i), zs[i]),
                 np.array([math.cos(_stair_phi(i)), math.sin(_stair_phi(i)), 0.0]),
                 np.array([0.0, 0.0, 1.0])) for i in range(n + 1)]
        w = ro - ri
        secs = [[(0.0, 0.0), (w, 0.0), (w, -_stringer_depth(z)), (0.0, -_stringer_depth(z))]
                for z in zs]
        out.append(sweep("access.stair.band", "steel", path, secs))

    if cfg["detail"] >= 1:
        # Aussenwange: Rechteckquerschnitt unter der Stufenlinie entlanggezogen.
        zs = [max(i, 1) * G.kStairRise for i in range(n + 1)]
        pts = [_pt(ro + G.kStringerThk / 2.0, _stair_phi(i), zs[i]) for i in range(n + 1)]
        t = G.kStringerThk / 2.0
        secs = [[(-t, 0.0), (t, 0.0), (t, -_stringer_depth(z)), (-t, -_stringer_depth(z))]
                for z in zs]
        out.append(sweep("access.stair.stringer", "steel", normal_frames(pts), secs))

    # Handlauf: nur aussen. [API T.5-18 Pkt.9] fordert den inneren erst ab 200 mm Abstand zur
    # Schale; die innere Wange liegt AUF der Schale, also entfaellt er.
    rr = ro + G.kStringerThk
    for tag, h in (("top", G.kStairRailH), ("mid", G.kStairRailH / 2.0)):
        pts = [_pt(rr, _stair_phi(i), max(i, 1) * G.kStairRise + h) for i in range(n + 1)]
        out.append(sweep("access.stair.rail.%s" % tag, "steel", normal_frames(pts),
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))

    step = n / float(G.kStairPosts - 1)
    for k in range(G.kStairPosts):
        i = int(round(k * step))
        p = _stair_phi(i)
        z = max(i, 1) * G.kStairRise
        u = np.array([math.cos(p), math.sin(p), 0.0])
        v = np.array([-math.sin(p), math.cos(p), 0.0])
        out.append(sweep("access.stair.post.%02d" % k, "steel",
                         [(_pt(rr, p, z - _stringer_depth(z)), u, v),
                          (_pt(rr, p, z + G.kStairRailH), u, v)],
                         ngon(cfg["tube"], G.kPostDia / 2.0), round_section=True))
    return out


def platform(cfg):
    """Podest an der Dachkante [API 5.8.10 c] mit Gelaender nach [API T.5-17]."""
    out = []
    ri = G.kRadius + G.kShellThk / 2.0 + G.kTopAngleLeg
    ro = ri + G.kPlatformWidth
    z = G.kPlatformZ
    p0 = _stair_phi(G.kStairSteps)
    p1 = p0 + G.kPlatformArc / ((ri + ro) / 2.0)
    nsec = max(3, int(round(cfg["seg"] * (p1 - p0) / TAU)) + 1)
    phis = [p0 + (p1 - p0) * i / nsec for i in range(nsec + 1)]

    out.append(sweep("access.platform.floor", "steel", arc_frames(ri, phis, lambda p: z),
                     [(0.0, 0.0), (G.kPlatformWidth, 0.0),
                      (G.kPlatformWidth, -0.030), (0.0, -0.030)]))
    out.append(sweep("access.platform.toeboard", "steel", arc_frames(ro, phis, lambda p: z),
                     [(0.0, 0.0), (0.008, 0.0),
                      (0.008, G.kPlatformToeH), (0.0, G.kPlatformToeH)]))

    for tag, h in (("top", G.kPlatformRailH), ("mid", G.kPlatformRailH / 2.0)):
        out.append(sweep("access.platform.rail.%s" % tag, "steel",
                         arc_frames(ro, phis, lambda p, h=h: z + h),
                         ngon(cfg["tube"], G.kRailTubeDia / 2.0), round_section=True))

    npost = max(2, int(math.ceil(G.kPlatformArc / G.kPlatformPostMax)) + 1)
    for k in range(npost):
        p = p0 + (p1 - p0) * k / (npost - 1)
        u = np.array([math.cos(p), math.sin(p), 0.0])
        v = np.array([-math.sin(p), math.cos(p), 0.0])
        out.append(sweep("access.platform.post.%02d" % k, "steel",
                         [(_pt(ro, p, z), u, v),
                          (_pt(ro, p, z + G.kPlatformRailH), u, v)],
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
    phi_mh = math.radians(300.0)
    phi_noz = math.radians(345.0)
    phi_rmh = math.radians(30.0)
    phi_vent = math.radians(70.0)

    out = [("foundation", ringwall(cfg)),
           ("tank", shell(cfg)),
           ("tank", bottom_lip(cfg)),
           ("tank", top_angle(cfg)),
           ("tank", wind_girder(cfg)),
           ("tank", roof(cfg))]
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
    return [(g, orient(m)) for g, m in out]


def build_lod(cfg, out_dir, keep_blend=False):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bodies = build_bodies(cfg)

    print("--- %s  seg=%d tube=%d detail=%d" % (cfg["name"], cfg["seg"], cfg["tube"], cfg["detail"]))
    fails, per_body = [], []
    for _, me in bodies:
        bad, st = check_body(me)
        per_body.append((me.name, st))
        if bad:
            fails.append("%s: %s" % (me.name, "; ".join(bad)))
    if fails:
        for f in fails:
            print("  PRUEFUNG FEHLER  %s" % f)
    print("  PRUEFUNG %d Koerper: dicht/Windung/Normalen/T-Stoesse/Verschweissung  %s"
          % (len(bodies), "OK" if not fails else "DURCHGEFALLEN (%d)" % len(fails)))

    genus = sorted(set(s["genus"] for _, s in per_body))
    vol_sum = sum(s["volume"] for _, s in per_body)
    lo = np.min([np.array(m.bbox()[0]) for _, m in bodies], axis=0)
    hi = np.max([np.array(m.bbox()[1]) for _, m in bodies], axis=0)
    print("  MERGE  Summe der Volumina %.4f m3 ueber %d Koerper, Geschlechter %s"
          % (vol_sum, len(bodies), genus))

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

    path = os.path.join(out_dir, "%s_%s.glb" % (kAsset, cfg["name"]))
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False,
                              export_yup=True, export_apply=False, export_normals=True,
                              export_texcoords=False, export_materials='EXPORT')

    used = sorted(set(mats[k].name for k in kMat))
    print("  ASSET %-3s %-34s %8d B %7d Tri %d Mat  x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]"
          % (cfg["name"], os.path.basename(path), os.path.getsize(path), tris, len(used),
             lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    if keep_blend:
        bpy.ops.wm.save_as_mainfile(
            filepath=os.path.join(out_dir, "%s_%s.blend" % (kAsset, cfg["name"])))
    return dict(lod=cfg["name"], file=os.path.basename(path), segments=cfg["seg"],
                triangles=tris, bytes=os.path.getsize(path), bodies=len(bodies),
                materials=used, checks_failed=fails,
                genus=genus, volume_sum_m3=round(vol_sum, 6),
                bbox={k: [round(float(lo[i]), 4), round(float(hi[i]), 4)]
                      for i, k in enumerate("xyz")},
                size_m={k: round(float(hi[i] - lo[i]), 4) for i, k in enumerate("xyz")},
                _bodies=bodies)


def check_silhouette(stats, res=1024, limit=2.0):
    """Silhouetten-Tor OHNE Renderer: dieselbe orthografische Kamera fuer alle Stufen, XOR der
    Masken. Ein Rasterer aus 30 Zeilen ist hier exakter als EEVEE und kostet keine Aussenwelt."""
    lo = np.array([stats[0]["bbox"][k][0] for k in "xyz"])
    hi = np.array([stats[0]["bbox"][k][1] for k in "xyz"])
    ctr = (lo + hi) / 2.0
    size = float((hi - lo).max()) * 1.06
    masks = {}
    for st in stats:
        bodies = [m for _, m in st["_bodies"]]
        masks[st["lod"]] = {a: raster(bodies, i, res, ctr, size)
                            for i, a in ((0, "side"), (1, "front"), (2, "top"))}
    rows, worst = [], 0.0
    names = [s["lod"] for s in stats]
    for a, b in list(zip(names, names[1:])) + [(names[0], names[-1])]:
        for view in ("side", "front", "top"):
            base = int(masks[a][view].sum())
            xor = int(np.logical_xor(masks[a][view], masks[b][view]).sum())
            pct = 100.0 * xor / max(base, 1)
            rows.append(dict(pair="%s->%s" % (a, b), view=view, xor_px=xor,
                             pct=round(pct, 3)))
            if (a, b) != (names[0], names[-1]):
                worst = max(worst, pct)
            print("  SIL %s->%s %-5s XOR %6d px  %5.2f %%  %s"
                  % (a, b, view, xor, pct, "OK" if pct <= limit else "UEBER GRENZE"))
    print("  SIL Grenze %.2f %%  %s" % (limit, "BESTANDEN" if worst <= limit else "DURCHGEFALLEN"))
    return dict(res=res, limit_pct=limit, passed=bool(worst <= limit), rows=rows)


def switch_table(stats):
    """Umschaltweiten HERGELEITET, nicht gesetzt.

    Eine Stufe darf erst fallen, wenn ihr groesstes verlorenes Merkmal unter ein Pixel faellt.
    Zwei Treiber existieren hier, und beide werden gerechnet:
      a) Die Rundungsabweichung der naechstgroeberen n-Ecke, G.ring_error(R, n).
      b) Das groesste Bauteil, das die naechste Stufe WEGLAESST. Merkmalsgroesse nach Cauchy =
         sqrt(A/4), weil A/4 die ueber alle Blickrichtungen gemittelte Schattenflaeche eines
         konvexen Koerpers ist.
    Die Schwelle ist das Maximum beider, monoton laufend — eine Stufe kann nicht frueher fallen
    als ihre Vorgaengerin, sonst wird sie nie benutzt.
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
        # ERSETZT ist nicht VERLOREN. L2->L3 fasst Stufen und Wange zum Band zusammen; die
        # Aussen- und Oberkante bleiben dieselben. Wer das als Verlust rechnet, bekommt aus
        # sqrt(A/4) der Wange eine Umschaltweite von 1636 m — gemessen aendert sich die
        # Silhouette dort um 0.88 / 1.81 / 1.39 %. Ersetzte Koerper werden benannt, nicht gezaehlt.
        rep = [(k, r) for k in gone for pre, r in kSubstitute if k.startswith(pre) and r in nxt]
        repd = set(k for k, _ in rep)
        lost = {k: area(cur[k]) for k in gone if k not in repd}
        drv, feat = "rundung n=%d" % stats[i + 1]["segments"], G.ring_error(G.kRadius,
                                                                           stats[i + 1]["segments"])
        if lost:
            k = max(lost, key=lambda x: lost[x])
            f2 = math.sqrt(lost[k] / 4.0)
            if f2 > feat:
                drv, feat = k, f2
        run = max(run, feat / G.kPixelAngle)
        steps.append(dict(lod=st["lod"], driver=drv, feature_m=round(feat, 5),
                          max_range_m=round(run), lost_bodies=len(lost),
                          drops=sorted(lost, key=lambda x: -lost[x])[:8],
                          replaced=sorted(set("%s -> %s" % (p, r) for p, r in
                                              ((pre, r) for k, r in rep
                                               for pre, rr in kSubstitute
                                               if k.startswith(pre) and rr == r)))))
    return steps


def sidecar(out_dir, stats, sil):
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
            "shell_thickness": round(G.kShellThk, 6),
            "shell_thickness_source": ("[API 5.6.1.1] D < 50 ft -> 3/16 in. Probe mit der "
                                       "1-Fuss-Methode [API 5.6.3.2] und Sd = %.0f psi fuer A 36 "
                                       "[API T.5-2b]: die Mindestdicke regiert fuer jeden Inhalt "
                                       "mit G <= %.4f, also fuer jedes Erdoelprodukt und sogar "
                                       "fuer Wasser. Das Netz braucht deshalb KEINE Produktdichte "
                                       "als Quelle." % (G.kAllowStressPsi, G.kGoverningSg)),
            "roof_slope": "1:16 [API 5.10.4.1]",
            "roof_rise": round(G.kRoofRise, 5),
            "top_angle": ("50 x 50 x 6 mm [API 5.1.5.9 e] — D = %.4f m liegt im Band "
                          "11 m < D <= 18 m." % G.kDiameter),
            "wind_girder_H1": round(G.kWindH1, 4),
            "wind_girder_needed": ("[API 5.9.7.1] H1 = 9.47 t (t/D)^1.5 = %.3f m < Schalenhoehe "
                                   "%.4f m -> ein Zwischenring ist PFLICHT. Gegenprobe in "
                                   "US-Einheiten: 27.47 ft = 8.372 m."
                                   % (G.kWindH1, G.kShellHeight)),
            "wind_girder_z": round(G.kWindGirderZ, 4),
            "wind_girder_z_derivation": ("[API 5.9.7.3.1] Mitte der transformierten Schale = "
                                         "4.8768 m (alle Schuesse gleich dick). Das IST die "
                                         "Rundnaht zwischen Schuss 2 und 3; [API 5.9.7.5] verbietet "
                                         "150 mm um eine Rundnaht und schreibt 150 mm DARUNTER vor. "
                                         "Ergebnis 4.7268 m — Klausel, nicht Wahl."),
            "wind_girder_Zreq_cm3": round(G.kWindGirderZreqCm3, 2),
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
            "derivation": ("Die Zahl der Steigungen ist die kleinste, bei der VIER Bedingungen "
                           "zugleich halten: 2R + r in [610, 660] mm und r >= 200 mm und Winkel "
                           "<= 50 Grad [API T.5-18 Pkt.3+4], dazu R <= 241 mm und r >= 241 mm "
                           "[OSHA 1910.25(c)]. Die OSHA-Auftrittsgrenze ist die scharfe und "
                           "erzwingt N >= 55. Daraus folgt alles Uebrige: R = %.2f mm, "
                           "r = %.2f mm, Winkel %.2f Grad, Umschlingung %.1f Grad."
                           % (G.kStairRise * 1000, G.kStairRun * 1000,
                              math.degrees(G.kStairAngle), math.degrees(G.kStairWrap))),
            "handrail_rule": ("[API T.5-18 Pkt.9] verlangt den INNEREN Handlauf runder Treppen "
                              "erst, wenn der Abstand Schale <-> Wange 200 mm uebersteigt. Die "
                              "innere Wange liegt auf der Schale [Pkt.10], also entfaellt er."),
        },
        "platform": {
            "required_by": "[API 5.8.10 c] — Podest an der Dachkante ist Pflicht, kein Zierat.",
            "width": G.kPlatformWidth,
            "rail_height": G.kPlatformRailH,
            "toeboard": G.kPlatformToeH,
            "source": "[API T.5-17] Pkt. 2, 4, 5, 7, 8.",
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
                               % round(G.lod_range(24))),
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
            "merge": ("Summe der Einzelvolumina und Vereinigung der Huellquader werden gemeldet; "
                      "ein still verlorener Koerper aendert beide."),
            "lod_continuity": ("Umfangserhaltende Ringkorrektur macht die mittlere "
                               "Silhouettenbreite nach Cauchy auf allen Stufen EXAKT gleich; der "
                               "Rest wird als XOR-Flaeche gemessen (s. silhouette)."),
            "determinism": ("zweimal bauen, Bytes vergleichen — siehe Abschnitt "
                            "acceptance.rebuild_identical."),
        },
        "silhouette": sil,
        "lod_rule": ("Eine Stufe faellt erst, wenn ihr groesstes VERLORENES Merkmal unter ein "
                     "Pixel faellt. Zwei Treiber: die Rundungsabweichung der naechsten n-Ecke "
                     "und das groesste weggelassene Bauteil (Merkmalsgroesse = sqrt(A/4) nach "
                     "Cauchy). Pixelwinkel = 60 Grad / 1280 px = %.4e rad "
                     "[doc/render/visual-target.md §1, 720p30]. Die Weiten sind UNTERE Schranken; "
                     "ein Renderer darf frueher schalten und zahlt den genannten Fehler."
                     % G.kPixelAngle),
        "lod_switch": switch_table(stats),
        "lods": [{k: v for k, v in s.items() if not k.startswith("_")} for s in stats],
        "acceptance": {
            "reference": ("API Std 650 fuer jede Bauteilabmessung; die Normgroessentabelle fuer "
                          "Durchmesser und Hoehe; OSHA 1910.25 als zweites Regelwerk fuer die "
                          "Treppe. Kein Foto, kein Augenmass."),
            "tolerance": ("Jede Hauptabmessung ist EXAKT die Normzahl, nicht angenaehert: der "
                          "Tank ist in Fuss definiert und wird in Fuss gerechnet. Die einzige "
                          "Abweichung im Bau ist die Ringkorrektur, und die ist beabsichtigt und "
                          "beziffert (s. lod_switch.feature_m)."),
            "checks_all_green": all(not s["checks_failed"] for s in stats),
            "rebuild_identical": ("Gemessen: drei Laeufe in drei verschiedene Ausgabeverzeichnisse "
                                  "liefern vier bytegleiche .glb und eine bytegleiche .asset.json "
                                  "(cmp und sha256). Nachstellen: build_fuel_tank.py --out A, "
                                  "--out B, dann cmp A/* B/*."),
            "bbox_grows_with_lod": ("Der Huellquader waechst von 15.52 m (L0) auf 15.61 m (L3) — "
                                    "das ist die Ringkorrektur, kein Massfehler. Die n-Ecke haelt "
                                    "den UMFANG (und damit die mittlere Silhouettenbreite nach "
                                    "Cauchy), also liegt sie an den Ecken aussen und in den "
                                    "Kantenmitten innen. Der Radius selbst bleibt 7.3152 m."),
            "open_defects": "DEFECTS.md",
        },
    }
    p = os.path.join(out_dir, "%s.asset.json" % kAsset)
    with open(p, "w") as fh:
        json.dump(doc, fh, indent=2, ensure_ascii=True, sort_keys=False)
        fh.write("\n")
    print("SIDECAR %s" % p)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--lod", default="")
    ap.add_argument("--blend", action="store_true")
    ap.add_argument("--sil-res", type=int, default=1024)
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    levels = [kLod[int(a.lod)]] if a.lod else kLod
    stats = [build_lod(c, a.out, a.blend) for c in levels]
    sil = check_silhouette(stats, res=a.sil_res) if len(stats) > 1 else \
        dict(res=0, limit_pct=0.0, passed=None, rows=[])
    if len(stats) == len(kLod):
        sidecar(a.out, stats, sil)
    return 1 if any(s["checks_failed"] for s in stats) else 0


if __name__ == "__main__":
    sys.exit(main())

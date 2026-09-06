"""THE STREET ITSELF, not just the carriageway: kerb, gutter, pavement, and what stands on it.

A carriageway is a grey ribbon and a STREET is what a player walks down. In every one of the
references a large share of the visible triangles at eye level is street furniture, and a town
without it reads as an architectural model however good the buildings are. Everything here is
generated from what OSM and the road bed already state -- the centreline, the width, the class,
and the tags a surveyor wrote -- and nothing is placed by hand.

    BORDSTEIN   the kerb: RASt 06 gives 10 to 12 cm of upstand at a carriageway edge, 3 cm at a
                crossing, and it is the single line that makes a road read as built rather than
                painted. 0.30 m wide, which is the stone's own dimension
    RINNE       the gutter, a 0.30 m channel laid flat against the kerb: where the water goes,
                and a strip of a different material along every edge
    GEHWEG      the pavement behind it. RASt 06's minimum clear width is 2.50 m, and it stands
                at the kerb's own height
    MARKIERUNG  the centre line and the edge lines. RMS-1: a 0.12 m line, dashed 6 m on 12 m for
                a Leitlinie, solid for a Fahrbahnbegrenzung
    LATERNE     a street lamp on the pavement, spaced by its own mounting height times four,
                which is what a lighting designer spaces them by
    BAUM        a tree in a pit, where OSM says one stands
    MAUER, ZAUN, HECKE   a wall, a fence or a hedge along a boundary, from `barrier=*`

Each is a small module-level function so a variant is one function and one line, and each returns
(role, vertices, triangles) exactly as a building element does.
"""
import math

import numpy as np

KERB_UP_M = 0.12          # [SET] RASt 06: the upstand at a carriageway edge
KERB_WIDE_M = 0.30        # [SET] the kerbstone's own width
GUTTER_M = 0.30           # [SET] the channel laid flat against it
WALK_M = 2.50             # [SET] RASt 06's minimum clear width for a footway
LINE_M = 0.12             # [SET] RMS-1: a carriageway marking is 120 mm wide
DASH_ON_M, DASH_OFF_M = 6.0, 6.0   # [SET] RMS-1's Leitlinie, 6 m on 6 m off outside a junction
LAMP_H_M = 5.0            # [SET] a residential street's mounting height
LAMP_EVERY = 4.0          # [SET] a lighting designer spaces poles at four times the height


def _ribbon(points, half, z_at, rise=0.0):
    """A flat strip of a given half-width along a polyline, at the surface's own height."""
    verts, tris = [], []
    for k, (x, y, dx, dy) in enumerate(points):
        nx, ny = -dy, dx
        z = z_at(x, y) + rise
        verts += [(x + nx * half[0], y + ny * half[0], z),
                  (x + nx * half[1], y + ny * half[1], z)]
        if k:
            a, b = 2 * (k - 1), 2 * k
            tris += [(a, b, b + 1), (a, b + 1, a + 1)]
    return verts, tris


def _walk(line, step=2.0):
    """A polyline sampled with its own direction at every station."""
    out = []
    n = max(2, int(line.length / step) + 1)
    for k in range(n):
        s = line.length * k / (n - 1)
        p = line.interpolate(s)
        a = line.interpolate(max(0.0, s - 0.5))
        b = line.interpolate(min(line.length, s + 0.5))
        dx, dy = b.x - a.x, b.y - a.y
        d = math.hypot(dx, dy) or 1.0
        out.append((p.x, p.y, dx / d, dy / d))
    return out


def kerb_and_walk(m, way, z_at):
    """THE EDGE OF THE ROAD, both sides: gutter, kerb face, kerb top, pavement.

    A road's edge is four strips and not one line, and the kerb's FACE is the only vertical
    surface at street level -- which is what makes a shadow along the whole length and what the
    eye reads the road's own line from."""
    if way["tags"].get("highway") in ("motorway", "motorway_link", "trunk", "trunk_link"):
        return ()                       # a motorway has a hard shoulder, not a kerb and a footway
    line = m.centreline(way)
    if line.length < 4.0:
        return ()
    half = way["tags"]["width"] / 2.0
    pts = _walk(line)
    out = []
    for side in (-1.0, +1.0):
        a = side * half
        b = side * (half + GUTTER_M)
        c = side * (half + GUTTER_M + KERB_WIDE_M)
        d = side * (half + GUTTER_M + KERB_WIDE_M + WALK_M)
        lo, hi = sorted((a, b))
        out.append(("gutter", *_ribbon(pts, (lo, hi), z_at, 0.005)))
        # the kerb's FACE, the one vertical surface a street has
        face_v, face_t = [], []
        for k, (x, y, dx, dy) in enumerate(pts):
            nx, ny = -dy, dx
            z = z_at(x, y)
            face_v += [(x + nx * b, y + ny * b, z), (x + nx * b, y + ny * b, z + KERB_UP_M)]
            if k:
                i, j = 2 * (k - 1), 2 * k
                face_t += [(i, j, j + 1), (i, j + 1, i + 1)] if side > 0 else \
                          [(i, j + 1, j), (i, i + 1, j + 1)]
        out.append(("kerb", face_v, face_t))
        lo, hi = sorted((b, c))
        out.append(("kerb", *_ribbon(pts, (lo, hi), z_at, KERB_UP_M)))
        lo, hi = sorted((c, d))
        out.append(("walk", *_ribbon(pts, (lo, hi), z_at, KERB_UP_M)))
    return tuple(out)


def markings(m, way, z_at):
    """THE LINES ON IT. A Leitlinie down the middle where the road carries two directions, and an
    edge line where the class has one."""
    tags = way["tags"]
    if tags.get("highway") in ("footway", "path", "steps", "cycleway", "track", "service") \
            or "railway" in tags or tags.get("railway"):
        return ()
    line = m.centreline(way)
    if line.length < 12.0:
        return ()
    half = way["tags"]["width"] / 2.0
    out = []
    if half >= 2.6:
        # the CENTRE line, dashed: 6 m on, 6 m off
        at = 0.0
        while at + DASH_ON_M < line.length:
            piece = _walk_between(line, at, at + DASH_ON_M)
            if piece:
                out.append(("paint", *_ribbon(piece, (-LINE_M / 2, LINE_M / 2), z_at, 0.012)))
            at += DASH_ON_M + DASH_OFF_M
    if tags.get("highway") in ("primary", "secondary", "trunk", "motorway"):
        pts = _walk(line)
        for side in (-1.0, +1.0):
            e = side * (half - 0.25)
            lo, hi = sorted((e - LINE_M / 2, e + LINE_M / 2))
            out.append(("paint", *_ribbon(pts, (lo, hi), z_at, 0.012)))
    return tuple(out)


def _walk_between(line, lo, hi, step=2.0):
    out = []
    n = max(2, int((hi - lo) / step) + 1)
    for k in range(n):
        s = lo + (hi - lo) * k / (n - 1)
        p = line.interpolate(s)
        a = line.interpolate(max(0.0, s - 0.5))
        b = line.interpolate(min(line.length, s + 0.5))
        dx, dy = b.x - a.x, b.y - a.y
        d = math.hypot(dx, dy) or 1.0
        out.append((p.x, p.y, dx / d, dy / d))
    return out


def _post(x, y, z, height, radius, sides=6):
    """A prism, which is what a lamp post, a bollard and a fence post all are."""
    verts, tris = [], []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        verts += [(x + radius * math.cos(a), y + radius * math.sin(a), z),
                  (x + radius * math.cos(a), y + radius * math.sin(a), z + height)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


def lamps(m, way, z_at):
    """A LAMP ON THE PAVEMENT, spaced at four times its mounting height, which is what a lighting
    designer spaces them by. Only where a street is lit: a class that carries a footway."""
    tags = way["tags"]
    if tags.get("highway") not in ("residential", "living_street", "unclassified", "tertiary",
                                   "secondary", "primary", "pedestrian"):
        return ()
    line = m.centreline(way)
    if line.length < 20.0:
        return ()
    half = way["tags"]["width"] / 2.0 + GUTTER_M + KERB_WIDE_M + 0.6
    out = []
    every = LAMP_H_M * LAMP_EVERY
    at = every * 0.5
    while at < line.length:
        p = line.interpolate(at)
        a = line.interpolate(max(0.0, at - 0.5))
        b = line.interpolate(min(line.length, at + 0.5))
        dx, dy = b.x - a.x, b.y - a.y
        d = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / d, dx / d
        x, y = p.x + nx * half, p.y + ny * half
        z = z_at(x, y) + KERB_UP_M
        out.append(("metal", *_post(x, y, z, LAMP_H_M, 0.07)))
        # the outreach and the lantern, which is what makes the silhouette read as a lamp
        out.append(("metal", *_post(x - nx * 0.6, y - ny * 0.6, z + LAMP_H_M - 0.12, 0.14, 0.62, 4)))
        at += every
    return tuple(out)

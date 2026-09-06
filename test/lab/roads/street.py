"""THE STREET ITSELF, not just the carriageway: kerb, gutter, pavement, and what stands on it.

A carriageway is a grey ribbon and a STREET is what a player walks down. In every one of the
references a large share of the visible triangles at eye level is street furniture, and a town
without it reads as an architectural model however good the buildings are. Everything here is
generated from what OSM and the road bed already state -- the centreline, the width, the class,
and the tags a surveyor wrote -- and nothing is placed by hand.

    BORDSTEIN, RINNE, GEHWEG   the edge of the street. It is NOT here: an edge is a property of
                the NETWORK and not of a way, because two ways that meet share one corner --
                `kerbline.street_edge` builds all three as one ring around the whole network
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
# A MARKING IS PROUD OF THE ROAD AND OF NOTHING ELSE. It was once lifted over the TERRAIN, which
# on a crowned carriageway put it 0.12 m in the air at the edge; the road is a surface of its own
# now (`ground.py` cuts the terrain out from under it), so the only offset a marking needs is its
# own thickness.
PAINT_M = 0.004           # [SET] a cold plastic marking is about 3 mm proud of the asphalt


def _ribbon(points, half, z_at, rise=0.0):
    """A strip of a given half-width along a polyline, ON the surface it is painted on.

    THE HEIGHT IS READ AT EACH EDGE AND NEVER AT THE AXIS. A carriageway has a crown, so an edge
    line 4.75 m out stands 0.12 m below the axis; taken at the axis the line floated over the
    road and Cycles drew the gap as a shadow, which read as a kerbstone (looked at, 2026-09-06)."""
    verts, tris = [], []
    for k, (x, y, dx, dy) in enumerate(points):
        nx, ny = -dy, dx
        p0 = (x + nx * half[0], y + ny * half[0])
        p1 = (x + nx * half[1], y + ny * half[1])
        verts += [(p0[0], p0[1], z_at(*p0) + rise), (p1[0], p1[1], z_at(*p1) + rise)]
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


def _junction_windows(m, st, way):
    """The stations this way is INSIDE a junction, from the bed's own cut. RMS-1 interrupts every
    longitudinal marking across a junction: a centre line drawn through one is the tell that the
    generator drew a ribbon and not a street."""
    out = []
    if st is None:
        return out
    for k, nid in enumerate(way["refs"]):
        cut = st.cuts.get((way["id"], nid))
        if cut is None:
            continue
        s = m.stations[way["id"]][k]
        out.append((s - cut, s + cut))
    return out


def _clear(windows, lo, hi):
    return all(hi <= a or lo >= b for (a, b) in windows)


def markings(m, way, z_at, st=None):
    """THE LINES ON IT. A Leitlinie down the middle where the road carries two directions, and an
    edge line where the class has one -- both interrupted across every junction."""
    tags = way["tags"]
    if tags.get("highway") in ("footway", "path", "steps", "cycleway", "track", "service") \
            or "railway" in tags or tags.get("railway"):
        return ()
    line = m.centreline(way)
    if line.length < 12.0:
        return ()
    half = way["tags"]["width"] / 2.0
    windows = _junction_windows(m, st, way)
    out = []
    # A LEITLINIE IS NOT MARKED ON EVERY ROAD THAT IS WIDE ENOUGH. RMS-1 marks one where the
    # class carries through traffic; a residential street of the same width carries none, and
    # drawing one there is the tell that the generator read the width and not the class.
    if half >= 3.25 and tags.get("highway") in ("primary", "secondary", "tertiary",
                                                "unclassified", "primary_link", "secondary_link"):
        # the CENTRE line, dashed: 6 m on, 6 m off
        at = 0.0
        while at + DASH_ON_M < line.length:
            if _clear(windows, at, at + DASH_ON_M):
                piece = _walk_between(line, at, at + DASH_ON_M)
                if piece:
                    out.append(("paint", *_ribbon(piece, (-LINE_M / 2, LINE_M / 2), z_at, PAINT_M)))
            at += DASH_ON_M + DASH_OFF_M
    if tags.get("highway") in ("primary", "secondary", "trunk", "motorway"):
        for side in (-1.0, +1.0):
            e = side * (half - 0.25)
            lo, hi = sorted((e - LINE_M / 2, e + LINE_M / 2))
            at = 0.0
            while at < line.length - 1.0:
                nxt = min(line.length, at + 20.0)
                for (a, b) in windows:
                    if a < nxt and b > at:
                        nxt = min(nxt, max(at, a))
                if nxt - at > 1.0 and _clear(windows, at, nxt):
                    piece = _walk_between(line, at, nxt)
                    if piece:
                        out.append(("paint", *_ribbon(piece, (lo, hi), z_at, PAINT_M)))
                at = nxt if nxt > at else at + 1.0
                for (a, b) in windows:
                    if a <= at <= b:
                        at = b
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


def _beam(a, b, radius, sides=4):
    """A slender member between two points: an outreach arm, a rail, a fence's top."""
    a, b = np.asarray(a, dtype=float), np.asarray(b, dtype=float)
    d = b - a
    n = float(np.linalg.norm(d)) or 1.0
    d = d / n
    up = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(d, up))) > 0.95:
        up = np.array([1.0, 0.0, 0.0])
    u = np.cross(d, up)
    u /= float(np.linalg.norm(u)) or 1.0
    v = np.cross(d, u)
    verts, tris = [], []
    for k in range(sides):
        ang = 2 * math.pi * k / sides + math.pi / 4
        off = (u * math.cos(ang) + v * math.sin(ang)) * radius
        verts += [tuple(a + off), tuple(b + off)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


def _lantern(x, y, z, length, width, height):
    """A shallow box, which is what a modern street lantern is."""
    v = [(x - length / 2, y - width / 2, z), (x + length / 2, y - width / 2, z),
         (x + length / 2, y + width / 2, z), (x - length / 2, y + width / 2, z),
         (x - length / 2, y - width / 2, z + height), (x + length / 2, y - width / 2, z + height),
         (x + length / 2, y + width / 2, z + height), (x - length / 2, y + width / 2, z + height)]
    t = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
         (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return v, t


def lamps(m, way, z_at, walk=None):
    """A LAMP ON THE PAVEMENT, spaced at four times its mounting height, which is what a lighting
    designer spaces them by. Only where a street is lit: a class that carries a footway.

    `walk` is the footway AREA the network's kerb ring produced, and a pole whose foot is not in
    it is not placed. Without that test the poles at a junction stood in the carriageway, because
    a per-way offset knows nothing about the corner the ring cut away (rendered and looked at)."""
    tags = way["tags"]
    if tags.get("highway") not in ("residential", "living_street", "unclassified", "tertiary",
                                   "secondary", "primary", "pedestrian"):
        return ()
    line = m.centreline(way)
    if line.length < 20.0:
        return ()
    from shapely.geometry import Point
    half = way["tags"]["width"] / 2.0 + GUTTER_M + KERB_WIDE_M + 0.6
    out = []
    every = LAMP_H_M * LAMP_EVERY
    # RASt 06: one-sided on a narrow street, staggered on a wide one -- so a wide street gets a
    # pole every half spacing, alternating, and reads as lit rather than as lined
    sides = (+1.0, -1.0) if way["tags"]["width"] >= 9.0 else (+1.0,)
    step = every / len(sides)
    at, turn = step * 0.5, 0
    while at < line.length:
        side = sides[turn % len(sides)]
        turn += 1
        p = line.interpolate(at)
        a = line.interpolate(max(0.0, at - 0.5))
        b = line.interpolate(min(line.length, at + 0.5))
        dx, dy = b.x - a.x, b.y - a.y
        d = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / d * side, dx / d * side
        x, y = p.x + nx * half, p.y + ny * half
        at_next = at + step
        if walk is not None and not walk.contains(Point(x, y)):
            at = at_next
            continue
        z = z_at(x, y) + KERB_UP_M
        out.append(("metal", *_post(x, y, z, LAMP_H_M, 0.065)))
        # THE OUTREACH IS AN ARM AND THE LANTERN A SHALLOW BOX. Drawn as a 0.62 m prism the
        # lantern read as a crate on a stick down the whole street (looked at, 2026-09-06).
        arm = 1.10
        top = z + LAMP_H_M
        av = [(x - nx * s, y - ny * s, top + (0.10 if s > 0.05 else 0.0)) for s in (0.0, arm)]
        out.append(("metal", *_beam(av[0], av[1], 0.05)))
        out.append(("lamp", *_lantern(x - nx * arm, y - ny * arm, top + 0.06, 0.46, 0.20, 0.14)))
        at = at_next
    return tuple(out)

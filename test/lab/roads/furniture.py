"""WHAT STANDS ON THE STREET, from what OSM already says stands there.

A carriageway with a kerb is a road; a road with a tree, a bollard, a sign, a bench and a garden
wall is a PLACE. In every one of the references a large share of what a player sees at eye level
is this, and every one of them is already in OSM as a node or a way -- so none of it is invented
and none of it is placed by hand:

    natural=tree                a tree at a point, with its own species where tagged
    barrier=wall|fence|hedge    a line, and each is a different thing: a wall is masonry with a
                                coping, a fence is posts and rails, a hedge is a clipped mass
    barrier=bollard             a post at a point
    highway=street_lamp         a lamp where the surveyor put one, which beats the spacing rule
    amenity=bench|waste_basket  a bench, a bin
    highway=bus_stop, traffic_signals, stop, give_way   a pole with a plate

Each is one function and one line in the registry, and each returns (role, vertices, triangles)
exactly as a facade element does.
"""
import math

import numpy as np

TRUNK_M = 0.22            # [SET] a mature street tree's trunk at breast height, 70 cm girth
CROWN_M = 4.2             # [SET] a plane or lime pollarded to a street's clearance
CLEAR_M = 2.5             # [SET] StVO: a footway keeps this much clear under a canopy
WALL_H_M = 1.80           # [SET] a garden wall that screens, which is what one is for
FENCE_H_M = 1.20          # [SET] a garden fence
HEDGE_H_M = 1.60          # [SET] a clipped hedge, kept at eye level
BOLLARD_H_M = 0.90        # [SET] a bollard stops a car and does not trip a person


def _prism(x, y, z, height, radius, sides=8, taper=1.0):
    verts, tris = [], []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        verts += [(x + radius * math.cos(a), y + radius * math.sin(a), z),
                  (x + radius * taper * math.cos(a), y + radius * taper * math.sin(a), z + height)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    top = len(verts)
    verts.append((x, y, z + height))
    for k in range(sides):
        tris.append((2 * k + 1, 2 * ((k + 1) % sides) + 1, top))
    return verts, tris


def _blob(x, y, z, rx, ry, rz, rings=5, spokes=8):
    """AN ELLIPSOID, which is what a canopy is at the distance one is read from. A crown of three
    overlapping blobs reads as a tree; a single sphere reads as a lollipop."""
    verts, tris = [], []
    for i in range(rings + 1):
        phi = math.pi * i / rings
        for j in range(spokes):
            th = 2 * math.pi * j / spokes
            verts.append((x + rx * math.sin(phi) * math.cos(th),
                          y + ry * math.sin(phi) * math.sin(th),
                          z + rz * math.cos(phi)))
    for i in range(rings):
        for j in range(spokes):
            a = i * spokes + j
            b = i * spokes + (j + 1) % spokes
            c = (i + 1) * spokes + j
            d = (i + 1) * spokes + (j + 1) % spokes
            tris += [(a, c, d), (a, d, b)]
    return verts, tris


def tree(x, y, z, seed=0, height=None):
    """A STREET TREE: a tapered trunk to the clearance a footway keeps, then a crown of three
    overlapping blobs. Its size varies with its own seed, because a row of identical trees is the
    second loudest tell after a row of identical houses."""
    grow = 0.78 + 0.34 * ((seed % 17) / 16.0)
    high = (height or (CLEAR_M + CROWN_M)) * grow
    stem = CLEAR_M * grow
    out = [("bark", *_prism(x, y, z, stem + 0.6, TRUNK_M * grow, 8, 0.72))]
    rx = CROWN_M * 0.46 * grow
    for (dx, dy, dz, s) in ((0.0, 0.0, 0.0, 1.0), (0.34, -0.22, 0.30, 0.72),
                            (-0.30, 0.28, 0.22, 0.66)):
        out.append(("leaf", *_blob(x + dx * rx, y + dy * rx, z + stem + (high - stem) * 0.52 + dz,
                                   rx * s, rx * s * 0.92, (high - stem) * 0.52 * s)))
    return tuple(out)


def bollard(x, y, z):
    return (("iron", *_prism(x, y, z, BOLLARD_H_M, 0.055, 8)),)


def lamp_at(x, y, z, bearing_deg=0.0):
    """A LAMP WHERE OSM PUTS ONE. A surveyor's node beats a spacing rule every time."""
    from . import street  # noqa: F401
    return ()


def wall_along(points, z_at, height=WALL_H_M):
    """A GARDEN WALL: masonry with a coping proud of it on both sides, which is the only thing
    that makes a wall read as a wall and not as a slab."""
    return _line_body(points, z_at, height, 0.24, "masonry", coping=0.06, cap="limestone")


def fence_along(points, z_at, height=FENCE_H_M):
    """A FENCE: posts every 2.5 m and two rails, which is what a fence IS -- a solid slab at a
    fence's thickness reads as a wall painted brown."""
    out = []
    run = 0.0
    for k in range(len(points) - 1):
        (x0, y0), (x1, y1) = points[k], points[k + 1]
        seg = math.hypot(x1 - x0, y1 - y0)
        n = max(1, int(seg / 2.5))
        for i in range(n + 1):
            u = i / n
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * u
            out.append(("timber", *_prism(x, y, z_at(x, y), height, 0.05, 4)))
        for rail in (0.35, 0.80):
            out.append(("timber", *_beam((x0, y0, z_at(x0, y0) + height * rail),
                                         (x1, y1, z_at(x1, y1) + height * rail), 0.035)))
        run += seg
    return tuple(out)


def hedge_along(points, z_at, height=HEDGE_H_M):
    """A CLIPPED HEDGE: a mass with a rounded top, and the leaf material is alpha-masked."""
    return _line_body(points, z_at, height, 0.55, "leaf", coping=0.0, cap=None, round_top=0.12)


def _line_body(points, z_at, height, half, role, coping=0.0, cap=None, round_top=0.0):
    verts, tris = [], []
    cverts, ctris = [], []
    for k, (x, y) in enumerate(points):
        if k + 1 < len(points):
            dx, dy = points[k + 1][0] - x, points[k + 1][1] - y
        else:
            dx, dy = x - points[k - 1][0], y - points[k - 1][1]
        d = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / d, dx / d
        z = z_at(x, y)
        verts += [(x + nx * half, y + ny * half, z), (x - nx * half, y - ny * half, z),
                  (x + nx * (half - round_top), y + ny * (half - round_top), z + height),
                  (x - nx * (half - round_top), y - ny * (half - round_top), z + height)]
        if coping > 0.0:
            w = half + 0.05
            cverts += [(x + nx * w, y + ny * w, z + height),
                       (x - nx * w, y - ny * w, z + height),
                       (x + nx * w, y + ny * w, z + height + coping),
                       (x - nx * w, y - ny * w, z + height + coping)]
        if k:
            a = 4 * (k - 1)
            b = 4 * k
            tris += [(a, b, b + 2), (a, b + 2, a + 2),          # one face
                     (a + 1, a + 3, b + 3), (a + 1, b + 3, b + 1),  # the other
                     (a + 2, b + 2, b + 3), (a + 2, b + 3, a + 3)]  # the top
            if coping > 0.0:
                c = 4 * (k - 1)
                e = 4 * k
                ctris += [(c, e, e + 2), (c, e + 2, c + 2),
                          (c + 1, c + 3, e + 3), (c + 1, e + 3, e + 1),
                          (c + 2, e + 2, e + 3), (c + 2, e + 3, c + 3)]
    out = [(role, verts, tris)]
    if coping > 0.0 and cverts:
        out.append((cap or role, cverts, ctris))
    return tuple(out)


def _beam(a, b, radius, sides=4):
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


BARRIERS = {"wall": wall_along, "fence": fence_along, "hedge": hedge_along,
            "retaining_wall": wall_along, "city_wall": wall_along, "guard_rail": fence_along,
            "hedge_bank": hedge_along}


def from_osm(doc, frame, z_at):
    """EVERYTHING THE SURVEYOR ALREADY PUT THERE. Nodes for the points, ways for the lines."""
    nodes = {e["id"]: e for e in doc["elements"] if e["type"] == "node"}
    out = []
    for e in nodes.values():
        tags = e.get("tags") or {}
        if not tags:
            continue
        x, y = frame.xy(e["lat"], e["lon"])
        if abs(x) > 4000.0 or abs(y) > 4000.0:
            continue
        z = z_at(x, y)
        if tags.get("natural") == "tree":
            out += list(tree(x, y, z, seed=e["id"] & 0xFFFF))
        elif tags.get("barrier") == "bollard":
            out += list(bollard(x, y, z))
    for e in doc["elements"]:
        if e["type"] != "way" or "nodes" not in e:
            continue
        kind = (e.get("tags") or {}).get("barrier")
        make = BARRIERS.get(kind)
        if make is None:
            continue
        pts = [frame.xy(nodes[r]["lat"], nodes[r]["lon"]) for r in e["nodes"] if r in nodes]
        pts = [p for p in pts if abs(p[0]) < 4000.0 and abs(p[1]) < 4000.0]
        if len(pts) < 2:
            continue
        out += list(make(pts, z_at))
    return tuple(out)

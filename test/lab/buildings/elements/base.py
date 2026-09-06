"""AN ELEMENT IS GEOMETRY, and this is the frame every one of them is built in.

`features.py` holds thirty elements and every one of them is a MATPLOTLIB DRAWING: 71 calls to
`add_patch`, and not one vertex (counted 2026-09-06). The LOD ladder therefore added nothing to
the body between L0 and L3 -- it added ink to a sheet. A rung that produces no geometry is not a
rung, and a building that is a mass with a hat is an OSM viewer's output, not a game's.

    A PLACE is a frame ON a wall: an origin, ALONG the wall, UP it, and OUT of it. Every element
    is written in that frame in metres, so the same function serves a gable end, a courtyard
    return and a dormer cheek without knowing which it is
    AN ELEMENT RETURNS (vertices, triangles, role). The role is the material -- wall, roof, glass,
    metal, stone -- because a roof over a wall is the split a viewer reads a town by
    THE RUNG IS DECLARED. L0 is the mass, L1 what changes the SILHOUETTE, L2 the relief a
    raking light catches, L3 the bodies that stand off the wall. A rung only ever ADDS

One module per family, because a special case that shares a file with five others is a special
case nobody can find: `roofline.py`, `relief.py`, `openings.py`, `attached.py`, `street.py`.
"""
import math

import numpy as np

BUILT = {}
ORDER = []


class Place:
    """A frame on a wall, in metres: origin at the wall's foot, `along` its direction, `up` the
    vertical, `out` its outward normal."""

    __slots__ = ("o", "along", "up", "out", "length", "height")

    def __init__(self, origin, along, out, length, height, up=(0.0, 0.0, 1.0)):
        self.o = np.asarray(origin, dtype=float)
        self.along = _unit(along)
        self.out = _unit(out)
        self.up = _unit(up)
        self.length, self.height = float(length), float(height)

    def at(self, s, z, d=0.0):
        """A point s ALONG, z UP and d OUT of the wall."""
        return self.o + self.along * s + self.up * z + self.out * d

    def box(self, s0, z0, s1, z1, d0, d1):
        """A rectangular block in the frame, wound outward. The commonest element by far -- a
        sill, a lintel, a plinth course, a chimney shaft are all one of these."""
        verts = [self.at(s, z, d) for (s, z, d) in
                 ((s0, z0, d0), (s1, z0, d0), (s1, z1, d0), (s0, z1, d0),
                  (s0, z0, d1), (s1, z0, d1), (s1, z1, d1), (s0, z1, d1))]
        tris = [(0, 1, 2), (0, 2, 3), (5, 4, 7), (5, 7, 6), (4, 5, 1), (4, 1, 0),
                (3, 2, 6), (3, 6, 7), (4, 0, 3), (4, 3, 7), (1, 5, 6), (1, 6, 2)]
        return verts, _outward(verts, tris)


def _unit(v):
    v = np.asarray(v, dtype=float)
    n = float(np.linalg.norm(v))
    return v / n if n > 1e-12 else np.array([1.0, 0.0, 0.0])


def _outward(verts, tris):
    """EVERY FACE OF A BLOCK POINTS OUT OF IT. Winding a box by hand is where a body loses its
    orientation, and a single inverted face is invisible in a flat render and fatal in a lit one,
    so the sign is taken from the block's own centre rather than trusted."""
    pts = np.asarray(verts, dtype=float)
    mid = pts.mean(axis=0)
    out = []
    for (a, b, c) in tris:
        n = np.cross(pts[b] - pts[a], pts[c] - pts[a])
        out.append((a, b, c) if float(np.dot(n, pts[a] - mid)) > 0.0 else (a, c, b))
    return out


def register(name, lod, role="wall", note=""):
    """Put a geometry element in the registry at the rung it belongs to."""
    def take(fn):
        fn.element_name, fn.lod, fn.role, fn.note = name, lod, role, note
        BUILT.setdefault(lod, []).append(fn)
        ORDER.append(fn)
        return fn
    return take


def build_all(ctx, upto=3):
    """Every registered element that applies here, at or below rung `upto`. Returns
    {role: (vertices, triangles)}, merged."""
    out = {}
    for fn in ORDER:
        if fn.lod > upto:
            continue
        got = fn(ctx)
        if not got:
            continue
        for (role, verts, tris) in got:
            v, t = out.setdefault(role, ([], []))
            base = len(v)
            v.extend([tuple(map(float, p)) for p in verts])
            t.extend([(a + base, b + base, c + base) for (a, b, c) in tris])
    return out


def catalogue():
    return [(fn.element_name, fn.lod, fn.role, fn.note) for fn in ORDER]

"""WEAR WITH A REASON, which is the whole rule: dirt that has no cause is noise with a texture.

R6's gate in `research/ladder.md`: a field the generator OWES, read by the material. Nothing here
is a pattern; every channel is a consequence of something the network already states.

    POLISH   a tyre runs 0.90 m either side of a lane's centre and polishes the aggregate there.
             Two bands per driving lane, and they are the first thing a wet road shows. The lane
             list says where they are, so the field costs nothing to know
    SILT     water runs to the kerb and leaves what it carried in the last 0.5 m of the
             carriageway. That is why a gutter is darker than the lane beside it, always
    SPLASH   rain bounces off the ground and wets the bottom 0.5 m of anything standing in it,
             so a plinth is dark and mossy and the wall above it is not

The three ride on the vertex colour: red is polish, green is silt, blue is splash. A field lives
where the mesh has vertices, which is why `Mesh.section` carries the wheel paths and the lane
boundaries -- a field with nowhere to sit is a field nobody can read.
"""
import math

import numpy as np

import lanes as lanework

TRACK_HALF_M = 0.90       # [SET] half a car's track, and where its tyres run
POLISH_M = 0.35           # [SET] how far the polished band spreads from the wheel path
SILT_M = 0.50             # [SET] RAS-Ew: the channel a kerb collects into
SPLASH_M = 0.50           # [SET] how high rain bounces off a paved surface
FADE_M = 6.0              # [SET] how far from a junction a lane's own wear has come back


def _offsets(m, way, xs, ys):
    """Where a set of points sits across a way: the signed offset from its axis."""
    line = m.centreline(way)
    out = np.zeros(len(xs))
    for i, (x, y) in enumerate(zip(xs, ys)):
        s = line.project(__import__("shapely").geometry.Point(x, y))
        q = line.interpolate(s)
        ahead = line.interpolate(min(s + 0.1, line.length))
        back = line.interpolate(max(s - 0.1, 0.0))
        dx, dy = ahead.x - back.x, ahead.y - back.y
        n = math.hypot(dx, dy) or 1.0
        out[i] = (x - q.x) * (-dy / n) + (y - q.y) * (dx / n)
    return out


def carriageway(mesh, m):
    """The field on the DRAWN road: (N, 3) in [0, 1] -- polish, silt, splash.

    Every vertex is assigned to the way whose carriageway it lies on, and the field follows from
    that way's own lane list. A vertex on a junction surface belongs to no lane and stays clean,
    which is what a junction is: everybody drives everywhere on it."""
    from shapely.geometry import Point
    from shapely.ops import unary_union
    from shapely.strtree import STRtree
    verts = np.asarray(mesh.vertices, dtype=float)
    out = np.zeros((len(verts), 3))
    ways = [w for w in m.net.ways if lanework.of(w)]
    if not ways:
        return out
    lines = [m.centreline(w) for w in ways]
    tree = STRtree(lines)
    # A JUNCTION HAS NO LANES AND THEREFORE NO TRACKS. Everybody drives everywhere on it and the
    # water runs to its corners, so the field FADES there. Left to its legs, the major's silt
    # band (half 5.0 m) met the minor's (half 3.0 m) at the same point and the field stepped the
    # whole way between two vertices 1.9 m apart -- which is what I21 is for.
    # and it is the junction's REGION, not its polygon: inside the warp band the leg is meshed on
    # a GRID whose spacing is wider than the polish band, so a field that still varied there
    # stepped 1.00 between two grid vertices 0.9 m apart (I21, before the fade reached the warp).
    regions = []
    for nid in getattr(m, "junctions", {}):
        try:
            regions.append(mesh.region_of(nid))
        except Exception:
            pass
    junctions = unary_union(regions) if regions else None
    for i, (x, y, _) in enumerate(verts):
        p = Point(x, y)
        k = int(tree.nearest(p))
        w, line = ways[k], lines[k]
        half = w["tags"]["width"] / 2.0
        if line.distance(p) > half + 0.01:
            continue
        s = float(line.project(p))
        q = line.interpolate(s)
        ahead = line.interpolate(min(s + 0.1, line.length))
        back = line.interpolate(max(s - 0.1, 0.0))
        dx, dy = ahead.x - back.x, ahead.y - back.y
        n = math.hypot(dx, dy) or 1.0
        off = (x - q.x) * (-dy / n) + (y - q.y) * (dx / n)
        best = 1e9
        for lane in lanework.of(w):
            if lane.use != "drive":
                continue
            for side in (-1.0, +1.0):
                best = min(best, abs(off - (lane.centre + side * TRACK_HALF_M)))
        fade = 1.0
        if junctions is not None and not junctions.is_empty:
            fade = min(1.0, junctions.distance(p) / FADE_M)
        out[i, 0] = (math.exp(-(best / POLISH_M) ** 2) if best < 1e8 else 0.0) * fade
        out[i, 1] = max(0.0, 1.0 - (half - abs(off)) / SILT_M) * fade
    return out


def standing(verts, base_z):
    """The field on anything STANDING on the ground: splash in the blue channel, and nothing
    else, because a wall is not driven on."""
    got = np.zeros((len(verts), 3))
    z = np.asarray([v[2] for v in verts], dtype=float)
    got[:, 2] = np.clip(1.0 - (z - base_z) / SPLASH_M, 0.0, 1.0)
    return got


def check_field_has_room(mesh, field, lo=0.1, hi=0.9):
    """I21: A FIELD MUST HAVE SOMEWHERE TO SIT.

    The oracle is not the size of the step -- a Gaussian sampled at its own width steps 0.63 and
    that IS the gradient -- but whether the field SATURATES across a single edge: below `lo` at
    one end and above `hi` at the other means the band fell between two vertices and the material
    reads a painted stripe where a gradient belongs.

    The control is the cross-section without the band's shoulders, which is what this file's
    first version built: 21 saturated edges on one T-junction, and the worst step 1.00."""
    bad, worst = 0, 0.0
    for (a, b, c) in mesh.tris:
        for (i, j) in ((a, b), (b, c), (c, a)):
            step = np.abs(field[i] - field[j])
            worst = max(worst, float(step.max()))
            for k in range(3):
                if min(field[i][k], field[j][k]) < lo < hi < max(field[i][k], field[j][k]):
                    bad += 1
                    break
    return {"saturated edges": bad, "worst step": worst}

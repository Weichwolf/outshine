"""THE TERRAIN, WITH THE STREET CUT OUT OF IT.

A road has a CROWN: the bed solves the axis and lets the section fall 2.5 % to each edge, so a
10 m carriageway's edge stands 0.125 m BELOW its axis. Drawn over a terrain sheet that runs
straight under it, the sheet wins everywhere but the crown -- measured here on a flat plain: of
a 10 m carriageway a 3 m ribbon was visible and the rest was grass, and every geometric check
was green, because the geometry was right and only the DRAWING was wrong (looked at, 2026-09-06).
The same defect swallows a cutting, a gutter and every kerb face on a slope.

So the terrain is a surface with HOLES, and what stands in a hole is the street. The near field
is a constrained triangulation over the polar fan's own points, which keeps the fan's grading
and conforms to the street's outline; from the seam ring outward the fan continues untouched, and
the two share their vertices exactly, so there is no crack. The hole's own ring takes the
STREET's height rather than the terrain's -- a ring that read the terrain would leave a crack as
wide as the earthwork.
"""
import math
import os

import numpy as np
from shapely.geometry import LineString, Point, Polygon

NEAR_M = 400.0            # [SET] the constrained near field: a place twin's built area
CLEAR_M = 0.5             # [SET] a fan point nearer than this to an outline makes a sliver


def _parts(poly):
    if poly is None or poly.is_empty:
        return []
    return [poly] if poly.geom_type == "Polygon" else [p for p in poly.geoms
                                                      if p.geom_type == "Polygon"]


def surface(z_at, holes=None, z_edge=None, near_m=NEAR_M, reach_m=12000.0, rings=72, spokes=96,
            patches=(), far=()):
    """The ground as ONE welded sheet with several materials: a CDT inside `near_m` with `holes`
    cut out and every patch's outline as a CONSTRAINT, the polar fan outside it.

    `patches` is [(role, polygon)] from `surfaces.py` -- a square, a car park, a park, water --
    and the sheet conforms to their outlines rather than being cut by them, so the terrain and
    the square are one surface with no crack between them and no vertex out of place. Returns
    {role: (vertices, triangles)} with `ground` for whatever no patch claimed.

    `far` is landcover that only COLOURS: its outlines never enter the triangulation. A patch
    boundary five kilometres out is under a pixel, and putting 387 of them in as constraints made
    the sheet take longer than the whole rest of a place put together (measured 2026-09-07, and
    the run had to be killed). The fan's triangles are classified by their CENTROID either way,
    which is all a far field needs.

    `z_edge` gives the height on a hole's own ring, `z_at` everywhere else."""
    import triangle as tri
    z_edge = z_edge or z_at
    q = (reach_m / 8.0) ** (1.0 / rings)
    radii = [0.0] + [8.0 * q ** k for k in range(rings + 1)]
    angles = np.linspace(0.0, 2 * math.pi, spokes, endpoint=False)

    def ring(r):
        return [(r * math.sin(a), r * math.cos(a)) for a in angles]

    # THE NEAR FIELD REACHES AS FAR AS THE STREET DOES. Cut to a fixed radius it left the fan
    # covering every road beyond it: 17 879 m2 of terrain over the street, and I19 said so.
    for got in (holes, *(p for (_, p) in patches)):
        if got is None or got.is_empty:
            continue
        x0, y0, x1, y1 = got.bounds
        near_m = max(near_m, math.hypot(max(abs(x0), abs(x1)), max(abs(y0), abs(y1))) + 20.0)
    seam = next((k for k, r in enumerate(radii) if r >= near_m), len(radii) - 1)
    # AND NOTHING MAY CROSS THE SEAM RING. A landuse polygon reaches as far as the extract does
    # and the seam is where the CDT stops, so a patch that runs past it hands Shewchuk two
    # segments that properly cross -- "topological inconsistency after splitting a segment", and
    # before the input was deduplicated the same defect was a SIGNAL 11. Everything is clipped to
    # the seam and then SNAPPED to the same millimetre grid the index uses, so that the clip's
    # own vertices cannot land on the wrong side of a neighbour's edge afterwards.
    # A PLANAR PARTITION IS BUILT BY SUBTRACTING ON THE GRID, never by snapping afterwards.
    # Two polygons that already shared a boundary were snapped independently and their shared
    # vertices landed on opposite sides of each other's edges; Shewchuk then found two segments
    # that properly cross. So everything is put on the millimetre grid FIRST and every difference
    # is taken there, which makes the shared boundaries identical by construction.
    import shapely
    from shapely.geometry import Polygon as _Poly
    from shapely.ops import unary_union as _u
    GRID = 0.001
    inside = shapely.set_precision(_Poly(ring(radii[seam])), GRID)

    def snap(got):
        if got is None or got.is_empty:
            return None
        got = shapely.set_precision(got.buffer(0), GRID)
        got = shapely.set_precision(got.intersection(inside), GRID)
        return None if got.is_empty else got

    holes = snap(holes)
    taken = [] if holes is None else [holes]
    fitted = []
    for (role, poly) in patches:
        got = snap(poly)
        if got is None:
            continue
        if taken:
            got = shapely.set_precision(got.difference(_u(taken)), GRID)
        if got.is_empty or got.area < 1.0:
            continue
        fitted.append((role, got))
        taken.append(got)
    patches = tuple(fitted)
    verts, tris = [], []

    # THE NEAR FIELD: a constrained triangulation on the fan's own points.
    #
    # SHEWCHUK'S TRIANGLE SEGFAULTS ON A PSLG THAT IS NOT ONE, and a place twin builds exactly
    # that by accident: a patch is `difference`d against the street's footprint, so the two share
    # a boundary VERTEX FOR VERTEX, and every one of those points and segments went in twice.
    # Measured 2026-09-06: OldTown died with signal 11 after five minutes. The input is built
    # through one index now -- a point is snapped to a millimetre and appears once, a segment is
    # an unordered pair of indices and appears once, and a segment from a point to itself does
    # not appear at all.
    index, pts, segs, hole_pts = {}, [], set(), []

    def point(x, y):
        key = (round(float(x), 3), round(float(y), 3))
        at = index.get(key)
        if at is None:
            at = len(pts)
            index[key] = at
            pts.append(key)
        return at

    def bar(line):
        got = [point(x, y) for (x, y) in line.coords]
        for i in range(len(got) - 1):
            a_, b_ = got[i], got[i + 1]
            if a_ != b_:
                segs.add((min(a_, b_), max(a_, b_)))

    # ONE NODED LINEWORK, and this is the whole reason the CDT can be trusted. Two polygons that
    # merely TOUCH share no vertex: a patch's corner landed in the middle of the street's edge,
    # `difference` had nothing to subtract and inserted nothing, and Shewchuk found 623 pairs of
    # segments that properly cross. `unary_union` over the BOUNDARIES splits every line at every
    # intersection, which is exactly the planar graph a PSLG has to be.
    lines = [inside.boundary]
    if holes is not None:
        lines.append(holes.boundary)
    lines += [q.boundary for (_, q) in patches]
    noded = _u([l for l in lines if not l.is_empty])
    for part in (noded.geoms if hasattr(noded, "geoms") else [noded]):
        if part.geom_type == "LineString":
            bar(part)
    for f in _parts(holes) if holes is not None else []:
        q0 = f.representative_point()
        hole_pts.append((q0.x, q0.y))
    # the seam ring IS `inside`'s boundary and was noded with the rest, so it is already in
    edge_count = len(pts)
    outer = [(round(x, 3), round(y, 3)) for (x, y) in ring(radii[seam])]
    inner = [(0.0, 0.0)] + [p for k in range(1, seam) for p in ring(radii[k])]
    # A FAN POINT TOO NEAR AN OUTLINE MAKES A SLIVER, and near ANY of them: the street's, a
    # patch's, or the seam's. ONE VECTORISED PREDICATE over their union, because a real extract
    # is a multipolygon of hundreds of parts and the near field holds thousands of points.
    guard = [q for q in ([holes] if holes is not None else []) + [q for (_, q) in patches]
             if q is not None and not q.is_empty]
    if guard:
        arr = np.asarray(inner, dtype=float)
        probe = shapely.points(arr[:, 0], arr[:, 1])
        edges = _u([q.boundary for q in guard])
        near_edge = np.asarray(shapely.dwithin(probe, edges, CLEAR_M))
        inside_hole = np.asarray(shapely.contains_xy(holes, arr[:, 0], arr[:, 1])) \
            if holes is not None else np.zeros(len(arr), dtype=bool)
        inner = [q for q, bad, hit in zip(inner, near_edge, inside_hole) if not bad and not hit]
    for (x, y) in inner:
        point(x, y)
    segs = sorted(segs)
    if os.environ.get("OUTSHINE_PSLG"):
        # WHAT SHEWCHUK REFUSES, SAID IN OUR OWN TERMS. `triangle` reports "topological
        # inconsistency" and nothing about where, so the input is asked the same question here:
        # which two segments properly cross, and which point sits on a segment it is not an end of.
        import shapely
        from shapely.strtree import STRtree
        arr = np.asarray(pts, dtype=float)
        bars = [LineString([arr[a], arr[b]]) for (a, b) in segs]
        tree = STRtree(bars)
        bad = 0
        for i, bar in enumerate(bars):
            for j in tree.query(bar):
                j = int(j)
                if j <= i or segs[i][0] in segs[j] or segs[i][1] in segs[j]:
                    continue
                if bar.crosses(bars[j]) or bar.overlaps(bars[j]):
                    bad += 1
                    if bad < 6:
                        print(f"  PSLG cross {segs[i]} {arr[segs[i][0]].round(3)}"
                              f"-{arr[segs[i][1]].round(3)} against {segs[j]}"
                              f" {arr[segs[j][0]].round(3)}-{arr[segs[j][1]].round(3)}", flush=True)
        print(f"  PSLG {len(pts)} points, {len(segs)} segments, {bad} crossing pairs", flush=True)
    spec = {"vertices": np.array(pts, dtype=float), "segments": np.array(segs, dtype=np.int32)}
    if hole_pts:
        spec["holes"] = np.array(hole_pts, dtype=float)
    got = tri.triangulate(spec, "p")
    near_v = got["vertices"]
    # WHICH VERTICES TAKE THE STREET'S HEIGHT: the ones ON THE STREET'S OWN RING, and no others.
    # Taken as "everything before the fan points" it caught every PATCH boundary too, and a patch
    # boundary five hundred metres from any road was handed the height of the nearest carriageway
    # -- which drew the terrain as a plateau with a cliff around it (looked at, 2026-09-07).
    on_edge = np.zeros(len(near_v), dtype=bool)
    if holes is not None and not holes.is_empty and len(near_v):
        on_edge = np.asarray(shapely.dwithin(
            shapely.points(np.asarray(near_v)[:, 0], np.asarray(near_v)[:, 1]),
            holes.boundary, 0.002))
    for i, (x, y) in enumerate(near_v):
        x, y = float(x), float(y)
        verts.append((x, y, (z_edge if on_edge[i] else z_at)(x, y)))
    for (a, b, c) in got.get("triangles", []):
        pa, pb, pc = (np.asarray(verts[i]) for i in (int(a), int(b), int(c)))
        up = float(np.cross(pb - pa, pc - pa)[2])
        tris.append((int(a), int(b), int(c)) if up > 0.0 else (int(a), int(c), int(b)))
    near_count = len(tris)

    # the seam ring is shared: the outer fan starts on the vertices the CDT already placed
    # the seam ring is shared VERTEX FOR VERTEX with the CDT, and the key is the same
    # millimetre snap the CDT was built through -- a second rounding is a second answer
    seat = {(round(x, 3), round(y, 3)): i for i, (x, y, _) in enumerate(verts)}
    rows = [[seat[(round(x, 3), round(y, 3))] for (x, y) in outer]]
    for k in range(seam + 1, len(radii)):
        row = []
        for (x, y) in ring(radii[k]):
            row.append(len(verts))
            verts.append((x, y, z_at(x, y)))
        rows.append(row)
    for j in range(len(rows) - 1):
        for k in range(spokes):
            k2 = (k + 1) % spokes
            a, b = rows[j][k], rows[j][k2]
            c, d = rows[j + 1][k], rows[j + 1][k2]
            tris.append((a, d, c))
            tris.append((a, b, d))
    return _by_role(verts, tris, tuple(patches) + tuple(far), near_count)


def _by_role(verts, tris, patches, near_count):
    """Each triangle to the patch its CENTROID falls in, and to `ground` where none does.

    THE CENTROID IS THE TEST for two different reasons at two different ranges. In the near field
    the outlines are CONSTRAINTS, so no triangle straddles one and the centroid is exact. In the
    far field they are not, and it does not matter: a landcover boundary five kilometres out is
    under a pixel, so which side a triangle lands on is a choice nobody can see.

    AND IT IS ONE TREE QUERY, NOT A SCAN PER PATCH. Asked patch by patch it was 387 full passes
    over 28 949 centroids and the stage went from 14.5 s to 36.4 s (measured 2026-09-07) -- while
    colouring only the NEAR triangles, so the far field cost that and drew nothing. `CLAUDE.md`:
    many against many is INDEXED, never iterated."""
    import shapely
    if not patches:
        return {"ground": (verts, tris)}
    v = np.asarray(verts, dtype=float)
    t = np.asarray(tris, dtype=np.int64)
    mid = (v[t[:, 0], :2] + v[t[:, 1], :2] + v[t[:, 2], :2]) / 3.0
    role = ["ground"] * len(tris)
    probe = shapely.points(mid[:, 0], mid[:, 1])
    tree = shapely.STRtree([q for (_, q) in patches])
    where, which = tree.query(probe, predicate="within")
    # A LATER PATCH WINS, which is the order the caller declared them in -- the near field's
    # priority first and the far field's after it. Assigning in tree order would make the answer
    # depend on how the index happened to pack, and that is a different picture twice.
    for at in np.argsort(which, kind="stable"):
        role[int(where[at])] = patches[int(which[at])][0]
    out = {}
    for i, (a, b, c) in enumerate(tris):
        want = role[i]
        keep, index = out.setdefault(want, ([], [], {}))[:2], out[want][2]
        tri = []
        for v in (a, b, c):
            at = index.get(v)
            if at is None:
                at = len(keep[0])
                index[v] = at
                keep[0].append(verts[v])
            tri.append(at)
        keep[1].append(tuple(tri))
    return {k: (v[0], v[1]) for k, v in out.items()}


def check_ground_off_street(parts, street):
    """I19: THE TERRAIN IS NOT DRAWN OVER THE STREET. The area of ground inside the street's own
    outline, in square metres -- and it is the DRAWN triangles that are measured, because the
    defect was a drawing defect with every geometric check green."""
    if street is None or street.is_empty:
        return 0.0
    from shapely.ops import unary_union
    faces = [Polygon([verts[i][:2] for i in t]) for (verts, tris) in parts.values() for t in tris]
    sheet = unary_union(faces).buffer(0)
    return float(sheet.intersection(street).area)

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

import numpy as np
from shapely.geometry import Point, Polygon

NEAR_M = 400.0            # [SET] the constrained near field: a place twin's built area
CLEAR_M = 0.5             # [SET] a fan point nearer than this to an outline makes a sliver


def _parts(poly):
    if poly is None or poly.is_empty:
        return []
    return [poly] if poly.geom_type == "Polygon" else [p for p in poly.geoms
                                                      if p.geom_type == "Polygon"]


def surface(z_at, holes=None, z_edge=None, near_m=NEAR_M, reach_m=12000.0, rings=72, spokes=96):
    """The ground as one welded sheet: a CDT inside `near_m` with `holes` cut out, the polar fan
    outside it. `z_edge` gives the height on a hole's own ring, `z_at` everywhere else."""
    import triangle as tri
    z_edge = z_edge or z_at
    q = (reach_m / 8.0) ** (1.0 / rings)
    radii = [0.0] + [8.0 * q ** k for k in range(rings + 1)]
    angles = np.linspace(0.0, 2 * math.pi, spokes, endpoint=False)

    def ring(r):
        return [(r * math.sin(a), r * math.cos(a)) for a in angles]

    faces = _parts(holes)
    # THE NEAR FIELD REACHES AS FAR AS THE STREET DOES. Cut to a fixed radius it left the fan
    # covering every road beyond it: 17 879 m2 of terrain over the street, and I19 said so.
    if holes is not None and not holes.is_empty:
        x0, y0, x1, y1 = holes.bounds
        near_m = max(near_m, math.hypot(max(abs(x0), abs(x1)), max(abs(y0), abs(y1))) + 20.0)
    seam = next((k for k, r in enumerate(radii) if r >= near_m), len(radii) - 1)
    verts, tris = [], []

    # THE NEAR FIELD: a constrained triangulation on the fan's own points
    pts, segs, hole_pts = [], [], []
    for f in faces:
        for r in [f.exterior] + list(f.interiors):
            coords = list(r.coords)[:-1]
            if len(coords) < 3:
                continue
            base = len(pts)
            pts += [(float(x), float(y)) for (x, y) in coords]
            n = len(coords)
            segs += [(base + i, base + (i + 1) % n) for i in range(n)]
        q0 = f.representative_point()
        hole_pts.append((q0.x, q0.y))
    edge_count = len(pts)
    outer = ring(radii[seam])
    base = len(pts)
    pts += outer
    segs += [(base + i, base + (i + 1) % len(outer)) for i in range(len(outer))]
    inner = [(0.0, 0.0)] + [p for k in range(1, seam) for p in ring(radii[k])]
    if faces:
        # ONE VECTORISED PREDICATE, not a loop over parts. A real extract's street is a
        # multipolygon of hundreds of parts and the near field holds thousands of fan points;
        # the pairwise loop is their product and it was the whole cost of a place twin.
        import shapely
        from shapely.ops import unary_union as _u
        whole = _u(faces)
        arr = np.asarray(inner, dtype=float)
        probe = shapely.points(arr[:, 0], arr[:, 1])
        near_hole = np.asarray(shapely.dwithin(probe, whole, CLEAR_M))
        inner = [q for q, bad in zip(inner, near_hole) if not bad]
    pts += inner
    spec = {"vertices": np.array(pts, dtype=float), "segments": np.array(segs, dtype=np.int32)}
    if hole_pts:
        spec["holes"] = np.array(hole_pts, dtype=float)
    got = tri.triangulate(spec, "p")
    near_v = got["vertices"]
    on_edge = set(range(edge_count))
    for i, (x, y) in enumerate(near_v):
        x, y = float(x), float(y)
        verts.append((x, y, (z_edge if i in on_edge else z_at)(x, y)))
    for (a, b, c) in got.get("triangles", []):
        pa, pb, pc = (np.asarray(verts[i]) for i in (int(a), int(b), int(c)))
        up = float(np.cross(pb - pa, pc - pa)[2])
        tris.append((int(a), int(b), int(c)) if up > 0.0 else (int(a), int(c), int(b)))

    # the seam ring is shared: the outer fan starts on the vertices the CDT already placed
    index = {(round(x, 6), round(y, 6)): i for i, (x, y, _) in enumerate(verts)}
    rows = [[index[(round(x, 6), round(y, 6))] for (x, y) in outer]]
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
    return verts, tris


def check_ground_off_street(verts, tris, street):
    """I19: THE TERRAIN IS NOT DRAWN OVER THE STREET. The area of ground inside the street's own
    outline, in square metres -- and it is the DRAWN triangles that are measured, because the
    defect was a drawing defect with every geometric check green."""
    if street is None or street.is_empty:
        return 0.0
    from shapely.ops import unary_union
    sheet = unary_union([Polygon([verts[i][:2] for i in t]) for t in tris]).buffer(0)
    return float(sheet.intersection(street).area)

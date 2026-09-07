"""EVERY ROOF SHAPE, LOOKED AT. One footprint, fourteen roofs, one sheet and one render each.

The bed's thirty-five cases exercise seven of the fourteen registered shapes; the other seven had
never been drawn at all, which means nobody had ever checked them. This walks the REGISTRY -- so a
shape added tomorrow appears here without anyone remembering to add it -- builds each on the same
footprint, and asks the three questions a mesh has to answer before a picture is worth looking at:

    CLOSED      no edge with one face, none with more than two
    WOUND       every directed edge (a, b) exactly once with its partner (b, a) exactly once,
                which is what proves the orientation is consistent PER FACE rather than on
                average, and a positive volume, which is what makes that orientation outward
    SNAPPED     no two vertices nearer than the weld tolerance and not welded

    python3 test/lab/buildings/gallery.py [name ...]
"""
import math
import os
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))
import importlib.util as _util  # noqa: E402

_spec = _util.spec_from_file_location("outshine_building_bed", HERE / "synthetic.py")
bed = _util.module_from_spec(_spec)
_spec.loader.exec_module(bed)
import publish  # noqa: E402
import camera as lab_camera  # noqa: E402
import roofs  # noqa: E402

OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "gallery"


# THE COMB, AS A CEILING THAT MAY ONLY FALL. board:2156 measured the cause and wrote what would
# be true; nothing went red while it was not. `Building.comb` is the number: the angle between a
# roof face's normal and the ANALYTIC field's normal at its centroid, over the roof's faces.
#
# It is p95 and not the worst face, because a face that spans a CREASE legitimately disagrees --
# and the table says so itself: `hipped` and `pyramidal` read 0.0 at p95 with 23.9 at the worst,
# which is their ridge. Every shape built from PLANES reads exactly 0.0; every curved one carries
# the comb, and those are the rows a viewer rejects in a second.
#
# Measured 2026-09-07 on F1-rect, cell 0.5. A row may only ever be lowered.
COMB_MOST_DEG = {
    "barrel": 23.0, "butterfly": 0.0, "dome": 23.5, "flat": 0.0, "gabled": 0.0,
    "gambrel": 6.7, "half-hipped": 12.2, "hipped": 0.0, "mansard": 30.4, "onion": 28.8,
    "pyramidal": 0.0, "sawtooth": 0.0, "skillion": 0.0, "spire": 0.0,
}
COMB_WANTED_DEG = 5.0     # [SET] where the ring-and-spoke surface has to bring every row


def one(name, number):
    """One shape on the standard footprint, checked and drawn."""
    poly = bed.FOOTPRINTS["F1-rect"]()
    ground = bed.GROUNDS["G1-flat"]()
    tags = {"building": "yes", "building:levels": 3, "roof:shape": name}
    b = bed.Building(poly, tags, ground, cell=0.5)
    if b.roof != name:
        # the style refused the shape for this use; force it, because the point here is the
        # SHAPE and not the style's opinion of it
        b.roof = name
        b.__init__(poly, dict(tags, **{"roof:shape": name}), ground, cell=0.5)
        b.roof = name
        b._build()
    f = bed.Facade(b)
    closed = b.watertight()
    wrong, degenerate, _ = b.winding()
    vol = b.volume()
    near = _nearest_pair(b)
    red = []
    if not closed:
        red.append(f"open{b.open_edges()}/bad{b.bad_edges()}")
    if wrong or degenerate:
        red.append(f"wound{wrong}e/{degenerate}deg")
    p50, p95, worst = b.comb()
    if p95 > COMB_MOST_DEG.get(name, COMB_WANTED_DEG) + 0.05:
        red.append(f"comb p95 {p95:.1f} deg over {COMB_MOST_DEG.get(name, COMB_WANTED_DEG):.1f}")
    if vol <= 0.0:
        red.append("volume")
    if near < bed.WELD_M:
        red.append(f"snap{near:.5f}")
    OUT.mkdir(parents=True, exist_ok=True)
    bed.OUT = OUT
    publish.take("roofs", f"sheet_{name}", bed.draw((f"R-{name}", "G1-flat", tags), b, f, number), red)
    shot = _render(b, name, number)
    publish.take("roofs", f"view_{name}", shot, red)
    print(f"{number:02d} {name:12s} {'RED ' + ','.join(red) if red else 'ok':22s} "
          f"tris {len(b.tris):6d}  verts {len(b.vertices):6d}  volume {vol:9.1f} m3  "
          f"nearest pair {near:.4f} m  comb p95 {p95:5.1f} max {worst:5.1f}  "
          f"ridge +{b.ridge - b.pad:5.2f}  -> {shot.name}")
    return red


def _nearest_pair(b):
    """The closest two DISTINCT vertices. Under the weld tolerance they should have been one
    vertex: snapping is the MESHER's job here (`Building.vertex` welds on a quantised key at
    1 mm) and this is what says it did it."""
    pts = np.asarray(b.vertices, dtype=float)
    if len(pts) < 2:
        return float("inf")
    from scipy.spatial import cKDTree
    d, _ = cKDTree(pts).query(pts, k=2)
    return float(np.min(d[:, 1]))


def _render(b, name, number):
    """A THREE-QUARTER AERIAL, rendered by CYCLES on the GPU -- the lab's one renderer.

    A roof is what this gallery is about and an eye at nine metres with a seven degree pitch sees
    a box: the first pass drew a skillion as a flat top and a sawtooth as one dark face, and both
    height fields were CORRECT when sampled. The camera looks DOWN at 26 degrees from 22 m with a
    raking sun, which is how a roof is drawn."""
    import sys as _s, pathlib as _p
    _s.path.insert(0, str(_p.Path(__file__).resolve().parents[1]))
    import blend
    import materials as stock
    centre = b.poly.centroid
    cam = lab_camera.Camera(lat=50.0, lon=8.0, agl_m=22.0, bearing_deg=35.0, pitch_deg=-26.0,
                            fov_deg=45.0, width=1100, height=680)
    back = 30.0
    bearing = math.radians(cam.bearing_deg)
    off = np.array([-math.sin(bearing) * back, -math.cos(bearing) * back, 0.0])

    def moved(points):
        return [(p[0] - centre.x - off[0], p[1] - centre.y - off[1], p[2] - b.pad) for p in points]

    mats = b.materials()
    parts, looks = {}, {}
    for role, (vv, tt) in b.body(3).items():
        parts[role] = (moved(vv), tt)
        looks[role] = mats.get(role) or stock.STOCK["render"]
    r = 60.0
    parts["ground"] = (moved([(centre.x - r, centre.y - r, b.pad), (centre.x + r, centre.y - r, b.pad),
                              (centre.x + r, centre.y + r, b.pad), (centre.x - r, centre.y + r, b.pad)]),
                       [(0, 2, 1), (0, 3, 2)])
    looks["ground"] = stock.STOCK["grass"]
    out = OUT / f"{number:02d}_roof-{name}.png"
    blend.render({k: v for k, v in parts.items() if v[1]}, cam,
                 np.array([-0.62, -0.35, 0.70]), str(out), samples=48, looks=looks)
    return out


def main(argv):
    names = [n for n in sorted(roofs.SHAPES) if not argv or any(a in n for a in argv)]
    if not argv:
        publish.sweep("roofs")
    reds = 0
    for number, name in enumerate(names, start=1):
        try:
            reds += bool(one(name, number))
        except Exception as why:
            print(f"{number:02d} {name:12s} REFUSED {type(why).__name__}: {why}")
            reds += 1
    print(f"\n{len(names)} roof shape(s), {reds} red; sheets and renders under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

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
import render as lab_render  # noqa: E402
import roofs  # noqa: E402

OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "gallery"


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
          f"nearest pair {near:.4f} m  ridge +{b.ridge - b.pad:5.2f}  -> {shot.name}")
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
    """A flat-shaded look at the body from a walking eye, so a fold or an inverted face shows."""
    scene = lab_render.Scene()
    centre = b.poly.centroid
    verts = [(v[0] - centre.x, v[1] - centre.y, v[2] - b.pad) for v in b.vertices]
    scene.add(verts, b.tris, (0.78, 0.74, 0.70))
    # a patch of ground so the body has something to stand on
    r = 60.0
    scene.add([(-r, -r, 0.0), (r, -r, 0.0), (r, r, 0.0), (-r, r, 0.0)],
              [(0, 1, 2), (0, 2, 3)], (0.42, 0.46, 0.34))
    # A THREE-QUARTER AERIAL, because a roof is what this gallery is about and an eye at nine
    # metres with a seven degree pitch sees a box: the first pass rendered a skillion as a flat
    # top and a sawtooth as one dark face, and both height fields were CORRECT when sampled.
    # The camera looks DOWN at 26 degrees from 22 m, which is how a roof is drawn.
    cam = lab_render.Camera(lat=50.0, lon=8.0, agl_m=22.0, bearing_deg=35.0, pitch_deg=-26.0,
                            fov_deg=45.0, width=900, height=560)
    back = 30.0
    bearing = math.radians(cam.bearing_deg)
    off = np.array([-math.sin(bearing) * back, -math.cos(bearing) * back, 0.0])
    moved = lab_render.Scene()
    moved.vertices = [(v[0] - off[0], v[1] - off[1], v[2]) for v in scene.vertices]
    moved.tris, moved.colours = scene.tris, scene.colours
    # a LOW sun from the side, so every plane of a roof takes a different tone: a light straight
    # overhead flattens a hip and a mansard into one grey
    img = lab_render.render(moved, cam, np.array([-0.62, -0.35, 0.70]), ambient=0.30)
    return pathlib.Path(lab_render.save(img, OUT / f"{number:02d}_roof-{name}.png"))


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

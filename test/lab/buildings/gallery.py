"""WHAT THE HEURISTIC EMITS, LOOKED AT. A real footprint, real tags, and the roof the GENERATOR
chooses -- never one forced on it.

This bed used to walk the registry and put all fourteen shapes on one 12 x 8 rectangle, forcing
past the style where it refused:

    if b.roof != name:  b.roof = name   # the point here is the SHAPE

and the pictures said so. An onion came out a rounded BOX, because an onion sits on a little
tower and never on a terraced house. A sawtooth came out with ONE tooth, because its bay is
twelve metres and so was the building. A spire ran out of the frame. None of those is what the
generator would ever build; all three were what the bed asked for.

A ROOF FOLLOWS THE PLAN, so the cases are PLANS. Each is a footprint and the tags OSM actually
carries with it, and what appears is whatever `classify` and `Style.roof_for` decide -- which is
the thing that has to be judged, since it is the thing a place is built from. A registered shape
that no case ever reaches is reported as such: either it is dead, or the heuristic will not put
it anywhere, and both are findings rather than a picture to be forced.

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
import shape  # noqa: E402

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
#
# AND p95 IS NOT WHAT THE EYE READS. The barrel's row fell from 23.0 to 6.3 with Shewchuk's
# quality bound and the picture kept its teeth, looked at the same hour. What an eye reads along
# an eaves is the ALTERNATION of neighbours, not the average error, and a few per cent of the
# faces run the whole length of a roof. A neighbour-dihedral oracle was written and thrown away
# in the same round: it separates a gable's ridge from a comb cleanly (70.0 -> 0.0) and a hip's
# grat not at all (47.8), because the faces beside a hip are themselves a little off the field.
# Telling a crease from a comb needs the mesher to SAY which edges are creases, and that is part
# of the ring-and-spoke surface board:2156 is waiting for, not a heuristic to be guessed at here.
COMB_MOST_DEG = {
    "barrel": 6.3, "butterfly": 0.0, "dome": 23.5, "flat": 0.0, "gabled": 0.0,
    "gambrel": 6.7, "half-hipped": 12.2, "hipped": 0.0, "mansard": 30.4, "onion": 28.8,
    "pyramidal": 0.0, "sawtooth": 0.0, "skillion": 0.0, "spire": 0.0,
}
COMB_WANTED_DEG = 5.0     # [SET] where the ring-and-spoke surface has to bring every row


# WHAT OSM ACTUALLY CARRIES, plan by plan. A tag set here is one a surveyor really writes, and
# the footprint is the shape that use really has. Nothing chooses a roof: `classify` and
# `Style.roof_for` do, and that choice is what the picture is of.
CASES = (
    ("village house",   "F13-detached", {"building": "house", "building:levels": 2}),
    ("terrace",         "F10-terrace",  {"building": "terrace", "building:levels": 3}),
    ("bungalow",        "F12-bungalow", {"building": "bungalow", "building:levels": 1}),
    ("semi",            "F14-semi",     {"building": "semidetached_house", "building:levels": 2}),
    ("barn",            "F11-farm",     {"building": "barn"}),
    ("factory hall",    "F9-shed",      {"building": "industrial", "building:levels": 1}),
    ("gothic church",   "F8-church",    {"building": "church", "start_date": "1480"}),
    ("baroque tower",   "F7-tower",     {"building": "church", "start_date": "1720",
                                         "height": "38"}),
    ("rotunda",         "F5-round",     {"building": "chapel"}),
    ("town hall tower", "F7-tower",     {"building": "townhall", "height": "60"}),
    ("courtyard block", "F4-courtyard", {"building": "apartments", "building:levels": 4}),
    ("L block",         "F2-L",         {"building": "apartments", "building:levels": 3}),
    ("U school",        "F3-U",         {"building": "school", "building:levels": 3}),
    ("thin infill",     "F6-thin",      {"building": "yes", "building:levels": 2}),
    ("office",          "F1-rect",      {"building": "office", "building:levels": 9,
                                         "height": "31"}),
    ("garage",          "F1-rect",      {"building": "garage"}),
)


def named(label, b):
    """THE FILE NAME CARRIES THE RULE THAT MADE IT. `view_village_house.png` says which case ran
    and nothing about WHY the roof came out as it did -- and the whole point of this bed is that
    the roof follows the plan. The name is therefore the decision and its evidence:

        village-house__House-hipped__a95_asp1.04_fill0.91_h8.7_st2.png

    which is `shape.use_of` and `shape.roof_of` with every number they were given, so a reader
    can walk `BuildingShape.cpp`'s own branches without opening anything. A picture whose name
    does not say why it looks like that is a picture somebody has to re-derive."""
    area, half_u, half_v, fill, aspect = shape.measured(b.poly)
    return (f"{label.replace(' ', '-')}__{getattr(b, 'use', '?')}-{b.roof}"
            f"__a{area:.0f}_asp{aspect:.2f}_fill{fill:.2f}"
            f"_h{b.ridge - b.pad:.1f}_st{int(b.levels or 1)}")


def one(case, number):
    """One PLAN with its own tags, built the way a place would build it."""
    label, plan, tags = case
    poly = bed.FOOTPRINTS[plan]()
    ground = bed.GROUNDS["G1-flat"]()
    b = bed.Building(poly, dict(tags), ground, cell=0.5)
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
    if p95 > COMB_WANTED_DEG + 0.05:
        red.append(f"comb p95 {p95:.1f} deg over {COMB_WANTED_DEG:.1f}")
    if vol <= 0.0:
        red.append("volume")
    if near < bed.WELD_M:
        red.append(f"snap{near:.5f}")
    OUT.mkdir(parents=True, exist_ok=True)
    bed.OUT = OUT
    publish.take("roofs", f"sheet_{label}", bed.draw((f"R-{label}", plan, tags), b, f, number), red)
    shot = _render(b, label, number)
    publish.take("roofs", named(label, b), shot, red)
    print(f"{number:02d} {label:16s} {'RED ' + ','.join(red) if red else 'ok':22s} "
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
    picked = [c for c in CASES if not argv or any(a.lower() in c[0].lower() or a.lower() in c[1].lower()
                                                  for a in argv)]
    if not argv:
        publish.sweep("roofs")
    reds = 0
    chose = {}
    for number, case in enumerate(picked, start=1):
        try:
            got = one(case, number)
        except Exception as why:
            print(f"{number:02d} {case[0]:16s} REFUSED {type(why).__name__}: {why}")
            reds += 1
            continue
        reds += 1 if got else 0
        poly = bed.FOOTPRINTS[case[1]]()
        chose[case[0]] = bed.Building(poly, dict(case[2]), bed.GROUNDS["G1-flat"](), cell=2.0).roof
    print("\n  the heuristic chose: "
          + ", ".join(f"{k} -> {v}" for k, v in chose.items()))
    # A SHAPE NOTHING REACHES IS A FINDING, NOT A PICTURE TO BE FORCED. Either no plan the
    # generator meets will ever ask for it, or the rule that would has not been written.
    unseen = sorted(set(roofs.SHAPES) - set(chose.values()))
    if unseen and not argv:
        print(f"  registered but never chosen: {', '.join(unseen)}")
    print(f"\n{len(picked)} plan(s), {reds} red; renders under {publish.SHOTS / 'roofs'}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

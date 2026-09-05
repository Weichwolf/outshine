"""The same bed, on REAL landmarks: the footprint, the tags and the ground OSM and the DEM give.

    python3 test/lab/buildings/real.py [name ...]

A synthetic case proves a rule; a real one proves the rule survives the data. Each landmark
below is fetched from Overpass by its position, its largest building way taken with all its
tags, its parts read from `building:part` relations where they exist, and its ground sampled
from the same terrarium tiles the engine reads. Then it goes through the ordinary pipeline --
classify, style, mass, roof, facade, LOD -- and comes out as a sheet.

What this catches that a synthetic case cannot: tags nobody invents (`building:levels:underground`,
`roof:shape=round`, `height` with a unit, `start_date` as a century), footprints with 400 nodes
and holes, and buildings whose type the tag does not say.
"""
import json
import math
import os
import pathlib
import sys
import urllib.parse
import urllib.request

import numpy as np
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import polygonize, unary_union

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "roads"))
import data as roaddata  # noqa: E402
# the building bed sits beside this file and shares the name `synthetic` with the road bed, so
# it is loaded by PATH rather than by name -- importing by name got the road bed (measured)
import importlib.util as _util  # noqa: E402
_spec = _util.spec_from_file_location("outshine_building_bed", HERE / "synthetic.py")
bed = _util.module_from_spec(_spec)
_spec.loader.exec_module(bed)

CACHE = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "real"
OVERPASS = "https://overpass-api.de/api/interpreter"

# Landmarks chosen so that every epoch, every use and every roof this bed knows meets a real
# building somewhere: a Gothic cathedral, a Baroque palace, a Gruenderzeit block, a Bauhaus
# work, a post-war slab, a steel-and-glass tower, a colliery, a station hall, a farmstead.
# The NAME is the fourth column and it is not decoration: a coordinate lands within reach of a
# dozen buildings and the biggest of them is usually the block, so the way or relation whose
# `name` carries this string wins before either test. Without it the Sagrada Familia came out as
# the apartment house across the street (measured).
LANDMARKS = {
    "Koelner-Dom":        (50.94133, 6.95817, "gothic cathedral, 157 m towers", "Dom"),
    "Reichstag":          (52.51863, 13.37617, "1894, Wallot, the dome of 1999", "Reichstag"),
    "Elbphilharmonie":    (53.54130, 9.98410, "2016 on a 1963 warehouse", "Elbphilharmonie"),
    "Empire-State":       (40.74844, -73.98566, "1931 art deco, 102 storeys", "Empire State"),
    "Chrysler-Building":  (40.75165, -73.97551, "1930 art deco", "Chrysler"),
    "Transamerica":       (37.79520, -122.40280, "1972 pyramid, San Francisco", "Transamerica"),
    "Zollverein-XII":     (51.48630, 7.04430, "1932 colliery, Essen, Bauhaus in steel", "Kohlenw"),
    "Villa-Savoye":       (48.92472, 2.02694, "1931 Le Corbusier", "Savoye"),
    "Bundeshaus-Bern":    (46.94650, 7.44440, "1902, Swiss parliament", "Bundeshaus"),
    "Wuppertal-Rathaus":  (51.25620, 7.15020, "1900, Gruenderzeit", "Rathaus"),
    "Zytglogge":          (46.94790, 7.44770, "13th century tower, Bern", "Zytglogge"),
    "Sagrada-Familia":    (41.40363, 2.17435, "Gaudi, begun 1882", "Sagrada"),
    "Hamburg-Speicher":   (53.54430, 9.98890, "Speicherstadt warehouse, 1888", "Speicher"),
    "Paris-Gare-du-Nord": (48.88090, 2.35520, "1864 station hall", "building=train_station"),
    "Zuerich-HB":         (47.37770, 8.53990, "1871 station", "building=train_station"),
}


def fetch(name, lat, lon, reach=160.0):
    """The largest building way within `reach` of the point, with every tag it carries."""
    CACHE.mkdir(parents=True, exist_ok=True)
    held = CACHE / f"{name}.json"
    if not held.exists():
        query = (f'[out:json][timeout:120];'
                 f'(way["building"](around:{reach:.0f},{lat},{lon});'
                 f' relation["building"](around:{reach:.0f},{lat},{lon});'
                 f' way["building:part"](around:{reach:.0f},{lat},{lon}););'
                 f'(._;>;);out body;')
        request = urllib.request.Request(
            OVERPASS, data=urllib.parse.urlencode({"data": query}).encode(),
            headers={"User-Agent": "outshine-lab"})
        with urllib.request.urlopen(request, timeout=300) as answer:
            held.write_bytes(answer.read())
    doc = json.loads(held.read_text())
    nodes = {e["id"]: (e["lat"], e["lon"]) for e in doc["elements"] if e["type"] == "node"}
    ways = [e for e in doc["elements"] if e["type"] == "way" and "nodes" in e]
    rels = [e for e in doc["elements"] if e["type"] == "relation"]
    return nodes, ways, rels


def from_relation(rel, ways, nodes, lat0, lon0):
    """A MULTIPOLYGON RELATION is a building too, and the biggest ones usually are: its outer
    members are open ways that have to be sewn into rings first. `polygonize` does exactly that,
    and the largest ring it returns is the outline."""
    by_id = {w["id"]: w for w in ways}
    lines = []
    for member in rel.get("members", ()):
        if member.get("type") != "way" or member.get("role") not in ("outer", ""):
            continue
        w = by_id.get(member.get("ref"))
        if w is None:
            continue
        # `as_metres` drops a way's repeated closing node, which leaves the ring OPEN, and
        # `polygonize` returns nothing for an open line: Zurich's station relation produced no
        # polygon at all and the 44 000 m2 platform roof won on area (measured). Close it again
        pts = as_metres(nodes, w["nodes"], lat0, lon0)
        if len(pts) >= 3 and pts[0] != pts[-1] and w["nodes"][0] == w["nodes"][-1]:
            pts = pts + [pts[0]]
        if len(pts) >= 2:
            lines.append(LineString(pts))
    if not lines:
        return None
    made = [g for g in polygonize(unary_union(lines))]
    if not made:
        return None
    return max(made, key=lambda g: g.area)


def as_metres(nodes, refs, lat0, lon0):
    per_lat = 111132.0
    per_lon = 111320.0 * math.cos(math.radians(lat0))
    out = []
    for r in refs:
        if r not in nodes:
            continue
        la, lo = nodes[r]
        out.append(((lo - lon0) * per_lon, (la - lat0) * per_lat))
    if len(out) >= 3 and out[0] == out[-1]:
        out = out[:-1]
    return out


def biggest(nodes, ways, rels, lat0, lon0, want=""):
    best, best_rank, parts = None, (-1, -1, -1.0), []
    candidates = []
    for w in ways:
        pts = as_metres(nodes, w["nodes"], lat0, lon0)
        if len(pts) >= 3:
            candidates.append((pts, w.get("tags", {})))
    for r in rels:
        if "building" not in r.get("tags", {}) and r.get("tags", {}).get("type") != "multipolygon":
            continue
        got = from_relation(r, ways, nodes, lat0, lon0)
        if got is not None:
            candidates.append((list(got.exterior.coords)[:-1], r.get("tags", {})))
    for pts, tags in candidates:
        try:
            poly = Polygon(pts)
            if not poly.is_valid:
                poly = poly.buffer(0)
        except Exception:
            continue
        if poly.is_empty or poly.geom_type != "Polygon":
            continue
        if "building" not in tags:
            continue
        # the NAME wins, then the geometry that CONTAINS the point, then the area: at the
        # Chrysler Building the largest way within reach is the block it stands in, and at the
        # Sagrada Familia the largest one that holds the point is an apartment house
        # the hint matches a NAME, or -- where OSM gives the landmark no name at all -- the
        # `building` value: Zurich's main station is an unnamed `building=train_station` RELATION
        # standing under a 44 000 m2 `building=roof` way, and area alone always picks the roof
        if want.startswith("building="):
            named = tags.get("building") == want.split("=", 1)[1]
        else:
            named = bool(want) and want.lower() in str(tags.get("name", "")).lower()
        holds = poly.contains(Point(0.0, 0.0))
        rank = (1 if named else 0, 1 if holds else 0, poly.area)
        if best is None or rank > best_rank:
            best, best_rank = (poly, tags), rank
    if best is None:
        return None, []
    for w in ways:
        tags = w.get("tags", {})
        if "building:part" not in tags:
            continue
        pts = as_metres(nodes, w["nodes"], lat0, lon0)
        if len(pts) < 3:
            continue
        poly = Polygon(pts)
        if not poly.is_valid:
            poly = poly.buffer(0)
        if poly.is_empty or poly.geom_type != "Polygon" or poly.area < 4.0:
            continue
        if poly.intersection(best[0]).area < poly.area * 0.4:
            continue
        parts.append((poly, tags))
    # a part is kept for its HEIGHT before its area: the Koelner Dom's towers are its smallest
    # parts by footprint and its whole silhouette, and eight parts taken by area drew a
    # cathedral with no spires (measured)
    parts.sort(key=lambda pt: (-_told_height(pt[1]), -pt[0].area))
    return best, parts


def _told_height(tg):
    """What a part's tags say it is high, in metres, or zero if they say nothing."""
    got = clean(tg)
    return float(got.get("height", 0) or 0) or float(got.get("building:levels", 0) or 0) * 3.2


def clean(tags):
    """OSM's own units and spellings, made into the numbers the bed reads. Everything a real
    dataset does and a synthetic case never: `height=157 m`, `building:levels=5;6`, a
    `start_date` of `C13`, `roof:shape=round`."""
    out = dict(tags)
    for key in ("height", "min_height", "roof:height", "building:levels", "roof:levels"):
        if key not in out:
            continue
        raw = str(out[key]).strip().lower().replace(",", ".")
        raw = raw.split(";")[0].replace("m", "").replace("meter", "").strip()
        try:
            out[key] = float(raw)
        except ValueError:
            del out[key]
    told = str(out.get("start_date", "")).strip()
    if told.upper().startswith("C") and told[1:].isdigit():
        out["start_date"] = str((int(told[1:]) - 1) * 100 + 50)     # C13 -> 1250
    shape = out.get("roof:shape")
    if shape in ("round",):
        out["roof:shape"] = "barrel"
    if shape in ("many", "complex", "unknown"):
        out.pop("roof:shape")
    return out


class RealGround:
    """The DEM the engine reads, at the building's own place."""

    def __init__(self, lat0, lon0):
        self.dem = roaddata.Dem()
        self.lat0, self.lon0 = lat0, lon0
        self.water = None
        self.per_lat = 111132.0
        self.per_lon = 111320.0 * math.cos(math.radians(lat0))

    def at(self, x, y):
        lat = self.lat0 + y / self.per_lat
        lon = self.lon0 + x / self.per_lon
        return float(self.dem.at(lat, lon))


def run(name, number):
    lat, lon, note, want = LANDMARKS[name]
    nodes, ways, rels = fetch(name, lat, lon)
    got, parts = biggest(nodes, ways, rels, lat, lon, want)
    if got is None:
        print(f"{number:02d} {name:20s} no building way within reach")
        return None
    poly, tags = got
    tags = clean(tags)
    ground = RealGround(lat, lon)
    # SIMPLE 3D BUILDINGS: where `building:part` ways cover the outline, THEY carry the heights
    # and the outline way's `height` is the tallest point of the whole -- the Koelner Dom's 157 m
    # is its towers, and extruding the entire 146 x 87 m outline to it drew a box the size of the
    # cathedral's spires (measured). The mass then takes the LOWEST part's height, which is the
    # aisle, and every part rises out of it: aisle 20 m, nave 61 m, towers 157 m
    made = []
    for (ppoly, ptags) in parts[:14]:
        merged = dict(tags)
        merged.pop("height", None)
        merged.pop("building:levels", None)
        merged.update(clean(ptags))
        try:
            made.append(bed.Building(ppoly, merged, ground, cell=1.2))
        except Exception:
            continue
    covered = 0.0
    if parts:
        covered = unary_union([pp for pp, _ in parts]).intersection(poly).area / max(poly.area, 1e-9)
    if made and covered > 0.5:
        # the LOWEST part is the aisle and it sets the mass, and it is read from EVERY part and
        # not only from the eight that are drawn -- the eight are chosen for their height, so
        # their minimum is a tower and the outline went back to 157 m (measured)
        told = [h for h in (_told_height(pt[1]) for pt in parts) if h > 0.5]
        tags = dict(tags)
        tags["height"] = min(told) if told else min(m.ridge - m.pad for m in made)
        tags.pop("building:levels", None)
    b = bed.Building(poly, tags, ground, cell=max(0.8, math.sqrt(poly.area) / 40.0))
    b.parts = made
    f = bed.Facade(b)
    red = f.faults()
    if not b.watertight():
        red.append("B-closed")
    if b.volume() <= 0:
        red.append("B-wound")
    shown = {k: v for k, v in tags.items()
             if k in ("building", "height", "building:levels", "start_date", "roof:shape",
                      "building:architecture", "min_height", "name")}
    # the sheet's title splits its first field on the first hyphen (the synthetic beds name
    # cases `F1-rect`), so a landmark's own hyphen would print "Familia" for the Sagrada Familia
    case = (name.replace("-", "_"), f"{lat:.4f},{lon:.4f}", shown)
    bed.OUT = CACHE
    bed.draw(case, b, f, number)
    print(f"{number:02d} {name:20s} {b.style.kind[:12]:12s} {b.style.epoch:13s} {b.roof:10s} "
          f"{'RED ' + ','.join(red) if red else 'ok':22s} "
          f"nodes {len(poly.exterior.coords):4d} parts {len(b.parts):2d} "
          f"area {poly.area:8.0f} m2  levels {f.levels():3d}  H {b.ridge - b.pad:6.1f} m  "
          f"tris {len(b.tris):6d}   {note}")
    return red


def main(argv):
    picked = [n for n in LANDMARKS if not argv or any(a.lower() in n.lower() for a in argv)]
    reds = 0
    for number, name in enumerate(sorted(picked), start=1):
        try:
            red = run(name, number)
        except Exception as why:                     # a real dataset refuses in its own ways
            print(f"{number:02d} {name:20s} REFUSED {type(why).__name__}: {why}")
            reds += 1
            continue
        reds += bool(red)
    print(f"\n{len(picked)} landmarks, {reds} red; sheets under {CACHE}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

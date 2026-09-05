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
from shapely.geometry import Point, Polygon
from shapely.ops import unary_union

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
LANDMARKS = {
    "Koelner-Dom":        (50.94133, 6.95817, "gothic cathedral, 157 m towers"),
    "Reichstag":          (52.51863, 13.37617, "1894, Wallot, the dome of 1999"),
    "Elbphilharmonie":    (53.54130, 9.98410, "2016 on a 1963 warehouse"),
    "Empire-State":       (40.74844, -73.98566, "1931 art deco, 102 storeys"),
    "Chrysler-Building":  (40.75165, -73.97551, "1930 art deco"),
    "Transamerica":       (37.79520, -122.40280, "1972 pyramid, San Francisco"),
    "Zollverein-XII":     (51.48660, 7.04520, "1932 colliery, Essen, Bauhaus in steel"),
    "Villa-Savoye":       (48.92360, 2.02690, "1931 Le Corbusier"),
    "Bundeshaus-Bern":    (46.94650, 7.44440, "1902, Swiss parliament"),
    "Wuppertal-Rathaus":  (51.25620, 7.15020, "1900, Gruenderzeit"),
    "Zytglogge":          (46.94790, 7.44770, "13th century tower, Bern"),
    "Sagrada-Familia":    (41.40360, 2.17440, "Gaudi, begun 1882"),
    "Hamburg-Speicher":   (53.54430, 9.98890, "Speicherstadt warehouse, 1888"),
    "Paris-Gare-du-Nord": (48.88090, 2.35520, "1864 station hall"),
    "Zuerich-HB":         (47.37790, 8.54020, "1871 station"),
}


def fetch(name, lat, lon, reach=90.0):
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
    return nodes, ways


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


def biggest(nodes, ways, lat0, lon0):
    best, best_rank, parts = None, (-1, -1.0), []
    for w in ways:
        pts = as_metres(nodes, w["nodes"], lat0, lon0)
        if len(pts) < 3:
            continue
        try:
            poly = Polygon(pts)
            if not poly.is_valid:
                poly = poly.buffer(0)
        except Exception:
            continue
        if poly.is_empty or poly.geom_type != "Polygon":
            continue
        tags = w.get("tags", {})
        if "building" not in tags:
            continue
        # the way that CONTAINS the point wins over the biggest one: at the Chrysler Building
        # the largest way within reach is the block it stands in, and the sheet came out as a
        # two-storey hall (measured). A landmark's coordinate is the landmark
        holds = poly.contains(Point(0.0, 0.0))
        rank = (1 if holds else 0, poly.area)
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
    parts.sort(key=lambda pt: -pt[0].area)
    return best, parts


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
    lat, lon, note = LANDMARKS[name]
    nodes, ways = fetch(name, lat, lon)
    got, parts = biggest(nodes, ways, lat, lon)
    if got is None:
        print(f"{number:02d} {name:20s} no building way within reach")
        return None
    poly, tags = got
    tags = clean(tags)
    ground = RealGround(lat, lon)
    b = bed.Building(poly, tags, ground, cell=max(0.8, math.sqrt(poly.area) / 40.0))
    b.parts = []
    for (ppoly, ptags) in parts[:8]:
        merged = dict(tags)
        merged.update(clean(ptags))
        try:
            b.parts.append(bed.Building(ppoly, merged, ground, cell=1.2))
        except Exception:
            continue
    f = bed.Facade(b)
    red = f.faults()
    if not b.watertight():
        red.append("B-closed")
    if b.volume() <= 0:
        red.append("B-wound")
    case = (name, f"{lat:.4f},{lon:.4f}", tags)
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

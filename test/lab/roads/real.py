"""The road bed on REAL infrastructure: the hardest structures this planet actually holds.

    python3 test/lab/roads/real.py [name ...]      # a substring picks; no argument runs all

A synthetic case proves a rule in isolation. A real one proves the rule survives a network
somebody surveyed: ways that leave the extract half-built, a bridge tagged on three of its five
segments, a `layer` nobody set, a viaduct whose piers are separate ways, a DEM with the deck
already baked into it. The catalogue below is a LADDER -- one clean structure, then a junction,
then a mountain, then rail, then the places where road, rail and water stand on four levels --
and the rule is that a rung is not climbed until its sheet has been LOOKED AT and accepted.

Each case goes through the same bed as `synthetic.py`: the network is snapped and deduped, the
profile solved as the same QP, the junctions found by the same clustering, the structures typed by
their FREE SPAN, and the surface meshed and checked against the same invariants I1..I11.
"""
import json
import math
import os
import pathlib
import sys
import urllib.parse
import urllib.request

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import data as roaddata  # noqa: E402
import importlib.util as _util  # noqa: E402

_spec = _util.spec_from_file_location("outshine_road_bed", HERE / "synthetic.py")
bed = _util.module_from_spec(_spec)
_spec.loader.exec_module(bed)

CACHE = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "infra"
OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "infra-sheets"
MIRRORS = ("https://overpass-api.de/api/interpreter",
           "https://overpass.kumi.systems/api/interpreter",
           "https://overpass.private.coffee/api/interpreter")

# THE LADDER. reach is the half-width of the extract in metres: big enough that the structure and
# both its landings are inside, small enough that the solve is one place and not a city.
CASES = {
    # -- rung 1: ONE structure, its type unmistakable ------------------------------------------
    "1A-GoldenGate":     (37.81990, -122.47830, 700, "suspension, 1280 m main span, 1937", "golden gate"),
    "1B-Muengsten":      (51.18890, 7.12470, 320, "steel truss arch, 107 m over the Wupper, 1897", "müngstener"),
    "1C-PonteVecchio":   (43.76800, 11.25310, 160, "segmental stone arches, built over, 1345", "ponte vecchio"),
    "1D-ForthBridge":    (56.00060, -3.38830, 800, "cantilever truss, two 521 m spans, 1890", "forth bridge"),
    "1E-Glenfinnan":     (56.87570, -5.43310, 350, "21 concrete arches on a curve, 1901", "glenfinnan"),
    "1F-Goeltzschtal":   (50.62220, 12.24280, 300, "brick arcade on four storeys, 1851", "göltzschtal"),
    "1G-Millau":         (44.07970, 3.02210, 900, "cable-stayed, seven pylons, 2004", "millau"),
    "1H-TowerBridge":    (51.50550, -0.07540, 250, "bascule plus suspension, 1894", "tower bridge"),
    "1I-Hohenzollern":   (50.94140, 6.96600, 300, "three steel arches, rail, 1911", "hohenzollern"),
    "1J-Landwasser":     (46.68050, 9.67520, 250, "curved viaduct straight into a tunnel, 1902", "landwasser"),
    "1K-Brusio":         (46.25550, 10.13290, 200, "open spiral viaduct, rail gains height in a circle", ""),
    "1L-Storseisundet":  (62.99860, 7.08000, 300, "cantilever curve, the Atlantic Road", "storseisundet"),

    # -- rung 2: JUNCTIONS, where the network decides the geometry ------------------------------
    "2A-Gravelly":       (52.51170, -1.84770, 500, "Spaghetti Junction, five levels, 18 routes", "m6"),
    "2B-Pregerson":      (33.92800, -118.27250, 600, "a four-level stack, Los Angeles", "105"),
    "2C-Kaiserberg":     (51.44420, 6.80060, 500, "A3 x A40, the tree's own place", "a 3"),
    "2D-MagicRoundabout": (51.56300, -1.77080, 180, "five mini roundabouts around one", ""),
    "2E-Etoile":         (48.87380, 2.29500, 300, "twelve radial avenues on one ring", "charles de gaulle"),
    "2F-Puxi":           (31.22460, 121.46920, 450, "a five-level interchange, Shanghai", ""),

    # -- rung 3: the MOUNTAIN, where the profile is the whole problem ---------------------------
    "3A-Stelvio":        (46.52850, 10.45270, 500, "48 hairpins, 1848 m of climb", "stilfser"),
    "3B-Lombard":        (37.80210, -122.41870, 150, "eight hairpins in 180 m, 27 % beside them", "lombard"),
    "3C-Transfagarasan": (45.60350, 24.61720, 500, "hairpins under a dam wall", "transf"),
    "3D-Furka":          (46.57230, 8.41520, 500, "a pass road with galleries and avalanche sheds", "furka"),

    # -- rung 4: RAIL, where the geometry is stricter than a road's -----------------------------
    "4A-ZuerichHB":      (47.37850, 8.53850, 400, "the throat: sixteen tracks fanning out", ""),
    "4B-SemmeringRinne": (47.63830, 15.82670, 350, "the Kalte Rinne viaduct, two storeys of arches", ""),
    "4C-Schwebebahn":    (51.25600, 7.15000, 400, "a monorail hung over a river, 1901", "schwebebahn"),
    "4D-HamburgHbf":     (53.55280, 10.00650, 400, "a hall over the tracks, throat both ends", ""),

    # -- rung 5: the VERTICAL STACK, where layer is the only thing that separates them ----------
    "5A-Elbtunnel":      (53.54330, 9.96780, 500, "portals in the dyke, six lanes under a river", "elbtunnel"),
    "5B-WackerDrive":    (41.88700, -87.63200, 300, "a double-decked street along a river", "wacker"),
    "5C-YerbaBuena":     (37.81290, -122.36330, 500, "a bridge into a tunnel into a bridge", "80"),
    "5D-HongKongCentral": (22.28150, 114.15810, 350, "elevated walkways over an elevated road", ""),
}


def fetch(name, lat, lon, reach):
    """Every way with a `highway` or a `railway` in the box, with its nodes."""
    CACHE.mkdir(parents=True, exist_ok=True)
    held = CACHE / f"{name}.json"
    if held.exists():
        return json.loads(held.read_text())
    dlat = reach / 111132.0
    dlon = reach / (111320.0 * math.cos(math.radians(lat)))
    box = f"{lat - dlat:.6f},{lon - dlon:.6f},{lat + dlat:.6f},{lon + dlon:.6f}"
    query = (f'[out:json][timeout:180];'
             f'(way["highway"]({box});way["railway"]({box}););'
             f'(._;>;);out body;')
    last = None
    for mirror in MIRRORS:
        try:
            request = urllib.request.Request(
                mirror, data=urllib.parse.urlencode({"data": query}).encode(),
                headers={"User-Agent": "outshine-lab"})
            with urllib.request.urlopen(request, timeout=240) as answer:
                held.write_bytes(answer.read())
            return json.loads(held.read_text())
        except Exception as why:                      # a mirror refuses in its own way
            last = why
    raise last


# What a way IS, in the bed's own words. OSM's `highway` and `railway` values are a vocabulary of
# a hundred; the bed needs a width, a priority and a design speed, and the table below is the
# smallest one that covers what the ladder meets. Widths are the carriageway between kerbs, from
# RAS-Q for the German classes and from the way's own `lanes` where it carries one.
ROAD_WIDTH = {"motorway": 12.5, "motorway_link": 6.5, "trunk": 11.0, "trunk_link": 6.5,
              "primary": 9.5, "primary_link": 6.0, "secondary": 8.5, "secondary_link": 6.0,
              "tertiary": 7.5, "tertiary_link": 5.5, "unclassified": 6.0, "residential": 6.0,
              "living_street": 5.0, "service": 4.0, "track": 3.5, "pedestrian": 6.0,
              "footway": 2.5, "cycleway": 2.5, "path": 1.8, "steps": 1.8, "bridleway": 2.0}
ROAD_RANK = {"motorway": 14, "trunk": 12, "primary": 10, "secondary": 8, "tertiary": 7,
             "unclassified": 5, "residential": 5, "living_street": 4, "service": 3,
             "pedestrian": 4, "track": 2, "footway": 1, "cycleway": 1, "path": 1, "steps": 1}
RAIL_WIDTH = {"rail": 5.0, "light_rail": 4.6, "subway": 4.6, "tram": 3.4, "narrow_gauge": 3.6,
              "monorail": 3.0, "funicular": 3.4, "preserved": 5.0, "disused": 5.0}


def _float(tags, key, otherwise=0.0):
    raw = str(tags.get(key, "")).split(";")[0].strip().lower()
    raw = raw.replace("m", "").replace("meter", "").replace(",", ".").strip()
    try:
        return float(raw)
    except ValueError:
        return otherwise


def width_of(tags):
    """The carriageway's width: the way's own `width` where it carries one, else `lanes` times a
    lane, else the class's default. A `width` tag on a bridge is the DECK and is wider than the
    carriageway, so it is only trusted when the class has no better answer."""
    told = _float(tags, "width")
    if told > 0.5:
        return min(told, 40.0)
    lanes = _float(tags, "lanes")
    kind = tags.get("highway") or ""
    if lanes >= 1.0:
        lane = 3.75 if kind in ("motorway", "trunk", "motorway_link", "trunk_link") else 3.25
        return lanes * lane + (0.5 if kind in ("motorway", "trunk") else 0.0)
    if kind:
        return ROAD_WIDTH.get(kind, 6.0)
    return RAIL_WIDTH.get(tags.get("railway", ""), 5.0)


def net_of(nodes, ways, lat0, lon0):
    """The OSM extract as the bed's own Network, in metres east and north of the centre."""
    per_lat = 111132.0
    per_lon = 111320.0 * math.cos(math.radians(lat0))
    net = bed.Network()
    seen = {}
    kept, dropped = 0, 0
    for w in ways:
        tags = dict(w.get("tags", {}))
        kind = tags.get("highway") or tags.get("railway")
        if kind is None or kind in ("platform", "construction", "proposed", "razed",
                                    "abandoned", "elevator", "raceway"):
            dropped += 1
            continue
        refs = []
        for r in w.get("nodes", ()):
            if r not in nodes:
                continue
            if r not in seen:
                la, lo = nodes[r]
                seen[r] = net.node((lo - lon0) * per_lon, (la - lat0) * per_lat, id_=r)
            refs.append(seen[r])
        if len(refs) < 2:
            dropped += 1
            continue
        out = dict(tags)
        out["width"] = width_of(tags)
        if "highway" not in out:
            # the bed speaks `highway`; a railway is a way with a rank of its own and a design
            # speed that is not a road's, and calling it one would give it a road's crossfall
            out["highway"] = "rail"
            out["rail"] = tags.get("railway")
        out["priority"] = ROAD_RANK.get(out["highway"], 6 if "rail" in out else 4)
        out["layer"] = int(_float(tags, "layer", 0.0))
        net.way(refs, **out)
        kept += 1
    return net, kept, dropped


class RealTerrain:
    """The engine's own DEM at this place, as the bed's terrain function."""

    def __init__(self, lat0, lon0):
        self.dem = roaddata.Dem()
        self.lat0, self.lon0 = lat0, lon0
        self.per_lat = 111132.0
        self.per_lon = 111320.0 * math.cos(math.radians(lat0))

    def __call__(self, x, y):
        return float(self.dem.at(self.lat0 + y / self.per_lat, self.lon0 + x / self.per_lon))


def axis_of(m, case):
    """WHICH WAY THE SHEET IS ABOUT. A real extract holds hundreds and the longest is whichever
    street happens to be longest, so the case names its own: a substring of `name` or `ref`
    first, then -- for a case that is a structure -- the longest way carrying `bridge` or
    `tunnel`, and only then the longest way at all."""
    want = case[4].lower() if len(case) > 4 and case[4] else ""
    ways = list(m.net.ways)
    if not ways:
        return None

    def length(w):
        return m.stations[w["id"]][-1]

    if want:
        named = [w for w in ways
                 if want in str(w["tags"].get("name", "")).lower()
                 or want in str(w["tags"].get("ref", "")).lower()
                 or want in str(w["tags"].get("bridge:name", "")).lower()]
        if named:
            # a landmark's NAME sits on its approaches too: three ways are called Ponte Vecchio
            # and only one carries `bridge`. The structure wins over the length (measured)
            spans_named = [w for w in named if m.net.spans(w) or m.net.bores(w)]
            return max(spans_named or named, key=length)
    spans = [w for w in ways if m.net.spans(w) or m.net.bores(w)]
    if spans:
        return max(spans, key=length)
    return max(ways, key=length)


def run(name, number, only=None):
    lat, lon, reach, note, want = CASES[name]
    doc = fetch(name, lat, lon, reach)
    nodes = {e["id"]: (e["lat"], e["lon"]) for e in doc["elements"] if e["type"] == "node"}
    ways = [e for e in doc["elements"] if e["type"] == "way" and "nodes" in e]
    net, kept, dropped = net_of(nodes, ways, lat, lon)
    ground = RealTerrain(lat, lon)
    terrain = bed.Terrain(ground, extent=reach + 200.0, posting=bed.POSTING_M)
    m = bed.Map(terrain, net)
    # THE EXTRACT IS A BOX AND ITS EDGE CUTS RAMPS. A way that leaves it climbs on outside, so
    # its profile there owes the DEM nothing -- see `mark_open_ends`.
    m.mark_open_ends(reach)
    m = m.solve()
    st = bed.Structure(m)
    red = []
    verdict = {
        "I1 C0 m": bed.check_c0(m),
        "I2 C1 grade": bed.check_c1(m),
        "I3 |z-dem| / band": bed.check_dem_band(m),
        "I6 junction step m": st.check_junction_steps() if m.junctions else None,
        "I4 bridge": bed.check_bridge(m),
        "I5 tunnel": bed.check_tunnel(m),
        "I12 clearance residual m": bed.check_clearance_fixpoint(m),
        "P finite": bed.check_finite(m),
    }
    if verdict["I1 C0 m"] > 1e-9:
        red.append("I1")
    if verdict["I2 C1 grade"] > 1e-6:
        red.append("I2")
    if verdict["I3 |z-dem| / band"] > 1.0 + 1e-6:
        red.append("I3")
    if verdict["I6 junction step m"] is not None and verdict["I6 junction step m"] > bed.STEP_TOL_M:
        red.append("I6")
    if verdict["I12 clearance residual m"] > bed.CLEARANCE_TOL_M:
        red.append("I12")
    if not verdict["P finite"]:
        red.append("Pfinite")
    bed.OUT = OUT
    bed.plot((name, note), m, st, number, seed=axis_of(m, CASES[name]))
    spans = sum(1 for w in net.ways if net.spans(w))
    bores = sum(1 for w in net.ways if net.bores(w))
    rails = sum(1 for w in net.ways if "rail" in w["tags"])
    layers = sorted({w["tags"].get("layer", 0) for w in net.ways})
    print(f"{number:02d} {name:20s} {'RED ' + ','.join(red) if red else 'ok':16s} "
          f"ways {kept:4d} (-{dropped:3d})  nodes {len(net.nodes):5d}  bridge {spans:3d}  "
          f"tunnel {bores:3d}  rail {rails:3d}  layers {min(layers)}..{max(layers)}  "
          f"C0 {verdict['I1 C0 m']:.1e}  band {verdict['I3 |z-dem| / band']:5.2f}x  "
          f"fix {getattr(m, 'clearance_rounds', 0):2d}r {bed.clearance_verdict(m):10s} "
          f"{verdict['I12 clearance residual m']:7.4f} m   {note}")
    return red


def main(argv):
    picked = [n for n in CASES if not argv or any(a.lower() in n.lower() for a in argv)]
    reds = 0
    for number, name in enumerate(sorted(picked), start=1):
        try:
            reds += bool(run(name, number))
        except Exception as why:
            print(f"{number:02d} {name:20s} REFUSED {type(why).__name__}: {why}")
            reds += 1
    print(f"\n{len(picked)} case(s), {reds} red; sheets under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""What the DEM says along every OSM way of a place, and what the class table would allow.

    python3 test/lab/roads/profile.py OldTown

The engine's map (Path::Network::Elevate) samples the DEM at every way point and interpolates
with a C1 Hermite; this script does the same in numpy and then MEASURES rather than assumes:
how the grades are distributed per class, which ways carry the outliers, and what they are.
"""
import collections
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import data  # noqa: E402

TREE = pathlib.Path(__file__).resolve().parents[3]


def load(place, half_m=1500.0):
    lat, lon = data.PLACES[place]
    nodes, ways = data.overpass_ways(lat, lon, half_m)
    dem = data.Dem()
    table = data.class_table(TREE)
    heights = {nid: dem.at(*ll) for nid, ll in nodes.items()}
    made = []
    for way in ways:
        refs = [r for r in way["nodes"] if r in nodes]
        if len(refs) < 2:
            continue
        kind = way["tags"].get("highway")
        row = table.get(kind)
        s = [0.0]
        for a, b in zip(refs, refs[1:]):
            s.append(s[-1] + data.haversine_m(nodes[a], nodes[b]))
        made.append({
            "id": way["id"], "name": way["tags"].get("name", ""), "kind": kind,
            "grade_max": row["maxGradient"] if row else None,
            "sealed": bool(row["sealed"]) if row else None,
            "spans": way["tags"].get("bridge", "no") not in ("no",) or way["tags"].get("tunnel", "no") not in ("no",),
            "refs": refs, "s": np.array(s), "h": np.array([heights[r] for r in refs]),
        })
    return nodes, made, dem, table


def segment_grades(way):
    ds = np.diff(way["s"])
    dh = np.diff(way["h"])
    ok = ds > 0
    return np.abs(dh[ok] / ds[ok]), ds[ok]


def main(argv):
    place = argv[0] if argv else "OldTown"
    nodes, ways, dem, table = load(place)
    lat = data.PLACES[place][0]
    print(f"{place}: {len(ways)} ways, {len(nodes)} nodes, DEM posting {dem.posting_m(lat):.2f} m at zoom {dem.zoom}")
    per_kind = collections.defaultdict(list)
    for way in ways:
        g, ds = segment_grades(way)
        per_kind[way["kind"]].append((g, ds))
    print(f"{'kind':14s} {'segs':>6s} {'p50':>6s} {'p90':>6s} {'p99':>6s} {'max':>7s} {'class':>6s} {'>class':>7s} {'>class&ds<12m':>14s}")
    for kind, held in sorted(per_kind.items(), key=lambda kv: -sum(len(g) for g, _ in kv[1])):
        g = np.concatenate([x for x, _ in held])
        ds = np.concatenate([x for _, x in held])
        limit = table.get(kind, {}).get("maxGradient")
        over = int((g > limit).sum()) if limit else 0
        short = int(((g > limit) & (ds < 12.0)).sum()) if limit else 0
        print(f"{kind:14s} {len(g):6d} {np.percentile(g, 50):6.3f} {np.percentile(g, 90):6.3f} {np.percentile(g, 99):6.3f} {g.max():7.3f} {limit if limit else 0:6.2f} {over:7d} {short:14d}")
    worst = sorted(ways, key=lambda w: -segment_grades(w)[0].max() if len(segment_grades(w)[0]) else 0)[:8]
    print("\nthe eight steepest segments, by way:")
    for way in worst:
        g, ds = segment_grades(way)
        at = int(np.argmax(g))
        print(f"  {way['kind']:12s} {way['name'][:28]:28s} grade {g[at]:6.3f} over {ds[at]:6.1f} m, way {way['id']}, length {way['s'][-1]:7.1f} m, {len(way['refs'])} points")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

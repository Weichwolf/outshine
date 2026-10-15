"""SHA-pinned OSM polygon acquisition and metric experiment inputs."""

import hashlib
import json
import math
import time
from collections import defaultdict
from itertools import pairwise
from pathlib import Path

import mapbox_vector_tile
import numpy as np
import shapely
from outshine_ref.materials import MaterialRegion, coalesce_regions


def prepare(cache, manifest, inventory):
    wanted = {}
    sites = ("Vienna", "CentralPark", "Tokyo", "Feldkirch")
    for site in sites:
        rows = sorted(
            (
                r
                for r in inventory["receipts"]
                if site in r["sites"] and r["tile"][0] == 14
            ),
            key=lambda row: row["tile"],
        )
        receipt = rows[len(rows) // 2]
        wanted[receipt["sha256"]] = (site, receipt)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    found = {}
    for path in cache.iterdir():
        if not path.is_file() or path.stat().st_size > 8 * 1024 * 1024:
            continue
        with path.open("rb") as source:
            prefix = source.read(12)
        if not prefix.startswith((b"\x1a", b"\x1f\x8b")):
            continue
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest not in wanted:
            continue
        site, receipt = wanted[digest]
        target = manifest.parent / (digest + ".pbf")
        target.write_bytes(data)
        found[site] = {"receipt": receipt, "cache_file": str(path), "path": str(target)}
        if len(found) == len(wanted):
            break
    manifest.write_text(
        json.dumps({"sites": found, "requested_sites": sites}, indent=2) + "\n"
    )


def polygons(site, source, inventory):
    path = Path(source["path"])
    data = path.read_bytes()
    receipt = source["receipt"]
    if hashlib.sha256(data).hexdigest() != receipt["sha256"]:
        raise ValueError("changed pinned source " + str(path))
    latitude = next(row[1] for row in inventory["sites"] if row[0] == site)
    span = (
        40075016.68557849 / (1 << receipt["tile"][0]) * math.cos(math.radians(latitude))
    )
    layers = mapbox_vector_tile.decode(data, default_options={"y_coord_down": True})
    rows, omitted = [], []
    for name in ("landcover", "landuse"):
        layer = layers.get(name, {})
        extent = layer.get("extent", 4096)
        for feature_index, feature in enumerate(layer.get("features", [])):
            geometry = feature["geometry"]
            if geometry["type"] not in ("Polygon", "MultiPolygon"):
                continue
            parts = (
                geometry["coordinates"]
                if geometry["type"] == "MultiPolygon"
                else [geometry["coordinates"]]
            )
            for ordinal, rings in enumerate(parts):
                projected = [
                    (np.asarray(ring, dtype=float) / extent - 0.5) * [span, -span]
                    for ring in rings
                ]
                polygon = shapely.Polygon(projected[0], projected[1:])
                key = f"{name}:{feature.get('id', 'unknown')}:{feature_index}:{ordinal}"
                if not polygon.is_valid or polygon.area <= 0:
                    omitted.append(
                        {"id": key, "reason": shapely.is_valid_reason(polygon)}
                    )
                else:
                    rows.append(
                        {
                            "id": key,
                            "polygon": polygon,
                            "properties": feature["properties"],
                        }
                    )
    return rows, omitted


def segments(polygon):
    result = []
    for part in shapely.get_parts(polygon):
        for ring in (part.exterior, *part.interiors):
            points = np.asarray(ring.coords)
            result.extend(pairwise(points))
    edges = np.asarray(result, dtype=float)
    return edges[np.linalg.norm(edges[:, 1] - edges[:, 0], axis=1) > 0]


def resolved(rows, recipes):
    rules = {
        (rule["layer"], rule["kind"]): (index, rule["rank"])
        for index, row in enumerate(recipes["templates"])
        for rule in row["osm"]
    }
    known, unknown = [], []
    for row in rows:
        layer = "land" if row["id"].startswith("landcover:") else "sites"
        kind = row["properties"].get("class", "")
        rule = rules.get((layer, kind)) or (
            rules.get(("land", kind)) if layer == "sites" else None
        )
        if rule is None:
            unknown.append(row["id"])
        else:
            known.append(row | {"native_row": rule[0], "rank": rule[1]})
    return known, unknown


def coalesced(rows, frame):
    rank_rows = defaultdict(set)
    for row in rows:
        rank_rows[row["rank"]].add(row["native_row"])
    started = time.perf_counter()
    compiled = coalesce_regions(
        tuple(
            MaterialRegion(
                frame, row["native_row"], row["rank"], row["polygon"], (row["id"],)
            )
            for row in rows
        )
    )
    coalescing_ms = 1000 * (time.perf_counter() - started)
    by_id = {row["id"]: row for row in rows}
    output, probes = [], 0
    for region in compiled:
        members = [by_id[key] for key in region.source_ids]
        polygon = region.geometry
        sample = np.concatenate(
            [segments(row["polygon"]).mean(axis=1)[::4] for row in members]
        )
        sample = sample[
            np.linspace(0, len(sample) - 1, min(512, len(sample)), dtype=int)
        ]
        points = shapely.points(
            np.concatenate([sample + [0.031, 0.017], sample - [0.031, 0.017]])
        )
        before = np.zeros(len(points), dtype=bool)
        for row in members:
            before |= shapely.covers(row["polygon"], points)
        if not np.array_equal(before, shapely.covers(polygon, points)):
            raise ValueError("coalescing changed material-class membership")
        probes += len(points)
        output.append(
            members[0] | {"polygon": polygon, "contributors": region.source_ids}
        )
    return output, {
        "source_features": len(rows),
        "coalesced_features": len(output),
        "membership_probes": probes,
        "coalescing_ms": coalescing_ms,
        "ambiguous_ranks": sorted(
            rank for rank, classes in rank_rows.items() if len(classes) > 1
        ),
    }

"""Explicit fixed source context for the real Vienna stair; no solved elevations."""

import argparse
import hashlib
import itertools
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.places import (
    Elevation,
    decode_terrarium,
    decode_vector,
    receipt_bytes,
    tile_geography,
)
from stair_junction_layout import INPUT


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_receipt(value):
    return {
        k: v
        for k, v in value.items()
        if k in ("kind", "tile", "url", "bytes", "sha256")
    }


def parts(layer, tile, origin):
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])
    result = {}
    for feature in layer["features"]:
        shape = feature["geometry"]
        lines = (
            [shape["coordinates"]]
            if shape["type"] == "LineString"
            else shape["coordinates"]
            if shape["type"] == "MultiLineString"
            else []
        )
        for i, xy in enumerate(lines):
            if not 2 <= len(xy) <= 4096:
                raise ValueError("source context polyline point budget")
            lat, lon = tile_geography(xy, layer["extent"], tile)
            geographic = np.c_[lat, lon]
            enu = (geographic[:, ::-1] - origin[::-1]) * scale
            key = f"openfreemap:{tile[0]}/{tile[1]}/{tile[2]}:transportation:{feature['id']}:{i}"
            if key in result:
                raise ValueError("duplicate actual source part identity")
            result[key] = {
                "source_id": key,
                "feature_id": feature["id"],
                "part_index": i,
                "mvt_xy": xy,
                "points_lat_lon": geographic.tolist(),
                "xy_m": enu.tolist(),
                "properties": feature["properties"],
                "shape": shapely.LineString(enu),
                "original_feature": feature,
            }
    return result


def selected_sources(case, sources):
    keys = []
    for way in case["ways"]:
        matches = []
        for key, row in sources.items():
            if row["feature_id"] != way["source"]["feature"]["id"]:
                continue
            if row["original_feature"] != way["source"]["feature"]:
                raise ValueError(
                    "complete retained native feature differs from actual payload"
                )
            a, b = np.array(row["points_lat_lon"]), np.array(way["points_lat_lon"])
            if a.shape == b.shape and np.allclose(a, b, rtol=0, atol=1e-12):
                matches.append(key)
        if len(matches) != 1:
            raise ValueError(
                "actual native way is not exactly one retained full source part"
            )
        keys.append(matches[0])
    counts = {}
    for key in keys:
        for point in (sources[key]["mvt_xy"][0], sources[key]["mvt_xy"][-1]):
            counts[tuple(point)] = counts.get(tuple(point), 0) + 1
    frontier = {point for point, count in counts.items() if count == 1}
    extra = sorted(
        key
        for key, row in sources.items()
        if key not in keys and any(tuple(point) in frontier for point in row["mvt_xy"])
    )
    if len(extra) != 3:
        raise ValueError(
            "actual first native boundary ring changed; inspect the source"
        )
    return keys, extra


def topology(selected, sources):
    endpoints = {}
    for key in selected:
        points = sources[key]["mvt_xy"]
        for at in (0, len(points) - 1):
            endpoints.setdefault(tuple(points[at]), []).append(
                {"source_id": key, "vertex": at}
            )
    junctions = [
        {"mvt_xy": list(point), "incident": values}
        for point, values in sorted(endpoints.items())
        if len(values) > 1
    ]
    frontier = []
    for point, values in sorted(endpoints.items()):
        if len(values) != 1:
            continue
        outside = [
            key
            for key, row in sources.items()
            if key not in selected and any(tuple(p) == point for p in row["mvt_xy"])
        ]
        frontier.append(
            {
                "mvt_xy": list(point),
                "selected": values[0],
                "outside_incident_source_ids": sorted(outside),
                "status": "requires_next_context_or_chosen_ground_contact",
            }
        )
    intersections = []
    for a, b in itertools.combinations(selected, 2):
        x, y = sources[a], sources[b]
        line_a, line_b = (
            shapely.LineString(x["mvt_xy"]),
            shapely.LineString(y["mvt_xy"]),
        )
        common = line_a.intersection(line_b)
        if common.is_empty:
            continue
        shared = {tuple(x["mvt_xy"][i]) for i in (0, -1)} & {
            tuple(y["mvt_xy"][i]) for i in (0, -1)
        }
        interior = [
            list(p.coords[0])
            for p in shapely.get_parts(common)
            if p.geom_type == "Point" and tuple(p.coords[0]) not in shared
        ]
        intersections.append(
            {
                "sources": [a, b],
                "intersection_mvt_wkt": common.wkt,
                "shared_endpoints_mvt": [list(p) for p in sorted(shared)],
                "interior_points_mvt": interior,
                "scope": "Geometric evidence; levels/contact recipes remain explicitly chosen by the planner.",
            }
        )
    return junctions, frontier, intersections


def capture(args):
    if args.output.exists() or args.dem_output.exists():
        raise ValueError("explicit fresh context outputs required")
    case = json.loads(args.native_case.read_text())
    vector = json.loads(args.vector_receipt.read_text())
    dem = json.loads(args.elevation_receipt.read_text())
    layer = decode_vector(vector)["transportation"]
    sources = parts(layer, tuple(vector["tile"]), np.array(case["reference_lat_lon"]))
    original, extra = selected_sources(case, sources)
    selected = original + extra
    region = shapely.union_all([sources[k]["shape"].buffer(32) for k in selected])
    context = sorted(
        key
        for key, row in sources.items()
        if key not in selected and region.intersects(row["shape"])
    )
    if len(context) > 128:
        raise ValueError("actual local context part budget")
    selected_points = np.concatenate([np.array(sources[k]["mvt_xy"]) for k in selected])
    height = decode_terrarium(receipt_bytes(dem))
    padding = 12
    a = np.maximum(
        0,
        np.floor(
            selected_points.min(axis=0)
            / layer["extent"]
            * (np.array(height.shape[::-1]) - 1)
        ).astype(int)
        - padding,
    )
    b = np.minimum(
        np.array(height.shape[::-1]),
        np.ceil(
            selected_points.max(axis=0)
            / layer["extent"]
            * (np.array(height.shape[::-1]) - 1)
        ).astype(int)
        + padding
        + 1,
    )
    window = height[a[1] : b[1], a[0] : b[0]].astype(np.float32)
    args.dem_output.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        args.dem_output,
        heights_m=window,
        origin_pixel_xy=a,
        raster_shape=np.array(height.shape),
        tile=np.array(dem["tile"]),
    )
    elevation = Elevation([dem])
    records = []
    for key in selected + context:
        row = {
            k: v
            for k, v in sources[key].items()
            if k not in ("shape", "original_feature")
        }
        row["role"] = (
            "native"
            if key in original
            else "boundary_neighbour"
            if key in extra
            else "context_not_admitted"
        )
        if key in selected:
            ll = np.array(row["points_lat_lon"])
            row["new_dem_prior_m"] = elevation.sample(ll[:, 0], ll[:, 1]).tolist()
        if key in original:
            way = case["ways"][original.index(key)]
            row["native_lane_index"] = way["native_lane_index"]
            row["native_source_height_prior_m"] = way["source_ground_height_m"]
        records.append(row)
    features = {
        row["feature_id"]: row["original_feature"]
        for key, row in sources.items()
        if key in selected or key in context
    }
    junctions, frontier, intersections = topology(selected, sources)
    native_hash = case["ways"][0]["source"]["payload_sha256"]
    result = {
        "format": 1,
        "origin_lat_lon": case["reference_lat_lon"],
        "native_case_sha256": sha(args.native_case),
        "vector": dict(
            source_receipt(vector),
            extent=layer["extent"],
            native_payload_sha256=native_hash,
            full_payload_matches_native=vector["sha256"] == native_hash,
            native_original_features_match=True,
        ),
        "elevation": dict(
            source_receipt(dem),
            window_file=args.dem_output.name,
            window_sha256=sha(args.dem_output),
            raster_shape=list(height.shape),
            sampling="bilinear boundary-post convention; pixel=(tile-fraction)*(side-1)",
            scope="New explicit terrain reference, not proven identical to native input drape.",
        ),
        "selected_source_ids": selected,
        "sources": records,
        "original_features": [features[k] for k in sorted(features)],
        "junctions": junctions,
        "frontier": frontier,
        "intersections": intersections,
        "scope": "Seven complete candidate source parts plus actual nearby axis context within32m. Context is retained and not admitted; full original feature dictionaries preserve all parts. No geometry/height plan, chosen outer contact or native full-payload/DEM identity claim.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "selected": len(selected),
                "context": len(context),
                "features": len(features),
                "dem_window": list(window.shape),
                "fixture_sha256": sha(args.output),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (30, 30))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-case", type=Path, default=INPUT)
    for name in ("vector-receipt", "elevation-receipt", "output", "dem-output"):
        parser.add_argument("--" + name, type=Path, required=True)
    capture(parser.parse_args())

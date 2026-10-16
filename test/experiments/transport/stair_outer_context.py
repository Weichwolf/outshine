"""Full original transport-payload inventory; unplanned neighbours remain explicit."""

import json

import numpy as np
import shapely
from capture_stair_context import parts
from outshine_ref.places import decode_vector, tile_geography
from shared_bridge_geometry import corridor
from stair_context_inputs import PROFILES
from stair_junction_layout import INPUT


def area_catalog(layer, tree, domains, rows, context):
    lat0, lon0 = context["origin_lat_lon"]
    scale = np.array([111320 * np.cos(np.deg2rad(lat0)), 111132])

    def project(xy):
        lat, lon = tile_geography(xy, layer["extent"], tuple(context["vector"]["tile"]))
        return (np.c_[lon, lat] - [lon0, lat0]) * scale

    result = []
    for feature in layer["features"]:
        kind = feature["geometry"]["type"]
        if kind in ("LineString", "MultiLineString"):
            continue
        shape = shapely.transform(
            shapely.from_geojson(json.dumps(feature["geometry"])), project
        )
        overlaps = []
        if shape.is_valid:
            for i in tree.query(shape, predicate="intersects"):
                area = float(shape.intersection(domains[int(i)]).area)
                if area > 1e-7:
                    overlaps.append(
                        {
                            "candidate_source_id": rows[int(i)]["source_id"],
                            "area_m2": area,
                        }
                    )
        result.append(
            {
                "feature_id": feature["id"],
                "properties": feature["properties"],
                "geometry_type": kind,
                "source_geometry_valid": bool(shape.is_valid),
                "candidate_overlaps": sorted(
                    overlaps, key=lambda r: r["candidate_source_id"]
                ),
                "reason": "unplanned_transport_area_overlap"
                if overlaps
                else "unplanned_non_linear_transport_geometry",
            }
        )
    return sorted(result, key=lambda r: (r["feature_id"], r["geometry_type"]))


def catalog(case, rows, context):
    path = INPUT.with_name(case["origin"]["payload_file"])
    receipt = dict(context["vector"], path=str(path))
    layer = decode_vector(receipt)["transportation"]
    sources = parts(layer, tuple(receipt["tile"]), np.array(context["origin_lat_lon"]))
    selected = {r["source_id"] for r in rows}
    for row in rows:
        source = sources[row["source_id"]]
        original = row.get("original_xy_m", row["xy"])
        if (
            not np.allclose(original, source["xy_m"], rtol=0, atol=1e-8)
            or row["original"]["source"]["feature"] != source["original_feature"]
        ):
            raise ValueError(
                "selected whole source differs from exact original payload"
            )
    profiles = {
        r["kind"]: r
        for t in json.loads(PROFILES.read_text())["templates"]
        for r in t.get("osm", [])
        if r.get("layer") == "streets"
    }
    domains = [corridor(r) for r in rows]
    tree = shapely.STRtree(domains)
    unadmitted = []
    for key, source in sources.items():
        if key in selected:
            continue
        props = source["properties"]
        kind = props.get("subclass", props.get("class"))
        width = profiles.get(kind, {}).get("widthM")
        axis = source["shape"]
        shape = axis.buffer(
            width / 2 if width else 32,
            cap_style="flat",
            join_style="mitre",
            mitre_limit=2,
        )
        overlaps = []
        for i in tree.query(shape, predicate="intersects"):
            area = float(shape.intersection(domains[int(i)]).area)
            if area > 1e-7:
                overlaps.append(
                    {"candidate_source_id": rows[int(i)]["source_id"], "area_m2": area}
                )
        overlaps.sort(key=lambda r: r["candidate_source_id"])
        unadmitted.append(
            {
                "source_id": key,
                "properties": props,
                "kind": kind,
                "width_m": width,
                "reason": "unplanned_candidate_band_overlap"
                if overlaps and width
                else "unresolved_profile_potential_overlap"
                if overlaps
                else "outside_selected_local_network",
                "candidate_overlaps": overlaps,
                "overlap_scope": "Whole declared-width reference bands"
                if width
                else "32m axis search only; width/area semantics unresolved, not a collision guarantee",
            }
        )
    unadmitted.sort(key=lambda r: r["source_id"])
    unsupported = area_catalog(layer, tree, domains, rows, context)
    return {
        "payload_sha256": receipt["sha256"],
        "original_features": len(layer["features"]),
        "original_line_parts": len(sources),
        "selected_line_parts": len(selected),
        "unadmitted_sources": unadmitted,
        "unadmitted_non_line_features": unsupported,
        "scope": "Every original transport line part retained by payload+ID and explicit non-admission reason. Band broad phase is not a height/level/clearance/contact certificate. Unknown widths remain explicit. No missing input is silently replaced.",
    }

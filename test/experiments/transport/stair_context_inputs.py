"""Portable original context; native and new DEM height priors remain separate."""

import hashlib
import json
from pathlib import Path

import numpy as np
from outshine_ref.places import tile_geography
from stair_junction_layout import INPUT, inputs

FIXTURE = Path(__file__).with_name("fixtures") / "stair-context-vienna.json"
PROFILES = Path(__file__).resolve().parents[3] / "src/assets/world/vegetation.json"


def window_samples(row, raster, extent):
    pixel = (
        np.array(row["mvt_xy"], float) / extent * (raster["raster_shape"][::-1] - 1)
        - raster["origin_pixel_xy"]
    )
    base = np.floor(pixel).astype(int)
    height = raster["heights_m"]
    if np.any(base < 0) or np.any(base >= np.array(height.shape[::-1]) - 1):
        raise ValueError("selected source requires more actual DEM context")
    u, v = (pixel - base).T
    x, y = base.T
    return (1 - v) * ((1 - u) * height[y, x] + u * height[y, x + 1]) + v * (
        (1 - u) * height[y + 1, x] + u * height[y + 1, x + 1]
    )


def validate(value, raster, native):
    features = {f["id"]: f for f in value["original_features"]}
    rows = {r["source_id"]: r for r in value["sources"]}
    selected = value["selected_source_ids"]
    if (
        len(rows) != len(value["sources"])
        or len(selected) != 7
        or len(set(selected)) != 7
    ):
        raise ValueError("unique actual seven-source context required")
    for i, way in enumerate(native["ways"]):
        row = rows[selected[i]]
        feature = way["source"]["feature"]
        if (
            features[row["feature_id"]] != feature
            or row.get("native_source_height_prior_m") != way["source_ground_height_m"]
        ):
            raise ValueError(
                "actual native source/height prior was changed or conflated"
            )
    origin = np.array(value["origin_lat_lon"])
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])
    for key, row in rows.items():
        feature = features[row["feature_id"]]
        g = feature["geometry"]
        parts = [g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]
        if (
            row["mvt_xy"] != parts[row["part_index"]]
            or row["properties"] != feature["properties"]
        ):
            raise ValueError("context part lost original geometry or properties")
        lat, lon = tile_geography(
            row["mvt_xy"], value["vector"]["extent"], tuple(value["vector"]["tile"])
        )
        ll = np.c_[lat, lon]
        xy = (ll[:, ::-1] - origin[::-1]) * scale
        if not np.allclose(
            ll, row["points_lat_lon"], rtol=0, atol=1e-12
        ) or not np.allclose(xy, row["xy_m"], rtol=0, atol=1e-8):
            raise ValueError("context source frame changed")
        if key in selected:
            if (
                np.max(
                    abs(
                        window_samples(row, raster, value["vector"]["extent"])
                        - row["new_dem_prior_m"]
                    )
                )
                > 1e-7
            ):
                raise ValueError("context actual DEM samples changed")
        elif row["role"] != "context_not_admitted":
            raise ValueError("outside source lacks explicit unadmitted context role")
    matched = value["vector"]["sha256"] == value["vector"]["native_payload_sha256"]
    if value["vector"]["full_payload_matches_native"] != matched:
        raise ValueError("source payload provenance claims a false native identity")
    return rows


def load(path=FIXTURE, native_path=INPUT):
    value = json.loads(path.read_text())
    if (
        hashlib.sha256(native_path.read_bytes()).hexdigest()
        != value["native_case_sha256"]
    ):
        raise ValueError("original native case changed; recapture explicitly")
    dem_path = path.with_name(value["elevation"]["window_file"])
    if (
        hashlib.sha256(dem_path.read_bytes()).hexdigest()
        != value["elevation"]["window_sha256"]
    ):
        raise ValueError("retained DEM input window changed")
    with np.load(dem_path) as d:
        raster = dict(d)
    case, result = inputs(native_path)
    rows = validate(value, raster, case)
    for i, row in enumerate(result):
        row["native_feature_id"] = row["source_id"]
        row["source_id"] = value["selected_source_ids"][i]
    profiles = {
        r["kind"]: r
        for template in json.loads(PROFILES.read_text())["templates"]
        for r in template.get("osm", [])
        if r.get("layer") == "streets"
    }
    for key in value["selected_source_ids"][4:]:
        row = rows[key]
        recipe = profiles[row["properties"]["subclass"]]
        feature = next(
            f for f in value["original_features"] if f["id"] == row["feature_id"]
        )
        result.append(
            {
                "source_id": key,
                "xy": np.array(row["xy_m"]),
                "width_m": recipe["widthM"],
                "gradient": recipe["maxGradient"],
                "prior": np.array(row["new_dem_prior_m"]),
                "original": {
                    "source": {"feature": feature},
                    "bridge": row["properties"].get("brunnel") == "bridge",
                },
            }
        )
    keys = value["selected_source_ids"]
    junctions = []
    for node in value["junctions"]:
        first = node["incident"][0]
        row = rows[first["source_id"]]
        junctions.append(
            {
                "xy": row["xy_m"][first["vertex"]],
                "incident": [keys.index(r["source_id"]) for r in node["incident"]],
            }
        )
    return case, result, junctions, value

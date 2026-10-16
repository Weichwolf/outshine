"""Three actual stairs and the connected foot/cycle neighbour source parts."""

import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES, window_samples
from stair_context_inputs import load as context_inputs
from stair_junction_layout import INPUT

FIXTURE = Path(__file__).with_name("fixtures") / "stair-network-vienna.json"


def junctions(keys, sources, contacts, rows):
    nodes = {}
    for i, key in enumerate(keys):
        for vertex, point in enumerate(sources[key]["mvt_xy"]):
            nodes.setdefault(tuple(point), []).append((i, vertex))
    result = []
    for point, incident in sorted(nodes.items()):
        if len({i for i, _v in incident}) < 2 or not any(
            vertex in (0, len(sources[keys[i]]["mvt_xy"]) - 1) for i, vertex in incident
        ):
            continue
        i, vertex = incident[0]
        result.append(
            {
                "mvt_xy": list(point),
                "xy": sources[keys[i]]["xy_m"][vertex],
                "incident": sorted({i for i, _v in incident}),
            }
        )
    for contact in contacts:
        owners = [keys.index(k) for k in contact["source_ids"]]
        if "mvt_xy" not in contact:
            if len(owners) != 2:
                raise ValueError("a geometric contact needs two actual source bands")
            overlap = corridor(rows[owners[0]]).intersection(corridor(rows[owners[1]]))
            if overlap.geom_type != "Polygon" or overlap.area <= 1e-7:
                raise ValueError("one nonempty whole width-contact region required")
            result.append(
                {
                    "mvt_xy": None,
                    "xy": list(overlap.representative_point().coords[0]),
                    "incident": owners,
                    "chosen_overlap_wkt": overlap.wkt,
                }
            )
            continue
        matches = [r for r in result if r["mvt_xy"] == contact["mvt_xy"]]
        if len(matches) != 1:
            raise ValueError("chosen width contact lacks its original source node")
        if not set(matches[0]["incident"]) <= set(owners):
            raise ValueError("chosen width contact removed an actual source owner")
        matches[0]["incident"] = sorted(owners)
    return result


def stair_nodes(rows, nodes):
    result = []
    for i, row in enumerate(rows):
        properties = row["original"]["source"]["feature"]["properties"]
        if properties.get("subclass") != "steps":
            continue
        ends = []
        for point in row["xy"][[0, -1]]:
            matches = [
                n
                for n, node in enumerate(nodes)
                if i in node["incident"]
                and np.linalg.norm(np.array(node["xy"]) - point) < 1e-7
            ]
            if len(matches) != 1:
                raise ValueError("each stair needs two real chosen landing nodes")
            ends.append(matches[0])
        result.append((i, *ends))
    return result


def sample_xy(xy, context, raster):
    lat0, lon0 = context["origin_lat_lon"]
    lon = lon0 + xy[:, 0] / (111320 * np.cos(np.deg2rad(lat0)))
    lat = lat0 + xy[:, 1] / 111132
    z, x, y = context["vector"]["tile"]
    tile_xy = (
        np.c_[
            (lon + 180) / 360,
            (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2,
        ]
        * 2**z
    )
    extent = context["vector"]["extent"]
    mvt = (tile_xy - [x, y]) * extent
    return window_samples({"mvt_xy": mvt}, raster, extent)


def detour(rows, sources, recipe, context, raster, offset):
    row = next(r for r in rows if r["source_id"] == recipe["source_id"])
    original = row["xy"].copy()
    anchor = sources[row["source_id"]]["mvt_xy"].index(recipe["anchor_mvt"])
    if not 0 < anchor < len(original) - 1 or not 0 <= offset <= 24:
        raise ValueError("bounded detour with retained interior contact required")
    row["original_xy_m"] = original
    if offset:
        a, b = original[[anchor, -1]]
        t = (b - a) / np.linalg.norm(b - a)
        normal = np.array([-t[1], t[0]])
        knots = [a + (b - a) * f + offset * normal for f in (1 / 3, 2 / 3)]
        row["xy"] = np.vstack([original[: anchor + 1], knots, b])
        row["prior"] = sample_xy(row["xy"], context, raster)


def load(path=FIXTURE, context_path=CONTEXT, native_path=INPUT, *, offset=None):
    recipe = json.loads(path.read_text())
    if recipe["context_sha256"] != digest(context_path):
        raise ValueError("original neighbour context changed")
    case, rows, _junctions, context = context_inputs(context_path, native_path)
    keys = context["selected_source_ids"] + recipe["additional_source_ids"]
    if len(keys) != len(set(keys)) or len(keys) != 15:
        raise ValueError("unique actual fifteen-source network required")
    sources = {r["source_id"]: r for r in context["sources"]}
    features = {f["id"]: f for f in context["original_features"]}
    profiles = {
        r["kind"]: r
        for template in json.loads(PROFILES.read_text())["templates"]
        for r in template.get("osm", [])
        if r.get("layer") == "streets"
    }
    with np.load(context_path.with_name(context["elevation"]["window_file"])) as data:
        raster = dict(data)
    for key in recipe["additional_source_ids"]:
        source = sources[key]
        profile = profiles[source["properties"]["subclass"]]
        rows.append(
            {
                "source_id": key,
                "xy": np.array(source["xy_m"]),
                "width_m": profile["widthM"],
                "gradient": profile["maxGradient"],
                "prior": window_samples(source, raster, context["vector"]["extent"]),
                "original": {
                    "source": {"feature": features[source["feature_id"]]},
                    "bridge": source["properties"].get("brunnel") == "bridge",
                },
            }
        )
    detour(
        rows,
        sources,
        recipe["detour"],
        context,
        raster,
        recipe["detour"]["offset_m"] if offset is None else offset,
    )
    nodes = junctions(keys, sources, recipe["width_contacts"], rows)
    stairs = stair_nodes(rows, nodes)
    return case, rows, nodes, stairs, context, recipe

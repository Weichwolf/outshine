"""Actual whole ground neighbours, retained DEM and explicitly chosen ground joins."""

import argparse
import itertools
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from stair_context_inputs import PROFILES, window_samples
from stair_junction_layout import landings
from stair_network_inputs import FIXTURE as NETWORK
from stair_network_inputs import junctions, sample_xy, stair_nodes
from stair_network_inputs import load as network_inputs

FIXTURE = Path(__file__).with_name("fixtures") / "stair-outer-vienna.json"


def ground_contacts(rows, nodes):
    domains = [corridor(r) for r in rows]
    result = list(nodes)
    for i, j in itertools.combinations(range(len(rows)), 2):
        if rows[i]["original"]["bridge"] or rows[j]["original"]["bridge"]:
            continue
        for owner in (i, j):
            props = rows[owner]["original"]["source"]["feature"]["properties"]
            if props.get("brunnel") or props.get("layer", 0) != 0:
                raise ValueError("explicit ordinary ground semantics required")
        overlap = domains[i].intersection(domains[j])
        if overlap.area <= 1e-7:
            continue
        _xy, owners, masks = landings(rows, domains, "width", 0.75, result)
        existing = shapely.union_all(
            [
                mask
                for mask, ids in zip(masks, owners, strict=True)
                if i in ids and j in ids
            ]
        )
        for region in shapely.get_parts(overlap):
            if region.difference(existing).area <= 1e-7:
                continue
            if region.geom_type != "Polygon":
                raise ValueError("one polygon per chosen ground-width contact required")
            result.append(
                {
                    "mvt_xy": None,
                    "xy": list(region.representative_point().coords[0]),
                    "incident": [i, j],
                    "chosen_overlap_wkt": region.wkt,
                    "basis": "Chosen ordinary common-ground band contact, not an asserted source vertex or measured Earth junction.",
                }
            )
    return result


def folded_ramp(row, source, anchor_mvt, overshoot_m, return_fraction):
    original = row["original_xy_m"]
    anchor = source["mvt_xy"].index(anchor_mvt)
    a, b = original[[anchor, -1]]
    chord = b - a
    length = np.linalg.norm(chord)
    tangent = chord / length
    normal = np.array([-tangent[1], tangent[0]])
    width = row["width_m"]
    if not 0 < overshoot_m <= 24 or not 0.5 <= return_fraction <= 0.9:
        raise ValueError("bounded positive folded-ramp overshoot required")
    points = [
        a + 0.6 * chord + 6 * width * normal,
        b + overshoot_m * tangent + 6 * width * normal,
        b + overshoot_m * tangent + 2 * width * normal,
        a + return_fraction * chord + 2 * width * normal,
        b,
    ]
    row["xy"] = np.vstack([original[: anchor + 1], points])


def load(path=FIXTURE, *, folded=True, overshoot_m=None, return_fraction=None):
    recipe = json.loads(path.read_text())
    if recipe["network_sha256"] != digest(NETWORK):
        raise ValueError("original three-stair recipe changed")
    case, rows, _nodes, _stairs, context, network = network_inputs()
    sources = {r["source_id"]: r for r in context["sources"]}
    features = {r["id"]: r for r in context["original_features"]}
    dem_path = path.with_name(recipe["elevation"]["window_file"])
    if (
        digest(dem_path) != recipe["elevation"]["window_sha256"]
        or recipe["elevation"]["raw_sha256"] != context["elevation"]["sha256"]
    ):
        raise ValueError("whole-neighbour DEM source/window changed")
    with np.load(dem_path) as d:
        raster = dict(d)
    if folded:
        ramp = recipe["continuation_ramp"]
        row = next(r for r in rows if r["source_id"] == ramp["source_id"])
        folded_ramp(
            row,
            sources[row["source_id"]],
            network["detour"]["anchor_mvt"],
            ramp["overshoot_m"] if overshoot_m is None else overshoot_m,
            ramp["return_fraction"] if return_fraction is None else return_fraction,
        )
        row["prior"] = sample_xy(row["xy"], context, raster)
    profiles = {
        r["kind"]: r
        for t in json.loads(PROFILES.read_text())["templates"]
        for r in t.get("osm", [])
        if r.get("layer") == "streets"
    }
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
                    "bridge": source["properties"].get("brunnel") == "bridge",
                    "source": {"feature": features[source["feature_id"]]},
                },
            }
        )
    keys = [r["source_id"] for r in rows]
    if len(keys) != 18 or len(set(keys)) != 18:
        raise ValueError("unique eighteen-source outer-neighbour plan required")
    nodes = junctions(keys, sources, network["width_contacts"], rows)
    nodes = ground_contacts(rows, nodes)
    return case, rows, nodes, stair_nodes(rows, nodes), context, recipe


def capture(args):
    from outshine_ref.places import decode_terrarium, receipt_bytes

    if args.output.exists():
        raise ValueError("fresh explicit DEM window output required")
    recipe = json.loads(FIXTURE.read_text())
    _case, _rows, _nodes, _stairs, context, _network = network_inputs()
    receipt = json.loads(args.dem_receipt.read_text())
    if receipt["sha256"] != context["elevation"]["sha256"]:
        raise ValueError("original retained DEM receipt required")
    selected = {r["source_id"] for r in _rows} | set(recipe["additional_source_ids"])
    points = np.concatenate(
        [
            np.array(r["mvt_xy"])
            for r in context["sources"]
            if r["source_id"] in selected
        ]
    )
    height = decode_terrarium(receipt_bytes(receipt))
    pixel = points / context["vector"]["extent"] * (np.array(height.shape[::-1]) - 1)
    a = np.maximum(0, np.floor(pixel.min(axis=0)).astype(int) - 12)
    b = np.minimum(height.shape[::-1], np.ceil(pixel.max(axis=0)).astype(int) + 13)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        args.output,
        heights_m=height[a[1] : b[1], a[0] : b[0]].astype(np.float32),
        origin_pixel_xy=a,
        raster_shape=np.array(height.shape),
        tile=np.array(receipt["tile"]),
    )
    print(
        json.dumps(
            {
                "window_sha256": digest(args.output),
                "origin_pixel_xy": a.tolist(),
                "shape": (b - a)[::-1].tolist(),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dem-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    capture(parser.parse_args())

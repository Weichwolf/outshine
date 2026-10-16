"""Whole eastern cycle neighbour and two independently retained DEM windows."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from capture_stair_context import parts
from outshine_ref.places import decode_terrarium, decode_vector, receipt_bytes
from stair_context_inputs import PROFILES, window_samples
from stair_corner_geometry import apply_pads
from stair_junction_layout import INPUT
from stair_landing_pads import complete_landings
from stair_network_inputs import FIXTURE as NETWORK
from stair_network_inputs import junctions, stair_nodes
from stair_outer_inputs import FIXTURE as OUTER
from stair_outer_inputs import ground_contacts
from stair_outer_inputs import load as outer_inputs

FIXTURE = Path(__file__).with_name("fixtures") / "stair-cycle-vienna.json"
SOURCE = "openfreemap:14/8937/5678:transportation:32651280:3"


def selected_sources(recipe, available):
    keys = recipe.get("additional_source_ids", [recipe.get("source_id")])
    if (
        not isinstance(keys, list)
        or not 1 <= len(keys) <= 6
        or not all(isinstance(key, str) for key in keys)
        or len(set(keys)) != len(keys)
    ):
        raise ValueError("one to six unique whole additional sources required")
    for key in keys:
        if key not in available:
            raise ValueError("additional whole source absent from original payload")
        props = available[key]["properties"]
        if (
            props.get("brunnel")
            or props.get("layer", 0) != 0
            or props.get("level", 0) != 0
            or props.get("indoor")
        ):
            raise ValueError(
                "ordinary exterior ground recipe cannot admit indoor/level sources"
            )
    return keys


def original_inputs():
    case, rows, _nodes, _stairs, context, _recipe = outer_inputs()
    receipt = dict(
        context["vector"], path=str(INPUT.with_name(case["origin"]["payload_file"]))
    )
    layer = decode_vector(receipt)["transportation"]
    sources = parts(layer, tuple(receipt["tile"]), np.array(context["origin_lat_lon"]))
    return case, rows, context, sources


def vector_xy(xy, context):
    lat0, lon0 = context["origin_lat_lon"]
    latitude = lat0 + xy[:, 1] / 111132
    longitude = lon0 + xy[:, 0] / (111320 * np.cos(np.deg2rad(lat0)))
    world = np.c_[
        (longitude + 180) / 360,
        (1 - np.arcsinh(np.tan(np.deg2rad(latitude))) / np.pi) / 2,
    ]
    zoom, x, y = context["vector"]["tile"]
    return (world * 2**zoom - [x, y]) * context["vector"]["extent"]


def sampler(path, recipe, context):
    zoom, x, y = context["vector"]["tile"]
    allowed = {(zoom, x + dx, y + dy) for dx in (0, 1) for dy in (0, 1)}
    if not 1 <= len(recipe["elevation"]) <= 4:
        raise ValueError("one to four explicit adjacent DEM windows required")
    windows = {}
    for row in recipe["elevation"]:
        tile = tuple(row["tile"])
        if tile not in allowed or tile in windows:
            raise ValueError("unique adjacent DEM windows in the source frame required")
        source = path.with_name(row["window_file"])
        if digest(source) != row["window_sha256"]:
            raise ValueError("cycle DEM window changed")
        windows[tile] = dict(np.load(source))

    def sample(xy):
        points = vector_xy(np.asarray(xy), context)
        zoom, x, y = context["vector"]["tile"]
        extent = context["vector"]["extent"]
        cells = np.floor(points / extent).astype(int)
        result = np.full(len(points), np.nan)
        for offset in np.unique(cells, axis=0):
            key = zoom, x + int(offset[0]), y + int(offset[1])
            if key not in windows:
                raise ValueError("cycle source extends beyond declared DEM tiles")
            selected = np.all(cells == offset, axis=1)
            result[selected] = window_samples(
                {"mvt_xy": points[selected] - offset * extent}, windows[key], extent
            )
        if not np.isfinite(result).all():
            raise ValueError("cycle source has unresolved actual DEM priors")
        return result

    return sample


def load(path=FIXTURE, *, band_join=True):
    recipe = json.loads(path.read_text())
    if recipe["outer_sha256"] != digest(OUTER):
        raise ValueError("cycle extension lost its original outer identity")
    case, rows, context, sources = original_inputs()
    added = selected_sources(recipe, sources)
    sample = sampler(path, recipe, context)
    profiles = {
        r["kind"]: r
        for t in json.loads(PROFILES.read_text())["templates"]
        for r in t.get("osm", [])
        if r.get("layer") == "streets"
    }
    for key in added:
        source = sources[key]
        profile = profiles[source["properties"]["subclass"]]
        rows.append(
            {
                "source_id": key,
                "xy": np.array(source["xy_m"]),
                "prior": sample(np.array(source["xy_m"])),
                "width_m": profile["widthM"],
                "gradient": profile["maxGradient"],
                "original": {
                    "bridge": False,
                    "source": {"feature": source["original_feature"]},
                },
            }
        )
    keys = [r["source_id"] for r in rows]
    if len(keys) != 18 + len(added) or len(set(keys)) != len(keys):
        raise ValueError("unique whole source extension required")
    contacts = json.loads(NETWORK.read_text())["width_contacts"]
    nodes = junctions(keys, sources, contacts, rows)
    nodes = ground_contacts(rows, nodes)
    if "junction_pads" in recipe:
        context["junction_pads"] = apply_pads(rows, nodes, recipe["junction_pads"])
    if not band_join:
        nodes = [
            node
            for node in nodes
            if not (18 in node["incident"] and node["mvt_xy"] is None)
        ]
    stairs = stair_nodes(rows, nodes)
    if "complete_chosen_landings" in recipe:
        settings = recipe["complete_chosen_landings"]
        pads, ledger = complete_landings(rows, nodes, stairs, **settings)
        context.update(landing_pads=pads, landing_pad_selection=ledger)
    return case, rows, nodes, stairs, context, sample


def capture(args):
    if args.output.exists():
        raise ValueError("fresh explicit cycle recipe output required")
    _case, rows, context, sources = original_inputs()
    added = selected_sources({"additional_source_ids": args.sources}, sources)
    points = np.vstack([*[r["xy"] for r in rows], *[sources[k]["xy_m"] for k in added]])
    low, high = points.min(0) - 32, points.max(0) + 32
    mvt = vector_xy(np.array([low, high]), context)
    extent = context["vector"]["extent"]
    zoom, x, y = context["vector"]["tile"]
    if not 1 <= len(args.receipts) <= 4:
        raise ValueError("one to four explicit adjacent DEM receipts required")
    receipts = [json.loads(p.read_text()) for p in args.receipts]
    tiles = [tuple(r["tile"]) for r in receipts]
    allowed = {(zoom, x + dx, y + dy) for dx in (0, 1) for dy in (0, 1)}
    if len(set(tiles)) != len(tiles) or not set(tiles) <= allowed:
        raise ValueError("unique adjacent DEM receipts in the source frame required")
    multiple_rows = len({t[2] for t in tiles}) > 1
    records = []
    for receipt in receipts:
        tile = receipt["tile"]
        if tile == [zoom, x, y] and receipt["sha256"] != context["elevation"]["sha256"]:
            raise ValueError("original DEM source snapshot changed")
        height = decode_terrarium(receipt_bytes(receipt))
        pixels = (mvt / extent - (np.array(tile[1:]) - [x, y])) * (
            np.array(height.shape[::-1]) - 1
        )
        lo = np.maximum(0, np.floor(pixels.min(0)).astype(int) - 2)
        hi = np.minimum(height.shape[::-1], np.ceil(pixels.max(0)).astype(int) + 3)
        if np.any(hi - lo < 2):
            raise ValueError("empty requested cycle DEM crop")
        name = f"{tile[1]}-{tile[2]}" if multiple_rows else str(tile[1])
        target = args.output.with_name(args.output.stem + f"-dem-{name}.npz")
        if target.exists():
            raise ValueError("fresh cycle DEM window required")
        np.savez_compressed(
            target,
            heights_m=height[lo[1] : hi[1], lo[0] : hi[0]].astype(np.float32),
            origin_pixel_xy=lo,
            raster_shape=np.array(height.shape),
        )
        records.append(
            {
                "tile": tile,
                "url": receipt["url"],
                "raw_sha256": receipt["sha256"],
                "raw_bytes": receipt["bytes"],
                "window_file": target.name,
                "window_sha256": digest(target),
            }
        )
    recipe = {
        "format": 1,
        "outer_sha256": digest(OUTER),
        "source_id": SOURCE,
        "elevation": records,
        "scope": "Whole original neighbour; planar contacts are explicit choices. DEM priors select their actual tile without extrapolation. No measured deck, indoor or cross-tile vector continuity claim.",
    }
    if added != [SOURCE]:
        recipe.pop("source_id")
        recipe["additional_source_ids"] = added
    args.output.write_text(json.dumps(recipe, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipts", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sources", nargs="+", default=[SOURCE])
    capture(parser.parse_args())

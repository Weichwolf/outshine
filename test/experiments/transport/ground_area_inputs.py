"""Fixed whole source input and a small held terrain query, without private roots."""

import json
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from ground_area_recipe import AreaRecipe
from outshine_ref.contracts import Mesh
from outshine_ref.places import decode_vector, tile_geography
from stair_module_inputs import reference_profiles
from stair_patch_inputs import frame_for

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = Path(__file__).with_name("fixtures") / "ground-area-pedestrian-field-v1.json"


def load(path=FIXTURE):
    if path.stat().st_size > 1024**2:
        raise ValueError("Bounded complete area input required")
    case = json.loads(path.read_text())
    if case["version"] != 1:
        raise ValueError("Known whole-source area input schema required")
    source = case["source"]
    receipt = dict(source["receipt"], path=str(ROOT / source["payload_relative_path"]))
    layer = decode_vector(receipt)["transportation"]
    if (
        layer["extent"] != source["extent"]
        or layer["features"][source["feature_ordinal"]] != source["feature"]
    ):
        raise ValueError("Actual complete original feature binding changed")
    feature = source["feature"]
    props = feature["properties"]
    if (
        feature["geometry"]["type"] != "Polygon"
        or props.get("class") != "path"
        or props.get("subclass") not in ("pedestrian", "footway")
        or props.get("layer", 0) != 0
        or props.get("brunnel")
        or props.get("indoor")
    ):
        raise ValueError("First recipe requires one whole ground pedestrian polygon")
    profile = reference_profiles()[props["subclass"]]
    if profile != case["source_profile"]:
        raise ValueError("Bound actual source profile changed")
    origin = np.asarray(case["projection"]["origin_lat_lon"], float)
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])
    if (
        scale.tolist() != case["projection"]["longitude_latitude_metres_per_degree"]
        or frame_for(origin).identifier != case["frame"]
    ):
        raise ValueError("Same complete old terrain/source metric frame required")
    rings = []
    for ring in feature["geometry"]["coordinates"]:
        lat, lon = tile_geography(ring, layer["extent"], receipt["tile"])
        rings.append((np.c_[lon, lat] - origin[::-1]) * scale)
    row = case["terrain"]
    if (
        not 3 <= len(row["vertices"]) <= 1536
        or not 1 <= len(row["triangles"]) <= 512
        or len(row["ground"]) != len(row["triangles"])
        or len(row["constructed"]) != len(row["triangles"])
        or any(type(v) is not bool or v for v in row["ground"])
    ):
        raise ValueError(
            "Complete bounded free terrain, not retained transport contact, required"
        )
    mesh = Mesh(
        np.asarray(row["vertices"], float), np.asarray(row["triangles"], np.uint32)
    )
    return SimpleNamespace(
        case=case,
        original=shapely.Polygon(rings[0], rings[1:]),
        surface=mesh,
        recipe=AreaRecipe(maximum_grade=profile["maxGradient"]),
        clearance_m=profile["clearanceM"],
    )

"""Pinned original Wiener sources and current transport terrain for real consumers."""

import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.places import decode_vector, tile_geography
from stair_local_replay import load_chain
from stair_module_inputs import INPUT
from stair_occupancy_inputs import actual_sources
from stair_occupancy_verify import read
from stair_patch_inputs import whole


def inputs(products):
    report = json.loads((products / "comparison.json").read_text())
    receipt = json.loads((products / "verification.json").read_text())
    if not receipt["valid_saved_occupancy"] or receipt[
        "products_report_sha256"
    ] != digest(products / "comparison.json"):
        raise ValueError("verified complete transport products required")
    view, assets, supports, contacts, manifest = load_chain(
        Path(report["resident"]), [Path(p) for p in report["prior_modules"]]
    )
    if tuple(manifest["source_ids"]) != tuple(report["source_ids"]):
        raise ValueError("transport source ownership changed")
    context, *_ = actual_sources()
    origin = np.array(context["origin_lat_lon"])
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])

    def project(xy, extent):
        lat, lon = tile_geography(xy, extent, tuple(context["vector"]["tile"]))
        return (np.c_[lon, lat] - origin[::-1]) * scale

    native = json.loads(INPUT.read_text())
    path = INPUT.with_name(native["origin"]["payload_file"])
    layers = decode_vector(dict(context["vector"], path=str(path)))
    rows, _ = read(products / "volumes.json", report)
    terrain = whole(view).mesh
    return {
        "report": report,
        "context": context,
        "layers": layers,
        "project": project,
        "terrain": terrain,
        "sampler": SurfaceSampler(terrain),
        "reservations": rows,
        "frame": view.patches[0].frame,
        "assets": assets,
        "supports": supports,
        "contacts": contacts,
    }


def source_rows(data, layer_name):
    layer = data["layers"][layer_name]
    result, rejected = [], []
    for ordinal, feature in enumerate(layer["features"]):
        identity = (
            f"openfreemap:14/8937/5678:{layer_name}:"
            + str(feature.get("id"))
            + f":{ordinal}"
        )
        original = shapely.from_geojson(json.dumps(feature["geometry"]))
        shape = shapely.transform(
            original, lambda xy: data["project"](xy, layer["extent"])
        )
        record = {
            "source_id": identity,
            "original_feature": feature,
            "projected_original_wkb": shape.wkb_hex,
        }
        if (
            shape.is_empty
            or not shape.is_valid
            or shape.geom_type not in ("Polygon", "MultiPolygon")
        ):
            rejected.append(record | {"reason": "invalid_or_nonpolygon_source"})
            continue
        raw_parts = shapely.get_parts(original)
        for part, polygon in enumerate(shapely.get_parts(shape)):
            xy = shapely.get_coordinates(raw_parts[part])
            result.append(
                record
                | {
                    "id": identity + f":part:{part}",
                    "part": part,
                    "layer": layer_name,
                    "polygon": polygon,
                    "properties": dict(feature["properties"]),
                    "clipped": bool(np.any(xy <= 0) or np.any(xy >= layer["extent"])),
                }
            )
    if len(result) > 1024:
        raise ValueError("explicit source-part budget exceeded")
    return result, rejected


def root_contact(data, polygon):
    xy = np.array(polygon.representative_point().coords)
    contact = data["sampler"].sample(xy)
    if contact.reasons != ("",):
        raise ValueError("current transport terrain contact: " + contact.reasons[0])
    return {
        "world_frame": data["frame"].identifier,
        "root_enu_m": contact.points_enu_m[0].tolist(),
        "terrain_face": int(contact.face_indices[0]),
        "terrain_normal": contact.normals_enu[0].tolist(),
        "adapter": "transport-flat-recipe-to-current-terrain/1",
        "transform": "world XY retained; world Z = recipe Z + sampled root Z",
        "scope": "Single root anchor; no whole-footprint foundation/slope fit",
    }

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
from stair_occupancy_verify import check_export, read
from stair_patch_inputs import whole


def _baseline_is_prefix(report, state):
    if state is None:
        return False
    manifest = state[4]
    if (
        not {"root_resident_sha256", "parent_reports", "source_ids", "frame"}
        <= report.keys()
        or not {
            "root_resident_sha256",
            "parent_reports",
            "source_ids",
            "frame",
            "origin_lat_lon",
            "support_sha256",
        }
        <= manifest.keys()
    ):
        return False
    resident = Path(report["resident"])
    prior = tuple(Path(p).resolve() for p in report["prior_modules"])
    if len(prior) > 32 or len(set(prior)) != len(prior):
        raise ValueError("bounded unique ordered local-module inputs required")
    if (
        digest(resident / "resident.json") != report["root_resident_sha256"]
        or report["root_resident_sha256"] != manifest["root_resident_sha256"]
    ):
        raise ValueError("baseline root differs from verified current snapshot")
    parents = [digest(p / "comparison.json") for p in prior]
    if (
        parents != report["parent_reports"]
        or not isinstance(manifest["parent_reports"], (list, tuple))
        or parents != list(manifest["parent_reports"][: len(parents)])
    ):
        raise ValueError("baseline parents are not the verified snapshot prefix")
    root = json.loads((resident / "resident.json").read_text())
    prefix = (
        json.loads((prior[-1] / "comparison.json").read_text()).get(
            "candidate_source_ids"
        )
        if prior
        else root.get("source_ids")
    )
    if (
        not isinstance(prefix, list)
        or prefix != report["source_ids"]
        or prefix != manifest["source_ids"][: len(prefix)]
        or root.get("frame") != report["frame"]
        or root.get("frame") != manifest["frame"]
        or root.get("frame") != state[0].patches[0].frame.identifier
        or root.get("origin_lat_lon") != manifest["origin_lat_lon"]
        or root.get("support_sha256") != manifest["support_sha256"]
    ):
        raise ValueError("whole baseline source/frame/origin/support prefix differs")
    return True


def _inputs(products, current_state=None):
    report = json.loads((products / "comparison.json").read_text())
    receipt = json.loads((products / "verification.json").read_text())
    if not receipt["valid_saved_occupancy"] or receipt[
        "products_report_sha256"
    ] != digest(products / "comparison.json"):
        raise ValueError("verified complete transport products required")
    if _baseline_is_prefix(report, current_state):
        check_export(products / "volumes.json", report)
        rows = None
        view, assets, supports, contacts, _manifest = current_state
    else:
        view, assets, supports, contacts, manifest = load_chain(
            Path(report["resident"]), [Path(p) for p in report["prior_modules"]]
        )
        if tuple(manifest["source_ids"]) != tuple(report["source_ids"]):
            raise ValueError("transport source ownership changed")
        rows, _ = read(products / "volumes.json", report)
        if current_state is not None:
            view, assets, supports, contacts, _manifest = current_state
    context, *_ = actual_sources()
    origin = np.array(context["origin_lat_lon"])
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])

    def project(xy, extent):
        lat, lon = tile_geography(xy, extent, tuple(context["vector"]["tile"]))
        return (np.c_[lon, lat] - origin[::-1]) * scale

    native = json.loads(INPUT.read_text())
    path = INPUT.with_name(native["origin"]["payload_file"])
    layers = decode_vector(dict(context["vector"], path=str(path)))
    terrain = whole(view).mesh
    return {
        "report": report,
        "context": context,
        "layers": layers,
        "project": project,
        "terrain": terrain,
        "terrain_patches": view,
        "sampler": SurfaceSampler(terrain),
        "reservations": rows,
        "frame": view.patches[0].frame,
        "assets": assets,
        "supports": supports,
        "contacts": contacts,
    }


def inputs(products):
    return _inputs(products)


def candidate_inputs(baseline, state):
    data = _inputs(baseline, state)
    del data["reservations"], data["report"]
    return data


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
            anchor = np.asarray(raw_parts[part].representative_point().coords)[0]
            grid = data["project"](
                anchor + np.array([[0, 0], [1, 0], [0, 1]]), layer["extent"]
            )
            result.append(
                record
                | {
                    "id": identity + f":part:{part}",
                    "part": part,
                    "layer": layer_name,
                    "polygon": polygon,
                    "properties": dict(feature["properties"]),
                    "clipped": bool(np.any(xy <= 0) or np.any(xy >= layer["extent"])),
                    "coordinate_step_m": float(
                        np.linalg.norm(grid[1:] - grid[0], axis=1).max()
                    ),
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

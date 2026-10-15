"""Bind historical transport metric recipes to the shared native Place frame."""

import json
import math
from pathlib import Path

import numpy as np
from outshine_ref.places import Elevation, read_camera, tile_geography

ROOT = Path(__file__).resolve().parents[3]
GIRTH_M = 40075016.68557849


def mercator(latitude, longitude):
    return np.array(
        [
            longitude / 360 * GIRTH_M,
            math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi),
        ]
    )


def plan_geography(origin, xy):
    origin = np.asarray(origin, dtype=float)
    if (
        origin.shape != (2,)
        or not np.isfinite(origin).all()
        or abs(origin[0]) >= 85.05112878
        or abs(origin[1]) > 180
    ):
        raise ValueError("explicit historical Web-Mercator origin required")
    xy = np.asarray(xy, dtype=float)
    if xy.ndim != 2 or xy.shape[1] != 2 or not np.isfinite(xy).all():
        raise ValueError("finite historical metric XY pairs required")
    scaled = xy / math.cos(math.radians(origin[0])) + mercator(*origin)
    world = scaled / [GIRTH_M, -GIRTH_M] + 0.5
    return tile_geography(world, 1, (0, 0, 0))


def plan_vertices(camera, origin, vertices):
    vertices = np.asarray(vertices, dtype=float)
    if vertices.ndim != 2 or vertices.shape[1] != 3 or not np.isfinite(vertices).all():
        raise ValueError("finite historical XY/height triples required")
    latitude, longitude = plan_geography(origin, vertices[:, :2])
    return camera.local(latitude, longitude, vertices[:, 2])


def place_camera(name, inventory):
    path = ROOT / "src/assets/places" / (name + ".scenario")
    camera = read_camera(path)
    cases = json.loads(Path(inventory).read_text())["cases"]
    row = cases[name]
    if row["scenario_sha256"] != camera.scenario_sha256:
        raise ValueError("elevation selection belongs to another camera declaration")
    ground, zoom = None, None
    if camera.samples_height:
        elevation = Elevation(row["receipts"])
        ground, zoom = elevation.sample_with_zoom(
            camera.latitude_deg, camera.longitude_deg
        )
        ground, zoom = float(ground), int(zoom)
    eye = camera.eye(ground)
    basis = camera.basis
    recipes = json.loads(
        (ROOT / "test/experiments/transport/reference/place-sources.json").read_text()
    )
    recipe = next(row for row in recipes["cases"] if row["place"] == name)
    view = {
        "projection": "perspective",
        "eye": eye.tolist(),
        "target": (eye - basis[:, 2] * 1000).tolist(),
        "camera_basis": basis.tolist(),
        "vertical_fov_deg": camera.vertical_fov_deg,
        "clip_near_m": 0.05,
        "clip_far_m": camera.sight_m,
        "profile": list(camera.native_profile),
    }
    proof = {
        "scenario": str(path.relative_to(ROOT)),
        "scenario_sha256": camera.scenario_sha256,
        "time_utc": camera.time_utc,
        "geographic_pose": {
            "latitude_deg": camera.latitude_deg,
            "longitude_deg": camera.longitude_deg,
            "declared_height_m": camera.declared_height_m,
            "samples_height": camera.samples_height,
            "bearing_deg": camera.bearing_deg,
            "pitch_deg": camera.pitch_deg,
        },
        "lighting_recipe": "Fixed Workbench studio inspection; declared clock retained, no native sun/shading claim.",
        "local_origin_lat_lon": [
            camera.origin_latitude_deg,
            camera.origin_longitude_deg,
        ],
        "camera_dem_height_m": ground,
        "camera_dem_zoom": zoom,
        "dem_receipts": row["receipts"],
        "local_flat_datum_m": recipe["flat_recipe_datum_m"],
        "legacy_plan_origin_lat_lon": recipe["origin_lat_lon"],
        "flat_recipe_dem": recipe["flat_recipe_dem"],
        "height_sampling": "Shared highest-declared available Terrarium sample; absolute cameras require no terrain sample. Historical flat recipe datum remains unchanged; no geoid conversion.",
        "render_transform": "Inverse historical scaled Mercator, then shared WGS84 ECEF/ENU at the scenario origin; original metric plans and indices unchanged.",
    }
    return camera, view, proof

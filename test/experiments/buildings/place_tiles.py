"""Decode pinned Place vectors into one geodetic ENU frame with actual DEM base heights."""

import json

import numpy as np
import shapely
from context import attach
from outshine_ref.places import decode_vector
from outshine_ref.places import tile_geography as geography


def decode(reference, receipt, elevation=None):
    tile = tuple(receipt["tile"])
    layers = decode_vector(receipt)

    def project(points, extent):
        latitude, longitude = geography(points, extent, tile)
        return reference.local(latitude, longitude, 0.0)[:, :2]

    rows, invalid = [], []
    for name in ("building", "landuse"):
        layer = layers.get(name, {})
        extent = layer.get("extent", 4096)
        for ordinal, feature in enumerate(layer.get("features", ())):
            original = shapely.from_geojson(json.dumps(feature["geometry"]))
            for part, piece in enumerate(shapely.get_parts(original)):
                identifier = f"{reference.name}/{tile[0]}/{tile[1]}/{tile[2]}/{name}/{feature.get('id', 'unknown')}/{ordinal}/{part}"
                if piece.geom_type != "Polygon" or not piece.is_valid or piece.is_empty:
                    invalid.append(
                        {
                            "id": identifier,
                            "layer": name,
                            "reason": "Invalid or unsupported provider polygon",
                        }
                    )
                    continue
                polygon = shapely.transform(
                    piece, lambda xy, size=extent: project(xy, size)
                )
                coordinates = shapely.get_coordinates(piece)
                centre = np.asarray(piece.representative_point().coords)
                latitude, longitude = geography(centre, extent, tile)
                ground, base = None, 0.0
                if elevation is not None:
                    ground = float(elevation.sample(latitude, longitude)[0])
                    base = float(reference.local(latitude, longitude, ground)[0, 2])
                step_m = (
                    40075016.68557849
                    * np.cos(np.deg2rad(reference.latitude_deg))
                    / ((1 << tile[0]) * extent)
                )
                rows.append(
                    {
                        "id": identifier,
                        "layer": name,
                        "polygon": polygon,
                        "properties": feature["properties"].copy(),
                        "coordinate_step_m": float(step_m),
                        "clipped": bool(
                            np.any(coordinates <= 0) or np.any(coordinates >= extent)
                        ),
                        "ground_asl_m": ground,
                        "placement_base_enu_m": base,
                        "placement_origin": "flat shared building plane; no DEM"
                        if elevation is None
                        else "sampled DEM placement",
                        "source_sha256": receipt["sha256"],
                    }
                )
    context = attach(
        [row for row in rows if row["layer"] == "building"],
        layers,
        project,
        receipt["sha256"],
    )
    return rows, {
        "receipt": receipt,
        "invalid": invalid,
        "context": context,
        "frame": "WGS84 ENU at the native scenario origin, ellipsoid height zero",
        "ground": "Flat shared building plane; no DEM"
        if elevation is None
        else "Highest acquired Terrarium sample at each representative source point; not a native foundation/terrain-modifier proof",
    }

"""Normalize coarse source evidence; GEOS is used only while generating assets."""

import gzip
import json
import math
from pathlib import Path

import shapely
from shapely.affinity import affine_transform

URBAN = {
    "residential",
    "commercial",
    "industrial",
    "retail",
    "suburb",
    "quarter",
    "neighbourhood",
    "school",
    "university",
    "hospital",
    "college",
    "kindergarten",
    "library",
    "military",
    "garages",
    "bus_station",
}
OPEN = {
    "park",
    "cemetery",
    "pitch",
    "playground",
    "track",
    "stadium",
    "parking",
    "railway",
}
ROAD_HALF_WIDTH_M = {
    "motorway": 14,
    "trunk": 12,
    "primary": 9,
    "secondary": 7,
    "tertiary": 6,
    "rail": 4,
}
RADIUS = 6378137.0


def load_snapshot(path):
    path = Path(path)
    payload = (
        gzip.decompress(path.read_bytes())
        if path.suffix == ".gz"
        else path.read_bytes()
    )
    document = json.loads(payload)
    z, x, y = document["tile"]
    if not 0 <= z <= 22 or not 0 <= x < 2**z or not 0 <= y < 2**z:
        raise ValueError("bounded Web-Mercator source tile required")
    return document


def domains(document):
    z, x, y = document["tile"]
    span = 2 * math.pi * RADIUS / 2**z
    left, top = x * span - math.pi * RADIUS, math.pi * RADIUS - y * span
    latitude = math.atan(math.sinh((top - span / 2) / RADIUS))
    scale = math.cos(latitude)
    urban, excluded, records, urban_rows = [], [], [], []
    for layer, data in document["layers"].items():
        step = span / data["extent"]
        for ordinal, feature in enumerate(data["features"]):
            props = feature["properties"]
            kind = props.get("class", "")
            record = {
                "layer": layer,
                "ordinal": ordinal,
                "id": feature.get("id"),
                "properties": props,
                "role": "unclassified",
            }
            geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
            geometry = affine_transform(geometry, [step, 0, 0, -step, left, top])
            polygon = geometry.geom_type in {"Polygon", "MultiPolygon"}
            if geometry.is_empty or not geometry.is_valid:
                record["role"] = "invalid_or_empty_requires_other_evidence"
            elif polygon and layer == "landuse" and kind in URBAN:
                record["role"] = "urban_evidence_inferred_geometry"
                urban.append(geometry)
                urban_rows.append(len(records))
            elif polygon and (layer in {"water", "landcover"} or kind in OPEN):
                record["role"] = "explicit_open_space_exclusion"
                excluded.append(geometry)
            elif layer == "transportation" and geometry.geom_type in {
                "LineString",
                "MultiLineString",
            }:
                road = kind.removesuffix("_construction")
                if road in ROAD_HALF_WIDTH_M:
                    record["role"] = "inferred_corridor_around_source_line"
                    excluded.append(
                        shapely.buffer(geometry, ROAD_HALF_WIDTH_M[road] / scale)
                    )
            records.append(record)
    urban_union = shapely.union_all(urban)
    allowed = shapely.difference(urban_union, shapely.union_all(excluded))
    if not allowed.is_valid:
        raise ValueError("source union produced invalid permitted domain")
    return {
        "allowed": allowed,
        "urban": urban,
        "urban_rows": urban_rows,
        "records": records,
        "tile_bounds": [left, top - span, left + span, top],
        "origin_scale": [left, top - span, scale],
        "urban_area_projected_m2": float(urban_union.area),
        "allowed_area_projected_m2": float(allowed.area),
        "scope": "Coarse evidence and inferred setbacks, not observed building footprints or height bounds. Outside urban evidence is unknown, never World Empty.",
    }

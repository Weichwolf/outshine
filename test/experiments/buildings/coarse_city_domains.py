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


def source_geometries(document, owner_clip):
    contexts = [document, *document.get("parents", [])]
    if len(contexts) > 16:
        raise ValueError("bounded source context count required")
    has_transport = bool(document["layers"].get("transportation", {}).get("features"))
    for context in contexts:
        z, x, y = context["tile"]
        span = 2 * math.pi * RADIUS / 2**z
        left, top = x * span - math.pi * RADIUS, math.pi * RADIUS - y * span
        for layer, data in context["layers"].items():
            step = span / data["extent"]
            for ordinal, feature in enumerate(data["features"]):
                record = {
                    "tile": context["tile"],
                    "source_sha256": context["source"]["sha256"],
                    "layer": layer,
                    "ordinal": ordinal,
                    "id": feature.get("id"),
                    "properties": feature["properties"],
                    "role": "unclassified",
                }
                geometry = None
                if (
                    context is not document
                    and layer == "transportation"
                    and has_transport
                ):
                    record["role"] = "superseded_by_finer_transport_evidence"
                else:
                    geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
                    if geometry.is_empty or not geometry.is_valid:
                        record["role"] = "invalid_or_empty_requires_other_evidence"
                        geometry = None
                    else:
                        geometry = affine_transform(
                            geometry, [step, 0, 0, -step, left, top]
                        )
                        geometry = shapely.intersection(geometry, owner_clip)
                        if geometry.is_empty:
                            record["role"] = "outside_requested_owner_context"
                            geometry = None
                yield geometry, record


def domains(document):
    z, x, y = document["tile"]
    span = 2 * math.pi * RADIUS / 2**z
    left, top = x * span - math.pi * RADIUS, math.pi * RADIUS - y * span
    latitude = math.atan(math.sinh((top - span / 2) / RADIUS))
    scale = math.cos(latitude)
    urban, excluded, records, urban_rows = [], [], [], []
    owner = shapely.box(left, top - span, left + span, top)
    guard = shapely.box(left - 512, top - span - 512, left + span + 512, top + 512)
    for geometry, record in source_geometries(document, guard):
        if geometry is not None:
            layer, props = record["layer"], record["properties"]
            kind = props.get("class", "")
            polygon = geometry.geom_type in {"Polygon", "MultiPolygon"}
            if polygon and layer == "landuse" and kind in URBAN:
                record["role"] = "urban_evidence_inferred_geometry"
                urban.append(geometry)
                urban_rows.append(len(records))
            elif polygon and (
                layer in {"water", "landcover", "aeroway", "park"} or kind in OPEN
            ):
                record["role"] = "explicit_open_space_exclusion"
                excluded.append(geometry)
            elif layer in {
                "transportation",
                "waterway",
                "aeroway",
            } and geometry.geom_type in {
                "LineString",
                "MultiLineString",
            }:
                road = kind.removesuffix("_construction")
                widths = (
                    ROAD_HALF_WIDTH_M
                    if layer == "transportation"
                    else {"river": 5, "stream": 1.5, "canal": 4, "drain": 1, "ditch": 1}
                    if layer == "waterway"
                    else {"runway": 25, "taxiway": 8}
                )
                if props.get("brunnel") == "tunnel":
                    record["role"] = "below_grade_hint_not_surface_clearance"
                elif road in widths:
                    record["role"] = "inferred_corridor_around_source_line"
                    excluded.append(shapely.buffer(geometry, widths[road] / scale))
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
        "urban_area_projected_m2": float(shapely.intersection(urban_union, owner).area),
        "allowed_area_projected_m2": float(shapely.intersection(allowed, owner).area),
        "owner_area_projected_m2": float(owner.area),
        "scope": "Coarse evidence and inferred setbacks, not observed building footprints or height bounds. Outside urban evidence is unknown, never World Empty.",
    }

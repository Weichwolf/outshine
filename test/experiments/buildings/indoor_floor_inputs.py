"""Pinned provider volumes and original indoor requests in one explicit ENU frame."""

import hashlib
import json
import lzma
from dataclasses import dataclass
from pathlib import Path

import mapbox_vector_tile
import numpy as np
import shapely
from capture_height_targets import world_coordinates
from indoor_floor_plan import FloorRequest, Volume
from outshine_ref.contracts import LocalFrame
from outshine_ref.places import Elevation, tile_geography
from outshine_ref.places.camera import axes, ecef


@dataclass
class FloorInputs:
    origin: tuple
    frame: LocalFrame
    parts: tuple
    requests: tuple
    elevation: Elevation
    metadata: dict
    constraints: tuple = ()

    def __post_init__(self):
        self._rotation = axes(*self.origin)
        self._anchor = ecef(*self.origin, 0.0)

    def local(self, latitude, longitude, height=0.0):
        return (ecef(latitude, longitude, height) - self._anchor) @ self._rotation.T

    def ground(self, parts):
        longitude, latitude = np.asarray([p.anchor_lon_lat for p in parts]).T
        height = self.elevation.sample(latitude, longitude)
        return self.local(latitude, longitude, height)[:, 2]


def source_bytes(root, row):
    data = (root / row["archive"]).read_bytes()
    if row["kind"] == "vector":
        data = lzma.decompress(data)
    if len(data) != row["bytes"] or hashlib.sha256(data).hexdigest() != row["sha256"]:
        raise ValueError("Pinned indoor-floor source changed")
    return data


def building_parts(inputs, row, layers):
    layer = layers.get("building", {})
    extent = layer.get("extent", 4096)
    sample = np.array(
        [
            [extent / 2, extent / 2],
            [extent / 2 + 1, extent / 2],
            [extent / 2, extent / 2 + 1],
        ]
    )
    latitude, longitude = tile_geography(sample, extent, row["tile"])
    metric = inputs.local(latitude, longitude)[:, :2]
    coordinate_step = float(np.linalg.norm(metric[1:] - metric[0], axis=1).min())
    result, rejected = [], []
    for ordinal, feature in enumerate(layer.get("features", ())):
        raw = shapely.from_geojson(json.dumps(feature["geometry"]))
        for part, polygon in enumerate(shapely.get_parts(raw)):
            identifier = (
                f"{row['sha256']}:building:{feature.get('id')}:{ordinal}:{part}"
            )
            properties = feature["properties"]
            source = {
                "identifier": identifier,
                "tile": row["tile"],
                "sha256": row["sha256"],
                "feature_id": feature.get("id"),
                "ordinal": ordinal,
                "part": part,
                "properties": properties.copy(),
                "coordinate_step_m": coordinate_step,
            }
            height = (
                properties.get("render_min_height"),
                properties.get("render_height"),
            )
            if (
                polygon.geom_type != "Polygon"
                or polygon.is_empty
                or not polygon.is_valid
            ):
                reason = "invalid_source_building_polygon"
            elif properties.get("hide_3d"):
                reason = "provider_hidden_building_outline"
            elif (
                not all(type(v) in (int, float) and np.isfinite(v) for v in height)
                or not 0 <= height[0] < height[1]
            ):
                reason = "missing_source_building_height_interval"
            else:
                reason = ""
            if reason:
                rejected.append(source | {"reason": reason})
                continue

            def project(points):
                latitude, longitude = tile_geography(points, extent, row["tile"])
                return inputs.local(latitude, longitude)[:, :2]

            centre = np.asarray(polygon.representative_point().coords)
            latitude, longitude = tile_geography(centre, extent, row["tile"])
            coordinates = shapely.get_coordinates(polygon)
            source["clipped_or_buffered"] = bool(
                np.any(coordinates <= 0) or np.any(coordinates >= extent)
            )
            projected = shapely.transform(polygon, project)
            if not projected.is_valid or projected.is_empty:
                rejected.append(
                    source | {"reason": "invalid_projected_building_polygon"}
                )
                continue
            result.append(
                Volume(
                    identifier,
                    inputs.frame,
                    projected,
                    float(height[0]),
                    float(height[1]),
                    (float(longitude[0]), float(latitude[0])),
                    source,
                )
            )
    return result, rejected


def indoor_constraints(inputs, receipt, layers):
    layer = layers.get("transportation", {})
    extent = layer.get("extent", 4096)
    zoom, tile_x, tile_y = receipt["tile"]
    result = []
    for feature in layer.get("features", ()):
        properties = feature["properties"]
        if not properties.get("indoor"):
            continue
        geometry = feature["geometry"]
        coordinates = (
            geometry["coordinates"]
            if geometry["type"] == "MultiLineString"
            else [geometry["coordinates"]]
            if geometry["type"] == "LineString"
            else []
        )
        for part, raw in enumerate(coordinates):
            latitude, longitude = tile_geography(
                np.asarray(raw), extent, receipt["tile"]
            )
            axis = shapely.LineString(inputs.local(latitude, longitude)[:, :2])
            identifier = f"openfreemap:{zoom}/{tile_x}/{tile_y}:transportation:{feature['id']}:{part}"
            original = {
                "source_id": identifier,
                "properties": properties.copy(),
                "source_sha256": receipt["sha256"],
                "raw_tile_coordinates": raw,
                "width_m": 1.5,
                "width_origin": "explicit indoor footway/step recipe",
            }
            if properties.get("subclass") == "steps":
                purpose = "unplanned_indoor_vertical_connector"
            elif properties.get("brunnel") == "tunnel":
                purpose = "unplanned_underground_indoor_way"
            elif (
                properties.get("class") == "path"
                and properties.get("subclass") == "footway"
            ):
                purpose = "flat_indoor_walkway"
            else:
                purpose = "unplanned_indoor_source_kind"
            result.append(
                FloorRequest(
                    identifier,
                    inputs.frame,
                    axis,
                    axis.buffer(0.75, cap_style="flat", join_style="mitre"),
                    properties.get("level"),
                    original,
                    purpose,
                )
            )
    return result


def load_inputs(root):
    root = Path(root)
    manifest = json.loads((root / "manifest.json").read_text())
    if manifest["schema"] != "outshine/indoor-floor-inputs/1":
        raise ValueError("Known explicit indoor-floor capture required")
    origin = tuple(manifest["origin_lat_lon"])
    digest = hashlib.sha256(json.dumps(origin).encode()).hexdigest()[:20]
    frame = LocalFrame("WGS84-ENU-height-zero-" + digest)
    receipts = []
    for row in manifest["source_receipts"]:
        if row["kind"] == "elevation":
            receipts.append(row | {"path": str(root / row["archive"])})
    inputs = FloorInputs(origin, frame, (), (), Elevation(receipts), {})
    parts, rejected, constraints, byte_count = [], [], [], 0
    for row in manifest["source_receipts"]:
        if row["kind"] == "vector":
            data = source_bytes(root, row)
            layers = mapbox_vector_tile.decode(
                data, default_options={"y_coord_down": True}
            )
            accepted, skipped = building_parts(inputs, row, layers)
            parts.extend(accepted)
            rejected.extend(skipped)
            constraints.extend(indoor_constraints(inputs, row, layers))
            byte_count += len(data)
    requests = []
    for original in manifest["requests"]:
        world = world_coordinates(origin, np.asarray(original["xy"], dtype=float))
        longitude = world[:, 0] * 360 - 180
        latitude = np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * world[:, 1]))))
        points = inputs.local(latitude, longitude)[:, :2]
        axis = shapely.LineString(points)
        corridor = axis.buffer(
            original["width_m"] / 2, cap_style="flat", join_style="mitre"
        )
        requests.append(
            FloorRequest(
                original["source_id"],
                frame,
                axis,
                corridor,
                original["properties"].get("level"),
                original,
            )
        )
    for tile in inputs.elevation.receipts:
        inputs.elevation.raster(tile)
    inputs.parts, inputs.requests = tuple(parts), tuple(requests)
    if len({c.identifier for c in constraints}) != len(constraints):
        raise ValueError("Ambiguous original indoor source identity")
    inputs.constraints = tuple(constraints)
    bound = {c.identifier: c for c in constraints}
    maximum_frame_error = 0.0
    canonical_requests = []
    for request in requests:
        if (
            request.identifier not in bound
            or request.original["properties"]
            != bound[request.identifier].original["properties"]
        ):
            raise ValueError(
                "Original indoor request does not bind to the pinned source"
            )
        error = float(request.axis.hausdorff_distance(bound[request.identifier].axis))
        if error > 1e-7:
            raise ValueError("Legacy request/source coordinate frames disagree")
        maximum_frame_error = max(maximum_frame_error, error)
        source = bound[request.identifier]
        canonical_requests.append(
            FloorRequest(
                request.identifier,
                frame,
                source.axis,
                source.axis.buffer(
                    request.original["width_m"] / 2,
                    cap_style="flat",
                    join_style="mitre",
                ),
                request.level,
                request.original,
                source.purpose,
            )
        )
    inputs.requests = tuple(canonical_requests)
    inputs.metadata = {
        "manifest_sha256": hashlib.sha256(
            (root / "manifest.json").read_bytes()
        ).hexdigest(),
        "source_receipts": manifest["source_receipts"],
        "decoded_vector_bytes": byte_count,
        "source_parts": len(parts),
        "static_indoor_source_constraints": len(constraints),
        "maximum_original_request_source_frame_error_m": maximum_frame_error,
        "constraint_kinds": {
            k: sum(c.purpose == k for c in constraints)
            for k in sorted({c.purpose for c in constraints})
        },
        "rejected_parts": rejected,
        "legacy_request_frame": manifest["request_frame"],
        "frame": frame.identifier,
        "original_request_fixture_sha256": manifest["original_request_fixture_sha256"],
        "scope": "Fixed raw providers; explicit legacy-Mercator→geographic→ENU conversion. A chosen local programme is not original building-parent topology, native Source admission or a whole-world floor certificate.",
    }
    return inputs

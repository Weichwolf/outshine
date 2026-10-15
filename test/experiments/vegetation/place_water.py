"""Bounded source water context, without hydrological reconstruction claims."""

import json

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.places import decode_vector, tile_geography


def source_regions(receipts):
    tiles = {tuple(r["tile"]) for r in receipts if r["kind"] == "vector"}
    boxes = {
        tile: shapely.box(
            x / (1 << z), y / (1 << z), (x + 1) / (1 << z), (y + 1) / (1 << z)
        )
        for tile in tiles
        for z, x, y in (tile,)
    }
    higher = {
        z: shapely.union_all([shape for tile, shape in boxes.items() if tile[0] > z])
        for z in {tile[0] for tile in tiles}
    }
    return {tile: shape.difference(higher[tile[0]]) for tile, shape in boxes.items()}


def water(camera, elevation, receipts):
    vertices, faces, records, rejected = [], [], [], []
    regions = source_regions(receipts)
    offset = 0
    for receipt in receipts:
        if receipt["kind"] != "vector":
            continue
        layer = decode_vector(receipt).get("water", {})
        extent = layer.get("extent", 4096)
        zoom, x, y = receipt["tile"]
        region = shapely.transform(
            regions[tuple(receipt["tile"])],
            lambda p, z=zoom, xy=(x, y), e=extent: (p * (1 << z) - xy) * e,
        )
        for ordinal, feature in enumerate(layer.get("features", ())):
            kind = feature["properties"].get("class")
            identifier = f"{receipt['tile']}/water/{feature.get('id')}/{ordinal}"
            geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
            if not geometry.is_valid or geometry.geom_type not in (
                "Polygon",
                "MultiPolygon",
            ):
                rejected.append(
                    {"id": identifier, "reason": "invalid/unsupported water polygon"}
                )
                continue
            geometry = shapely.union_all(
                [
                    p
                    for p in shapely.get_parts(geometry.intersection(region))
                    if p.geom_type in ("Polygon", "MultiPolygon")
                ]
            )
            if geometry.is_empty:
                rejected.append(
                    {
                        "id": identifier,
                        "reason": "outside tile or superseded by finer vector coverage",
                    }
                )
                continue
            center = np.asarray(geometry.representative_point().coords)
            lat, lon = tile_geography(center, extent, receipt["tile"])
            try:
                height = (
                    0.0 if kind == "ocean" else float(elevation.sample(lat, lon)[0])
                )
            except ValueError:
                rejected.append(
                    {
                        "id": identifier,
                        "reason": "buffered water representative point lacks acquired DEM",
                    }
                )
                continue
            pieces = shapely.get_parts(shapely.constrained_delaunay_triangles(geometry))
            points = (
                shapely.get_coordinates(pieces).reshape(-1, 4, 2)[:, :3].reshape(-1, 2)
            )
            lat, lon = tile_geography(points, extent, receipt["tile"])
            xyz = camera.local(lat, lon, height + 0.08)
            local_faces = np.arange(len(xyz), dtype=np.uint32).reshape(-1, 3)
            triangles = xyz[local_faces]
            downward = (
                np.cross(
                    triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
                )[:, 2]
                < 0
            )
            local_faces[downward] = local_faces[downward, ::-1]
            vertices.append(xyz)
            faces.append(local_faces + offset)
            offset += len(xyz)
            records.append(
                {
                    "id": identifier,
                    "source_sha256": receipt["sha256"],
                    "class": kind,
                    "height_m": height,
                }
            )
    return Mesh(
        np.concatenate(vertices) if vertices else np.empty((0, 3)),
        np.concatenate(faces) if faces else np.empty((0, 3), dtype=np.uint32),
    ), {
        "sources": records,
        "rejected": rejected,
        "limits": "Finest supplied vector tile owns water coverage, including fine lakes; coarse polygons are clipped outside finer tile footprints. One representative DEM height per source fragment (ocean zero), curvature retained. No hydrological/shoreline, transport or native water admission; cross-tile water levels remain unconnected.",
    }

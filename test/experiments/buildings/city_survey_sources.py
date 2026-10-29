"""Preferred-provider building census inputs in local metric city windows, without DEM."""

import json
import math

import numpy as np
import shapely
from outshine_ref.places import acquire, covering_tiles, decode_vector
from outshine_ref.places.sources import tile_tuple
from provider_contract import attributes
from samples import GIRTH_M, SOURCES
from special_sources import endpoint

CITIES = {
    "Paris": (48.8710, 2.3330),
    "Rom": (41.8986, 12.4769),
    "Berlin": (52.5219, 13.4108),
    "London": (51.5132, -0.0815),
    "Zuerich": (47.3707, 8.5439),
}


def projector(city, tile):
    latitude, longitude = CITIES[city]
    scale = math.cos(math.radians(latitude))
    anchor = np.array(
        [
            longitude / 360 * GIRTH_M,
            math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi),
        ]
    )
    span = GIRTH_M / (1 << tile[0])

    def project(xy, size):
        return (
            (xy / size + tile[1:]) * [span, -span]
            + [-GIRTH_M / 2, GIRTH_M / 2]
            - anchor
        ) * scale

    return project


def pinned_sources(snapshot, tiles):
    result = {}
    for receipt in snapshot:
        tile = tile_tuple(receipt.get("tile"))
        digest, url = receipt.get("sha256"), receipt.get("url")
        if (
            receipt.get("kind") != "vector"
            or not isinstance(url, str)
            or not url
            or not isinstance(digest, str)
            or len(digest) != 64
            or any(c not in "0123456789abcdef" for c in digest)
        ):
            raise ValueError(
                "Pinned city sources require explicit vector URIs and SHA256"
            )
        if tile in result:
            raise ValueError("Pinned city source tile is duplicated")
        result[tile] = receipt
    if set(result) != set(tiles):
        raise ValueError("Pinned city sources do not cover the complete survey scope")
    return result


def read(city, fetch, *, snapshot=None):
    latitude, longitude = CITIES[city]
    tiles = covering_tiles(latitude, longitude, 1000.0, 14)
    pinned = None if snapshot is None else pinned_sources(snapshot, tiles)
    if pinned is None:
        template, definition, digest = endpoint()
    else:
        template, definition, digest = None, "explicit pinned source receipts", None
    buildings, pois, receipts, omissions = [], [], [], []
    for tile in tiles:
        url = (
            template.format(z=tile[0], x=tile[1], y=tile[2])
            if pinned is None
            else pinned[tile]["url"]
        )
        receipt = acquire(("vector", tile, url), SOURCES.parent, fetch=fetch)
        if pinned is not None and receipt["sha256"] != pinned[tile]["sha256"]:
            raise ValueError("Pinned city source payload hash changed before decode")
        receipts.append(receipt)
        layers = decode_vector(receipt)
        project = projector(city, tile)

        layer = layers.get("building", {})
        extent = layer.get("extent", 4096)
        for index, feature in enumerate(layer.get("features", ())):
            geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
            for part, polygon in enumerate(shapely.get_parts(geometry)):
                identifier = (
                    f"{city}/{tile}/{feature.get('id', 'unknown')}/{index}/{part}"
                )
                reason = None
                try:
                    parameters = attributes(
                        {
                            k: v
                            for k, v in feature["properties"].items()
                            if k not in ("colour", "color", "building:colour")
                        }
                    )
                    if (
                        polygon.geom_type != "Polygon"
                        or not polygon.is_valid
                        or polygon.is_empty
                    ):
                        reason = "invalid_or_nonpolygon"
                    elif parameters["hide3d"]:
                        reason = "provider_hide_3d"
                    elif np.any(shapely.get_coordinates(polygon) <= 0) or np.any(
                        shapely.get_coordinates(polygon) >= extent
                    ):
                        reason = "clipped_boundary_piece_excluded_from_census"
                    else:
                        polygon = shapely.transform(
                            polygon,
                            lambda xy, size=extent, projector=project: projector(
                                xy, size
                            ),
                        )
                        compactness = 4 * math.pi * polygon.area / polygon.length**2
                        shape = (
                            "courtyard"
                            if polygon.interiors
                            else "articulated"
                            if polygon.area / polygon.convex_hull.area < 0.75
                            else "elongated"
                            if compactness < 0.25
                            else "compact"
                        )
                        buildings.append(
                            {
                                "id": identifier,
                                "polygon": polygon,
                                "xy": polygon.representative_point().coords[0],
                                "height_m": parameters["buildingHeight"],
                                "min_height_m": parameters["minHeight"],
                                "body_height_m": parameters["bodyHeight"],
                                "shape": shape,
                                "properties": feature["properties"],
                                "source_sha256": receipt["sha256"],
                                "source_tile": list(tile),
                                "coordinate_step_m": GIRTH_M
                                * math.cos(math.radians(latitude))
                                / ((1 << tile[0]) * extent),
                            }
                        )
                except (ValueError, shapely.errors.GEOSException) as error:
                    reason = str(error)
                if reason:
                    omissions.append({"id": identifier, "reason": reason})
        layer = layers.get("poi", {})
        extent = layer.get("extent", 4096)
        for feature in layer.get("features", ()):
            if feature["geometry"]["type"] == "Point":
                point = project(np.array(feature["geometry"]["coordinates"]), extent)
                pois.append({"xy": point, "properties": feature["properties"]})
        print(city, tile, len(buildings), "census building polygons", flush=True)
    return (
        buildings,
        pois,
        {
            "receipts": receipts,
            "omissions": omissions,
            "provider_definition": definition,
            "provider_sha256": digest,
        },
    )

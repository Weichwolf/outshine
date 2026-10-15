"""Initial source-driven sites, retaining sources and reserving physical context."""

import json

import numpy as np
import shapely
from outshine_ref.places import axes, decode_vector, tile_geography


def polygons(camera, receipt):
    layers = decode_vector(receipt)
    rows, rejected = [], []
    for layer in ("landcover", "landuse", "water", "building", "transportation"):
        decoded = layers.get(layer, {})
        extent = decoded.get("extent", 4096)
        for ordinal, feature in enumerate(decoded.get("features", ())):
            identifier = f"{receipt['tile']}/{layer}/{feature.get('id')}/{ordinal}"
            original = shapely.from_geojson(json.dumps(feature["geometry"]))
            if not original.is_valid or original.is_empty:
                rejected.append({"id": identifier, "reason": "invalid source geometry"})
                continue

            def project(points):
                lat, lon = tile_geography(points, extent, receipt["tile"])
                return camera.local(lat, lon, 0)[:, :2]

            geometry = shapely.transform(original, project)
            rows.append(
                {
                    "id": identifier,
                    "layer": layer,
                    "properties": feature["properties"],
                    "geometry": geometry,
                    "source_sha256": receipt["sha256"],
                }
            )
    return rows, rejected


def sites(camera, receipts, ground, *, max_sites=250000):
    arrays, sources, rejected = [], [], []
    for receipt in receipts:
        if receipt["kind"] != "vector" or receipt["tile"][0] != 14:
            continue
        rows, invalid = polygons(camera, receipt)
        rejected.extend(invalid)
        wood = [
            r
            for r in rows
            if r["layer"] == "landcover" and r["properties"].get("class") == "wood"
        ]
        grass = [
            r
            for r in rows
            if r["layer"] == "landcover" and r["properties"].get("class") == "grass"
        ]
        if not wood and not grass:
            continue
        blockers = []
        for row in rows:
            if row["layer"] in ("water", "building"):
                blockers.append(row["geometry"].buffer(1.5))
            elif row["layer"] == "transportation":
                width = {
                    "motorway": 12,
                    "trunk": 10,
                    "primary": 8,
                    "secondary": 7,
                    "path": 2,
                }.get(row["properties"].get("class"), 5)
                blockers.append(row["geometry"].buffer(width / 2 + 1))
        tile = tuple(receipt["tile"])
        rng = np.random.default_rng(np.random.SeedSequence((17, *tile)))
        x, y = np.meshgrid(np.arange(128), np.arange(128))
        cell_ids = np.column_stack((x.ravel(), y.ravel()))
        pixels = (cell_ids + 0.5 + rng.uniform(-0.38, 0.38, (len(cell_ids), 2))) * 32
        lat, lon = tile_geography(pixels, 4096, tile)
        xy = camera.local(lat, lon, 0)[:, :2]
        points = shapely.points(xy)
        in_wood = shapely.contains(
            shapely.union_all([r["geometry"] for r in wood]), points
        )
        in_grass = shapely.contains(
            shapely.union_all([r["geometry"] for r in grass]), points
        )
        selected = in_wood | (in_grass & (rng.random(len(points)) < 0.035))
        if blockers:
            selected &= ~shapely.intersects(shapely.union_all(blockers), points)
        # Source tile IDs and site ordering are independent of the current camera view.
        indices = np.flatnonzero(selected)
        if not len(indices):
            continue
        if sum(len(a["positions"]) for a in arrays) + len(indices) > max_sites:
            raise ValueError("initial complete site plan exceeds explicit site budget")
        contacts = ground.sample(xy[indices])
        if any(contacts.reasons):
            raise ValueError(
                "initial source sites lack unambiguous rendered ground contacts"
            )
        source_offset = len(sources)
        sources.extend(
            {k: r[k] for k in ("id", "layer", "properties", "source_sha256")}
            for r in wood + grass
        )
        membership = np.full(len(indices), -1, dtype=np.int32)
        for i, row in enumerate(wood + grass):
            membership[
                (membership < 0) & shapely.covers(row["geometry"], points[indices])
            ] = source_offset + i
        if np.any(membership < 0):
            raise ValueError("placed site lost its source polygon")
        frames = np.array(
            [
                axes(camera.origin_latitude_deg, camera.origin_longitude_deg)
                @ axes(a, b).T
                for a, b in zip(lat[indices], lon[indices], strict=True)
            ]
        )
        angles = rng.uniform(0, 2 * np.pi, len(points))[indices]
        c, s = np.cos(angles), np.sin(angles)
        rotations = np.zeros((len(indices), 3, 3))
        rotations[:, 0, 0] = rotations[:, 1, 1] = c
        rotations[:, 0, 1], rotations[:, 1, 0], rotations[:, 2, 2] = -s, s, 1
        frames = frames @ rotations
        height = contacts.points_enu_m[:, 2]
        kind = ((height > 900) & (abs(camera.latitude_deg) > 35)).astype(np.uint8)
        arrays.append(
            {
                "positions": contacts.points_enu_m,
                "frames": frames,
                "scales": rng.uniform(0.8, 1.6, len(points))[indices]
                * np.where(kind, 0.6, 1),
                "kind": kind,
                "source": membership,
                "ids": np.column_stack(
                    (np.full((len(indices), 3), tile), cell_ids[indices])
                ),
            }
        )
    shapes = {
        "positions": (0, 3),
        "frames": (0, 3, 3),
        "scales": (0,),
        "kind": (0,),
        "source": (0,),
        "ids": (0, 5),
    }
    result = {
        k: np.concatenate([a[k] for a in arrays]) if arrays else np.empty(shape)
        for k, shape in shapes.items()
    }
    return result, {
        "sources": sources,
        "rejected": rejected,
        "sites": len(result["positions"]),
        "limits": "Initial jittered source-tile cells; two current morphological prototypes, not botanical species inference. Density is an explicit research recipe, not measured forest stocking. Water/buildings/nominal transport buffers excluded; final predecessor reservation products remain to be bound. Distant stand aggregation and near transition unapproved.",
    }

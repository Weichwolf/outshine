"""Initial source-driven sites, retaining sources and reserving physical context."""

import json

import numpy as np
import shapely
from outshine_ref.places import axes, decode_vector, tile_geography
from place_candidates import candidates


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

            def project(points, extent=extent):
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
        candidate = candidates(camera, tile)
        lat, lon = candidate["latitude"], candidate["longitude"]
        xy = camera.local(lat, lon, 0)[:, :2]
        points = shapely.points(xy)
        in_wood = shapely.contains(
            shapely.union_all([r["geometry"] for r in wood]), points
        )
        in_grass = shapely.contains(
            shapely.union_all([r["geometry"] for r in grass]), points
        )
        selected = (in_wood & candidate["tree"]) | (
            in_grass & ~in_wood & candidate["grass"]
        )
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
        angles = candidate["angle"][indices]
        c, s = np.cos(angles), np.sin(angles)
        rotations = np.zeros((len(indices), 3, 3))
        rotations[:, 0, 0] = rotations[:, 1, 1] = c
        rotations[:, 0, 1], rotations[:, 1, 0], rotations[:, 2, 2] = -s, s, 1
        frames = frames @ rotations
        height = contacts.points_enu_m[:, 2]
        tree_kind = ((height > 900) & (abs(camera.latitude_deg) > 35)).astype(np.uint8)
        kind = np.where(in_wood[indices], tree_kind, 2).astype(np.uint8)
        arrays.append(
            {
                "positions": contacts.points_enu_m,
                "frames": frames,
                "scales": candidate["scale"][indices] * np.where(tree_kind, 0.6, 1),
                "kind": kind,
                "source": membership,
                "ids": candidate["ids"][indices],
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
        "plan_format": "source-site-plan-3",
        "density": {
            "tree_grid_side": 192,
            "near_grass_radius_m": 40,
            "fine_grass_grid_side": 4096,
        },
        "kind_semantics": {
            "0": "broadleaf form proxy",
            "1": "conifer form proxy",
            "2": "graminoid tuft form proxy",
        },
        "sources": sources,
        "rejected": rejected,
        "sites": len(result["positions"]),
        "limits": "Authored jittered source-tile stocking, with independently world-anchored near grass demand. Fine cells outside the 40m camera-position window remain implicit, not deleted from the world. Camera rotation preserves this output; movement changes fine demand while shared identities and forms remain stable. Not botanical species inference or measured stocking. Water/buildings/nominal transport buffers excluded; final predecessor reservations, distant grass aggregation and near transition remain unapproved.",
    }

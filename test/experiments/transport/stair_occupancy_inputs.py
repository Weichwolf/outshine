"""Affine volumes from unchanged saved bodies and actual source/DEM receipts."""

import json
from fractions import Fraction
from math import nextafter
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from outshine_ref.places import decode_vector, tile_geography
from stair_module_inputs import (
    FIXTURE,
    INPUT,
    reference_profiles,
    source_context,
)
from stair_occupancy_geometry import AffineOccupancy, plane_value


def actual_sources():
    recipe = json.loads(FIXTURE.read_text())
    return source_context(FIXTURE.with_name(recipe["reference_fixture"]))


def full_tile_sampler(path, context):
    from camera_height_inputs import digest
    from outshine_ref.places import decode_terrarium
    from stair_context_inputs import window_samples
    from stair_cycle_inputs import vector_xy

    expected = "67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177"
    if digest(path) != expected or context["vector"]["tile"] != [14, 8937, 5678]:
        raise ValueError("pinned full original DEM tile required")
    heights = decode_terrarium(path.read_bytes())
    raster = {
        "heights_m": heights,
        "raster_shape": np.array(heights.shape),
        "origin_pixel_xy": np.array([0, 0]),
    }

    def sample(xy):
        return window_samples(
            {"mvt_xy": vector_xy(xy, context)}, raster, context["vector"]["extent"]
        )

    return sample


def outward_plane(vertices, origin, plane, delta, sign):
    wanted = [Fraction(float(p[2])) + Fraction(float(delta)) for p in vertices]
    errors = [
        plane_value(plane, origin, p[:2]) - z
        for p, z in zip(vertices, wanted, strict=True)
    ]
    correction = max(Fraction(0), max(-sign * error for error in errors))
    target = Fraction(plane[0]) + sign * correction
    z0 = float(target)
    if sign * (Fraction(z0) - target) < 0:
        z0 = nextafter(z0, sign * float("inf"))
    result = (z0, *plane[1:])
    residuals = [
        sign * (plane_value(result, origin, p[:2]) - z)
        for p, z in zip(vertices, wanted, strict=True)
    ]
    if min(residuals) < 0 or max(residuals) > Fraction(1, 100000000):
        raise ValueError("actual affine cap coverage/padding is unresolved")
    bound = float(max(residuals))
    if Fraction(bound) < max(residuals):
        bound = nextafter(bound, float("inf"))
    return result, bound


def volume(owner, lower, upper, clearance):
    if set(map(tuple, lower[:, :2])) != set(map(tuple, upper[:, :2])):
        raise ValueError("actual upper/lower projected cap vertices differ")
    origin = tuple(map(float, upper[0, :2]))
    planes, errors = [], []
    for vertices, delta, sign in ((lower, 0, -1), (upper, clearance, 1)):
        z0 = vertices[np.all(vertices[:, :2] == origin, axis=1), 2].item()
        slopes = np.linalg.solve(
            vertices[np.any(vertices[:, :2] != origin, axis=1), :2] - origin,
            vertices[np.any(vertices[:, :2] != origin, axis=1), 2] - z0,
        )
        plane, error = outward_plane(
            vertices, origin, (float(z0 + delta), *map(float, slopes)), delta, sign
        )
        planes.append(plane)
        errors.append(error)
    return AffineOccupancy(
        owner,
        shapely.Polygon(upper[:, :2]),
        "transport.body-and-headroom" if clearance else "transport.support",
        origin,
        *planes,
    ), max(errors)


def road_volumes(assets, sources):
    result, owners, errors = [], [], []
    profiles = reference_profiles()
    for asset in assets:
        data = asset.data
        for face, (top, bottom) in enumerate(
            zip(data["top_faces"], data["bottom_faces"], strict=True)
        ):
            keys = tuple(
                k
                for k, used in zip(asset.source_ids, data["owners"][face], strict=True)
                if used
            )
            kinds = [
                sources[k]["properties"].get("subclass") or "unclassified" for k in keys
            ]
            if any(kind not in profiles for kind in kinds):
                raise ValueError("explicit source headroom recipe missing")
            clearance = max(profiles[kind]["clearanceM"] for kind in kinds)
            row, error = volume(
                f"{asset.identity}:face:{face}",
                data["body_vertices"][bottom],
                data["body_vertices"][top],
                clearance,
            )
            result.append(row)
            owners.append(keys)
            errors.append(error)
    return tuple(result), tuple(owners), max(errors, default=0.0)


def reservations(assets, supports, contacts, sources, manifest):
    rows, source_keys, error = road_volumes(assets, sources)
    result, owners, errors = list(rows), list(source_keys), [error]
    support_report = json.loads(
        (Path(manifest["support_mesh"]).parent / "comparison.json").read_text()
    )
    stations = support_report["methods"][support_report["selected"]]["stations"]
    support_owners = {r["product"]: r["source_id"] for r in stations if "product" in r}
    if set(support_owners) != set(range(len(supports))) or not set(
        support_owners.values()
    ) <= set(manifest["source_ids"]):
        raise ValueError("whole support source ownership ledger differs")
    # The saved contacts are actual caps, not just support station points.
    for index, (body, (low, high)) in enumerate(zip(supports, contacts, strict=True)):
        if not (
            np.array_equal(body.vertices, np.vstack((high.vertices, low.vertices)))
            and np.array_equal(body.triangles[: len(high.triangles)], high.triangles)
            and np.array_equal(
                body.triangles[len(high.triangles) : 2 * len(high.triangles)],
                low.triangles[:, ::-1] + len(high.vertices),
            )
        ):
            raise ValueError("whole support caps differ from actual body")
        for face, triangle in enumerate(low.triangles):
            row, error = volume(
                f"{manifest['support_sha256']}:support:{index}:face:{face}",
                low.vertices[triangle],
                high.vertices[triangle],
                0,
            )
            result.append(row)
            owners.append((support_owners[index],))
            errors.append(error)
    if len(result) > 8192 or len({r.owner_id for r in result}) != len(result):
        raise ValueError("bounded unique actual occupancy ledger required")
    return tuple(result), tuple(owners), max(errors)


def buildings(context, sample):
    native = json.loads(INPUT.read_text())
    receipt = dict(
        context["vector"], path=str(INPUT.with_name(native["origin"]["payload_file"]))
    )
    layer = decode_vector(receipt)["building"]
    origin = np.array(context["origin_lat_lon"])
    scale = np.array([111320 * np.cos(np.deg2rad(origin[0])), 111132])

    def project(xy):
        lat, lon = tile_geography(xy, layer["extent"], tuple(receipt["tile"]))
        return (np.c_[lon, lat] - origin[::-1]) * scale

    result, ledger = [], []
    for index, feature in enumerate(layer["features"]):
        props = feature["properties"]
        key = (
            "openfreemap:14/8937/5678:building:" + str(feature.get("id")) + f":{index}"
        )
        shape = shapely.transform(
            shapely.from_geojson(json.dumps(feature["geometry"])), project
        )
        lo, hi = props.get("render_min_height", 0), props.get("render_height")
        reason = None
        if (
            shape.is_empty
            or not shape.is_valid
            or shape.geom_type not in ("Polygon", "MultiPolygon")
        ):
            reason = "invalid_or_nonpolygon_source"
        elif props.get("hide_3d"):
            reason = "source_hide_3d"
        elif (
            not isinstance(hi, (int, float))
            or not isinstance(lo, (int, float))
            or not hi > lo >= 0
        ):
            reason = "no_positive_provider_height_interval"
        if reason:
            ledger.append(
                {"source_id": key, "reason": reason, "original_feature": feature}
            )
            continue
        anchor = shapely.get_coordinates(shape.representative_point())
        try:
            base = float(sample(anchor)[0])
        except ValueError as error:
            ledger.append(
                {
                    "source_id": key,
                    "reason": "building_anchor_outside_declared_DEM",
                    "detail": str(error),
                    "original_feature": feature,
                }
            )
            continue
        result.append(
            BuildingFootprint(
                key, shape, HeightInterval(base + lo, base + hi), attributes=props
            )
        )
        ledger.append(
            {
                "source_id": key,
                "used": True,
                "anchor_dem_m": base,
                "height_recipe": "provider_render_heights_plus_original_DEM_anchor",
                "original_feature": feature,
            }
        )
    return tuple(result), ledger, receipt

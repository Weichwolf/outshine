"""Current tile building procedures composed at one fixed native Place camera."""

import hashlib

import numpy as np
from place_materials import Palette
from place_reference import flat
from place_tiles import decode
from tile_buildings import construct
from tile_stepped import SteppedDetails


def prepare(name, inputs):
    reference = flat(name)
    selected = next(row for row in inputs["places"] if row["camera"]["name"] == name)
    receipts = [
        row for row in inputs["receipts"] if row["url"] in selected["source_urls"]
    ]
    if any(row["kind"] != "vector" for row in receipts):
        raise ValueError("Flat Buildings scenes accept vector sources only; no DEM")
    view = reference.view()
    palette, meshes, records = Palette(), [], []
    for receipt in receipts:
        if receipt["kind"] != "vector":
            continue
        rows, details = decode(reference, receipt)
        buildings = [row for row in rows if row["layer"] == "building"]
        if not buildings:
            records.append(
                {"receipt": receipt, "generated": 0, "omissions": details["invalid"]}
            )
            continue
        generator = SteppedDetails(rows, view)
        span = (
            40075016.68557849
            * np.cos(np.deg2rad(reference.latitude_deg))
            / (1 << receipt["tile"][0])
        )
        mesh, source = construct(
            name,
            error_m=0.25,
            prepared=(rows, details | {"span_m": float(span)}),
            ordinary_builder=generator,
        )
        meshes.append(
            palette.bind(
                mesh, source["material_definitions"], generator.facades.bindings
            )
        )
        records.append(
            {
                "receipt": receipt,
                "generated": source["generated"],
                "delivered": source["delivered_building_polygons"],
                "counts": source["counts"],
                "omissions": source["omissions"],
                "source_member_ids_sha256": hashlib.sha256(
                    "\n".join(row["id"] for row in source["members"]).encode()
                ).hexdigest(),
                "detail_height_datum_m": 0.0,
                "detail_height_extent_m": [0.0, 0.0],
            }
        )
        print(name, receipt["tile"], source["generated"], "buildings", flush=True)
    arrays = {
        key: []
        for key in ("vertices", "faces", "normals", "uv", "materials", "facade_extent")
    }
    offset = 0
    for mesh in meshes:
        for key, values in arrays.items():
            if key == "faces":
                values.append(mesh[key] + offset)
            elif key in mesh:
                values.append(mesh[key])
            else:
                values.append(np.zeros((len(mesh["vertices"]), 3)))
        offset += len(mesh["vertices"])
    result = {key: np.concatenate(value) for key, value in arrays.items()}
    if not np.isfinite(result["vertices"]).all():
        raise ValueError("Place composition has nonfinite geometry")
    return result, {
        "camera": reference,
        "camera_ground_height_m": None,
        "ground_mode": "flat-no-dem",
        "view": view,
        "material_definitions": palette.definitions,
        "facade_materials": palette.facades,
        "tiles": records,
        "source_radius_m": selected["initial_source_radius_m"],
        "limits": "Flat OSM building study without DEM, terrain, transport, vegetation or water. Place locations/bearings/lenses retained; explicit flat camera heights replace geographic altitudes. Declared 3 km source window, not native 240 km/LOD/frame/device admission. Zero complete architecture recipe families.",
    }

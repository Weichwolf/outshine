"""Current tile building procedures composed at one fixed native Place camera."""

import hashlib
from dataclasses import replace

import numpy as np
from outshine_ref.places import Elevation
from place_materials import Palette
from place_reference import read
from place_terrain import mesh as terrain_mesh
from place_tiles import decode
from tile_buildings import construct
from tile_stepped import SteppedDetails


def prepare(name, inputs):
    reference = read(name)
    selected = next(row for row in inputs["places"] if row["camera"]["name"] == name)
    receipts = [
        row for row in inputs["receipts"] if row["url"] in selected["source_urls"]
    ]
    elevation = Elevation(receipts)
    camera_ground = float(
        elevation.sample(reference.latitude_deg, reference.longitude_deg)
    )
    view = reference.view(camera_ground)
    palette, meshes, records = Palette(), [], []
    for receipt in receipts:
        if receipt["kind"] != "vector":
            continue
        rows, details = decode(reference, receipt, elevation)
        buildings = [row for row in rows if row["layer"] == "building"]
        if not buildings:
            records.append(
                {"receipt": receipt, "generated": 0, "omissions": details["invalid"]}
            )
            continue
        heights = np.array([row["placement_base_enu_m"] for row in buildings])
        datum = float(np.median(heights))
        local_view = replace(
            view,
            eye=view.eye - [0, 0, datum],
            target=view.target - [0, 0, datum],
            movement_m=float(max(abs(heights - datum))),
        )
        generator = SteppedDetails(rows, local_view)
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
                "detail_height_datum_m": datum,
                "detail_height_extent_m": [float(heights.min()), float(heights.max())],
            }
        )
        print(name, receipt["tile"], source["generated"], "buildings", flush=True)
    ground_material = palette.identifier(
        "pbr",
        {
            "shader": "CityEnginePBRShader",
            "color.r": 0.36,
            "color.g": 0.39,
            "color.b": 0.31,
            "roughness": 0.9,
            "metallic": 0.0,
        },
    )
    meshes.append(terrain_mesh(reference, receipts, elevation, ground_material))
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
        "camera_ground_height_m": camera_ground,
        "view": view,
        "material_definitions": palette.definitions,
        "facade_materials": palette.facades,
        "tiles": records,
        "source_radius_m": selected["initial_source_radius_m"],
        "limits": "Declared initial near source window; distant terrain/buildings, transport, vegetation and water composition incomplete. Coarse DEM context and per-tile height-extent demand approximation are research controls, not native 240 km/LOD/frame/device admission. Zero complete architecture recipe families.",
    }

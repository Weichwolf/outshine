"""Actual source podium plans, paired prepared views and a complete pinned city window."""

import argparse
import hashlib
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from city_scene import inputs
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_products import freeze_asset, prepare
from recipe_graph_data import plain
from showcase_tiles import view_for
from stepped_facade_details import demand
from stepped_form_profiles import profile_for
from stepped_openings import apply as openings
from stepped_profile_probe import sections, sources
from stepped_source import exterior, plan_and_near
from stepped_source_probe import overview
from tile_buildings import construct
from tile_podiums import PodiumDetails
from tile_stepped import SteppedDetails


def write(output, name, mesh, record, proof):
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    report = (
        proof
        | record
        | {
            "city": name,
            "ground_mode": "flat-no-dem",
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "triangles": len(mesh["faces"]),
        }
    )
    (output / (name + "-geometry.json")).write_text(
        json.dumps(report, default=plain, indent=2) + "\n"
    )


def baseline(row):
    plan, physical = plan_and_near(row, profile_for(row["id"]))
    mesh, _env, _complete, _method = exterior(plan, 1)
    compact, records = openings(plan, mesh, 32768)
    return freeze_asset(plan, physical, compact, records)


def model(output, name, row, asset, view, proof):
    resolved, decision = demand(asset.plan, view)
    mesh = asset.near_draw if resolved else asset.compact_draw
    write(
        output,
        name,
        mesh,
        {
            "camera": camera(view),
            "source_id": row["id"],
            "source_footprint_wkb": row["footprint_wkb"],
            "source_properties": row["properties"],
            "room_ids": asset.plan["storey_room_ids"],
            "form_recipe": asset.plan["form_recipe"],
            "terrace_access": asset.plan["terrace_access"],
            "actual_near_access": asset.physical[1],
            "actual_terrace_paths": asset.physical[2],
            "facade_presentation": decision
            | {"mode": "near" if resolved else "compact"},
            "source": {
                "material_definitions": {
                    "1": {
                        "color.r": 0.72,
                        "color.g": 0.73,
                        "color.b": 0.74,
                        "roughness": 0.78,
                        "metallic": 0.0,
                    }
                }
            },
            "facade_materials": {
                "32768": {"wall_material": 1, "fractions": [1.0, 1.0], "filtered": True}
            },
            "scope": "Actual source and shared prepared form/room/core/portal plans; procedural podium proportions. Sampled physical access, no native/cache/frame/general shadow or temporal admission.",
        },
        proof,
    )


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze podium source before evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index, kind in ((2, "wing"), (3, "podium")):
        row = sources()[index]
        before, current = baseline(row), prepare(row, kind)
        points = np.r_[before.near_draw["vertices"], current.near_draw["vertices"]]
        whole = overview(points, [1, -1.25, 0.8])
        opposite = overview(points, [-1, 1.25, 0.8])
        terrace = current.plan["terrace_access"][0]
        floor = current.plan["storey_plans"][terrace["storey"]]
        door = next(
            d
            for d in floor["doors"]
            if d["id"] == "terrace-" + str(terrace["component"])
        )
        centre, t = np.asarray(door["centre"]), np.asarray(door["tangent"])
        normal = np.array([t[1], -t[0]])
        near = View(
            np.r_[centre + normal * 10 + t * 0.5, floor["floor_z_m"] + 1.7],
            np.r_[centre - normal * 0.5, floor["floor_z_m"] + 1.1],
            width=1920,
            height=1080,
            fov_deg=48,
        )
        for label, asset in (("Before", before), ("After", current)):
            for suffix, view in (
                ("Whole", whole),
                ("Opposite", opposite),
                ("Terrace", near),
            ):
                model(
                    output,
                    f"{index}{kind.title()}{label}{suffix}",
                    row,
                    asset,
                    view,
                    proof,
                )
            _mesh, _env, complete, _method = exterior(asset.plan, 1)
            sections(
                asset.plan,
                complete,
                output,
                f"{index}{kind.title()}{label}",
                indices=(
                    0,
                    current.plan["form_recipe"]["base_storeys"],
                    asset.plan["storeys"] - 1,
                ),
            )


def city_models(city, census, output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze podium city source before evidence")
    rows, receipts, window = inputs(city, json.loads(census.read_text()))
    view = replace(view_for(rows), width=1920, height=1080)
    output.mkdir(parents=True, exist_ok=True)
    for after in (False, True):

        def progress(requested, accepted, identifier):
            if requested % 8 == 0:
                print(
                    json.dumps(
                        {
                            "requested": requested,
                            "accepted": accepted,
                            "source_id": identifier,
                        }
                    ),
                    flush=True,
                )

        owner = (
            PodiumDetails(rows, view, progress=progress)
            if after
            else SteppedDetails(rows, view)
        )
        selected = owner.rows if after else rows
        mesh, source = construct(
            city,
            prepared=(selected, {"invalid": [], "span_m": window["width_m"]}),
            ordinary_builder=owner,
        )
        if source["generated"] != window["building_polygons"] or source["omissions"]:
            raise ValueError("Podium city comparison lost original source rows")
        write(
            output,
            city + "Podium" + ("After" if after else "Before"),
            mesh,
            {
                "camera": camera(view),
                "source": source,
                "window": window,
                "sources": receipts,
                "facade_materials": owner.facades.bindings,
                "procedures": owner.report(),
                "scope": "Complete unchanged pinned provider city window, flat without DEM. Source-stable new podium selection; unchanged existing fallback, not a native city/cache/frame admission.",
            },
            proof,
        )
        print(
            json.dumps(
                {
                    "after": after,
                    "generated": source["generated"],
                    "triangles": len(mesh["faces"]),
                }
            ),
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--city")
    parser.add_argument("--census", type=Path)
    args = parser.parse_args()
    if args.city:
        if args.census is None:
            parser.error("--city requires an explicit --census input")
        city_models(args.city, args.census, args.output)
    else:
        models(args.output)

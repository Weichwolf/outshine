"""Generate one bounded whole-source module directly against resident local assets."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_local_context import selected_keys, state_for
from stair_local_physics import piers, validate
from stair_local_replay import load_chain
from stair_module_catalog import catalog
from stair_module_experiment import refused_catalog, save_planning, write_json
from stair_module_family import audit, build
from stair_module_family import surfaces as old_surfaces
from stair_module_inputs import inputs
from stair_module_soil import fit, prepare
from stair_module_world import source_ground
from stair_patch_inputs import fresh, from_field
from stair_terrain_experiment import frozen


def construct(
    view,
    assets,
    supports,
    contacts,
    recipe,
    rows,
    sources,
    sample,
    *,
    max_faces=8192,
    height_floor_m,
):
    if (
        recipe.get("contact_mode") not in ("profile", "seed")
        or recipe.get("junction_mode") not in ("level_box", "shared_box")
        or (
            recipe.get("contact_mode") == "profile"
            and not ({"vertex", "end"} & recipe.get("port", {}).keys())
        )
    ):
        raise ValueError("explicit profile/level_box original root family required")
    state, context, axes, note = state_for(
        view, assets, supports, contacts, sources, recipe, max_faces=max_faces
    )
    old, line, ground, upper = old_surfaces(state, recipe, sources)
    data, network, planning = build(
        rows,
        sources,
        old,
        line,
        ground,
        sample,
        contact_mode="profile",
        junction_mode=recipe["junction_mode"],
    )
    original = shapely.union_all(planning[1])
    if not axes.buffer(16.0).covers(original):
        raise ValueError(
            "new original corridor exceeds conservative local axis support"
        )
    body = audit(state, recipe, data, rows, planning, line, upper, sources=sources)
    local, chosen, terrain = prepare(state, data, ground, recipe, sample)
    ground_reference = source_ground(ground, data)
    footprint = shapely.union_all(
        shapely.polygons(ground_reference.vertices[ground_reference.triangles, :2])
    )
    heights, roles, solve = fit(local, upper, footprint)
    if heights.min() < height_floor_m:
        raise ValueError("new field would cross the frozen cached terrain datum")
    new_foot = shapely.union_all(
        shapely.polygons(data["surface_vertices"][data["surface_faces"], :2])
    )
    removals = selected_keys(view, context, chosen, new_foot, contacts)
    patch = from_field(
        local, {"heights": heights, "constructed": roles}, view.patches[0].frame
    )
    updated, replace_note = view.replace(patch, removals)
    physical = validate(updated, state, local, heights, roles, chosen, data, upper)
    pier_note = piers(updated, contacts)
    return {
        "data": data,
        "planning": planning,
        "local": local,
        "field": {"heights": heights, "constructed": roles},
        "removals": removals,
        "chosen": chosen,
        "context": context,
        "patch": patch,
        "view": updated,
        "proof": {
            "context": note,
            "network": network,
            "module": body,
            "terrain": terrain,
            "solve": solve,
            "replacement": replace_note,
            "physical": physical,
            "piers": pier_note,
        },
    }


def refused(args, manifest, recipe, commit, reason, began):
    args.output.mkdir()
    inventory = refused_catalog(
        Path(manifest["catalog"]).parent,
        recipe,
        reason,
        catalog_path=Path(manifest["catalog"]),
    )
    write_json(args.output / "source-catalog.json", inventory)
    report = {
        "commit": commit,
        "resident_sha256": digest(args.resident / "resident.json"),
        "parent_reports": manifest["parent_reports"],
        "fixture_sha256": digest(args.fixture),
        "candidate_source_ids": manifest["source_ids"],
        "refused_source_ids": recipe["source_ids"],
        "reason": reason,
        "admitted_source_ids": [],
        "valid_local_module": False,
        "valid_retained_baseline": True,
        "artifacts": {
            "source-catalog.json": digest(args.output / "source-catalog.json")
        },
        "seconds": time.perf_counter() - began,
        "scope": "No new resident assets published; complete original source catalogue retained with joint trial reason.",
    }
    write_json(args.output / "comparison.json", report)
    print(json.dumps(report), flush=True)
    return report


def run(args):
    commit, began = frozen(), time.perf_counter()
    fresh(args.output)
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    recipe, rows, context, sources, sample, layer = inputs(args.fixture)
    if context["origin_lat_lon"] != manifest["origin_lat_lon"] or set(
        recipe["source_ids"]
    ) & set(manifest["source_ids"]):
        raise ValueError("same source frame and entirely new original sources required")
    if (
        type(args.max_context_faces) is not int
        or not 1 <= args.max_context_faces <= 8192
    ):
        raise ValueError("bounded local terrain context capacity required")
    try:
        result = construct(
            view,
            assets,
            supports,
            contacts,
            recipe,
            rows,
            sources,
            sample,
            max_faces=args.max_context_faces,
            height_floor_m=manifest["height_floor_m"],
        )
    except ValueError as error:
        return refused(args, manifest, recipe, commit, str(error), began)
    old_catalog = Path(manifest["catalog"])
    inventory = catalog(
        old_catalog.parent,
        {
            "catalog_path": old_catalog,
            "report": {"after": {"source_ids": manifest["source_ids"]}},
        },
        rows,
        sources,
        layer,
        result["planning"][1],
        context,
    )
    args.output.mkdir()
    arrays = {
        "module.npz": result["data"],
        "local-layout.npz": result["local"],
        "local-field.npz": result["field"],
        "selection.npz": {"old_selected_faces": result["chosen"]},
    }
    for name, data in arrays.items():
        np.savez_compressed(args.output / name, **data)
    save_planning(args.output, rows, result["planning"], inventory)
    write_json(
        args.output / "local-keys.json",
        {"context": result["context"].face_keys, "removals": result["removals"]},
    )
    names = [*arrays, "planning.json", "source-catalog.json", "local-keys.json"]
    report = {
        "commit": commit,
        "resident_sha256": digest(args.resident / "resident.json"),
        "parent_reports": manifest["parent_reports"],
        "fixture_sha256": digest(args.fixture),
        "origin_lat_lon": context["origin_lat_lon"],
        "before_source_ids": manifest["source_ids"],
        "candidate_source_ids": manifest["source_ids"] + recipe["source_ids"],
        "admitted_source_ids": [],
        "artifacts": {n: digest(args.output / n) for n in names},
        "proof": result["proof"],
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "valid_local_module": True,
        "scope": "Local resident-asset generator, no global field/road assembly or runtime/cache/LOD/native admission.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("commit", "seconds", "peak_rss_bytes", "valid_local_module")
            }
        ),
        flush=True,
    )
    return report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-context-faces", type=int, default=8192)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(arguments())

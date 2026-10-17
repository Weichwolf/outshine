"""Independent complete-field control; global assembly is deliberately diagnostic only."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_local_replay import load_chain, replay
from stair_module_experiment import write_json
from stair_module_family import audit, build
from stair_module_family import surfaces as old_surfaces
from stair_module_inputs import inputs, read_arrays
from stair_module_soil import assemble, fit, prepare
from stair_module_world import inspect_world, source_ground
from stair_patch_experiment import compare
from stair_patch_inputs import fresh
from stair_reference_roads import reference_owner_indices, reference_roads
from stair_terrain_experiment import frozen


def baseline_binding(args, manifest):
    before = json.loads((args.baseline / "comparison.json").read_text())
    receipt_path = args.baseline / "verification.json"
    if receipt_path.exists():
        receipt = json.loads(receipt_path.read_text())
        snapshot = (
            receipt.get("valid_prepared_snapshot") is True
            and args.baseline.resolve() == args.resident.resolve()
            and receipt.get("prepared_manifest_sha256")
            == manifest["root_resident_sha256"]
            and before.get("artifacts", {}).get("resident.json")
            == manifest["root_resident_sha256"]
        )
        valid = (
            (receipt.get("valid_local_module") is True or snapshot)
            and receipt["products_report_sha256"]
            == digest(args.baseline / "comparison.json")
            and before["origin_lat_lon"] == manifest["origin_lat_lon"]
        )
    else:
        equality = before.get("equality", {})
        parent = args.prior_module[-1] if args.prior_module else None
        valid = (
            parent is not None
            and before.get("resident_sha256") == manifest["root_resident_sha256"]
            and equality.get("local_report_sha256")
            == digest(parent / "comparison.json")
            and equality.get("local_verification_sha256")
            == digest(parent / "verification.json")
            and equality.get("module_local_field_all_arrays_bit_exact") is True
            and equality.get("exact_face_attributes") is True
        )
    if not valid or before["candidate_source_ids"] != manifest["source_ids"]:
        raise ValueError("independently verified whole reference source/frame required")
    return before


def reference_state(args):
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    before = baseline_binding(args, manifest)
    for name in ("world-layout.npz", "world-field.npz", "world-body.npz"):
        if before["artifacts"][name] != digest(args.baseline / name):
            raise ValueError("whole reference input geometry changed")
    plan, field = (
        read_arrays(args.baseline / "world-layout.npz"),
        read_arrays(args.baseline / "world-field.npz"),
    )
    compare(view, plan, field)
    road, ground, keys, rows, note = reference_roads(assets, max_assets=1024)
    if keys != manifest["source_ids"]:
        raise ValueError("diagnostic whole reference needs every original road asset")
    state = {
        "resident_profile_roots": True,
        "road_assets": assets,
        "road": road,
        "road_ground": ground,
        "road_source_faces": reference_owner_indices(assets),
        "cached_module_rows": rows,
        "report": {"candidate_source_ids": keys, "after": {"source_ids": keys}},
        "plan": plan,
        "heights": field["heights"],
        "piers": supports,
        "pier_contacts": contacts,
    }
    note["source_facet_index_bytes"] = sum(
        indices.nbytes for indices in state["road_source_faces"].values()
    )
    return state, manifest, note


def construct(state, recipe, rows, sources, sample):
    old, line, ground, upper = old_surfaces(state, recipe, sources)
    data, _network, planning = build(
        rows,
        sources,
        old,
        line,
        ground,
        sample,
        contact_mode="profile",
        junction_mode=recipe["junction_mode"],
    )
    module = audit(state, recipe, data, rows, planning, line, upper)
    local, chosen, terrain = prepare(state, data, ground, recipe, sample)
    reference = source_ground(ground, data)
    footprint = shapely.union_all(
        shapely.polygons(reference.vertices[reference.triangles, :2])
    )
    heights, local_roles, solve = fit(local, upper, footprint)
    plan, world_heights, body, parts, assembly = assemble(
        state, local, heights, chosen, max_vertices=32768
    )
    roles, world = inspect_world(
        state,
        local,
        heights,
        chosen,
        plan,
        world_heights,
        body,
        parts,
        reference,
        upper,
        footprint,
        expected_local_roles=local_roles,
    )
    arrays = {
        "module.npz": data,
        "local-layout.npz": local,
        "local-field.npz": {"heights": heights, "constructed": local_roles},
        "world-layout.npz": plan,
        "world-field.npz": {"heights": world_heights, "constructed": roles},
        "world-body.npz": {"vertices": body.vertices, "faces": body.triangles, **parts},
        "selection.npz": {"old_selected_faces": chosen},
    }
    return arrays, {
        "module": module,
        "terrain": terrain,
        "solve": solve,
        "assembly": assembly,
        "world": world,
    }


def compare_saved(args, arrays):
    for name in ("module.npz", "local-layout.npz", "local-field.npz"):
        actual = read_arrays(args.local / name)
        expected = arrays[name]
        if set(actual) != set(expected):
            raise ValueError("local and whole reference array schemas differ")
        for key, value in expected.items():
            same = (
                value.shape == actual[key].shape
                and value.dtype == actual[key].dtype
                and value.tobytes() == actual[key].tobytes()
            )
            if not same:
                raise ValueError(
                    "local and independent whole reference differ: " + name + ":" + key
                )
    view, _assets, _supports, _contacts, _manifest, report, _saved = replay(
        args.resident, args.local, args.prior_module
    )
    if not report["valid_local_module"]:
        raise ValueError("accepted independently verified local module required")
    exact = compare(view, arrays["world-layout.npz"], arrays["world-field.npz"])
    return {
        **exact,
        "module_local_field_all_arrays_bit_exact": True,
        "local_report_sha256": digest(args.local / "comparison.json"),
        "local_verification_sha256": digest(args.local / "verification.json"),
    }


def run(args):
    commit, began = frozen(), time.perf_counter()
    fresh(args.output)
    recipe, rows, context, sources, sample, _layer = inputs(args.fixture)
    state, manifest, note = reference_state(args)
    if context["origin_lat_lon"] != manifest["origin_lat_lon"]:
        raise ValueError("whole reference recipe frame differs")
    arrays, proof = construct(state, recipe, rows, sources, sample)
    equality = compare_saved(args, arrays)
    args.output.mkdir()
    for name, data in arrays.items():
        np.savez_compressed(args.output / name, **data)
    report = {
        "commit": commit,
        "resident_sha256": digest(args.resident / "resident.json"),
        "baseline_report_sha256": digest(args.baseline / "comparison.json"),
        "fixture_sha256": digest(args.fixture),
        "candidate_source_ids": manifest["source_ids"] + recipe["source_ids"],
        "admitted_source_ids": [],
        "proof": proof,
        "roads": note,
        "equality": equality,
        "artifacts": {n: digest(args.output / n) for n in arrays},
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Independent complete-field diagnostic control, not the local generator or a runtime load path.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {k: report[k] for k in ("commit", "seconds", "peak_rss_bytes", "equality")}
        ),
        flush=True,
    )
    return report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "baseline", "fixture", "local", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(arguments())

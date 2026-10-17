"""Compare bounded construction orders for the same five original source parts."""

import argparse
import copy
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_local_assets import road_asset
from stair_local_generate import construct
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import FIXTURE, inputs
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def cases():
    base = FIXTURE.parent
    feet = json.loads((base / "stair-four-surface-contact-vienna.json").read_text())
    minor = json.loads((base / "stair-cached-grid-minor-vienna.json").read_text())
    paving = json.loads((base / "stair-cached-grid-paving-vienna.json").read_text())
    after = json.loads((base / "stair-cached-grid-footways-vienna.json").read_text())
    joint = json.loads((base / "stair-six-source-contact-vienna.json").read_text())
    joint["source_ids"] = joint["source_ids"][1:]
    joint["port"], joint["junction_mode"] = minor["port"], "shared_box"
    joint["endpoint_contacts"] = minor["endpoint_contacts"] + feet["endpoint_contacts"]
    joint["surface_contacts"] = feet["surface_contacts"]
    joint.pop("source_fallback", None)
    gridded = dict(joint, cached_box_grids_m=minor["cached_box_grids_m"])
    all_boxes = dict(paving, cached_box_grids_m=minor["cached_box_grids_m"])
    return {
        "foot_first_undeclared": [feet, dict(minor, cached_box_grids_m={})],
        "joint_boxes": [joint],
        "joint_grid_boxes": [gridded],
        "road_first_grid": [minor, after],
        "foot_first_all_grid": [feet, all_boxes],
        "foot_first_selective_grid": [feet, paving],
    }


def append_trial(result, rows, assets, output):
    data = result["data"]
    path = output / "module.npz"
    np.savez_compressed(path, **data)
    road = {
        "body_vertices": data["body_vertices"],
        "body_faces": data["body_faces"],
        "top_faces": data["surface_faces"],
        "bottom_faces": data["surface_faces"] + len(data["surface_vertices"]),
        "owners": data["owners"],
        "ground": np.ones(len(data["surface_faces"]), bool),
        "surface_materials": data["surface_materials"],
    }
    asset = road_asset(digest(path), [r["source_id"] for r in rows], rows, road)
    return (*assets, asset), digest(path)


def compare_case(args, name, recipes, state, target):
    view, assets, supports, contacts, manifest = state
    completed, stages, reason = [], [], ""
    output = args.output / name
    output.mkdir()
    for index, recipe in enumerate(recipes):
        stage = output / str(index)
        stage.mkdir()
        fixture = stage / "recipe.json"
        write_json(fixture, recipe)
        actual, rows, _, sources, sample, _ = inputs(fixture)
        began = time.perf_counter()
        try:
            result = construct(
                view,
                assets,
                supports,
                contacts,
                actual,
                rows,
                sources,
                sample,
                height_floor_m=manifest["height_floor_m"],
            )
            seconds = time.perf_counter() - began
            assets, sha = append_trial(result, rows, assets, stage)
            view = result["view"]
            completed.extend(actual["source_ids"])
            proof = result["proof"]
            write_json(stage / "physical-proof.json", proof)
            stages.append(
                {
                    "source_ids": actual["source_ids"],
                    "physical_construct": True,
                    "seconds": seconds,
                    "module_sha256": sha,
                    "context": proof["context"],
                    "terrain": proof["terrain"],
                    "proof_sha256": digest(stage / "physical-proof.json"),
                }
            )
        except ValueError as error:
            reason = str(error)
            stages.append(
                {
                    "source_ids": actual["source_ids"],
                    "physical_construct": False,
                    "seconds": time.perf_counter() - began,
                    "reason": reason,
                }
            )
            break
    if not set(completed) <= target:
        raise ValueError("comparison introduced a different source cohort")
    return {
        "name": name,
        "stages": stages,
        "reason": reason,
        "whole_five_completed": set(completed) == target,
        "completed_source_ids": completed,
        "unused_source_ids": sorted(target - set(completed)),
        "admitted_source_ids": [],
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir())):
        raise ValueError("comparison requires explicit system-temp output")
    state = load_chain(args.resident, args.prior_module)
    variants = cases()
    target = set(variants["joint_boxes"][0]["source_ids"])
    for recipes in variants.values():
        if {key for recipe in recipes for key in recipe["source_ids"]} != target:
            raise ValueError("identical complete original cohort required")
    args.output.mkdir()
    result = [
        compare_case(args, name, copy.deepcopy(recipes), state, target)
        for name, recipes in variants.items()
    ]
    report = {
        "commit": commit,
        "target_source_ids": sorted(target),
        "cases": result,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Six bounded physical construction variants after identical72 preload; source/reader/export/whole-world admission is separate. Actual temporary body hashes/owners, no fake verification receipts. Each stage keeps normal geometry/solver limits; no global-optimality or runtime claim.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            [
                {k: c[k] for k in ("name", "whole_five_completed", "reason")}
                for c in result
            ]
        ),
        flush=True,
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

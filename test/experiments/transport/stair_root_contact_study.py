"""Replay a whole new through source and export its actual cached root and terrain."""

import argparse
import copy
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_local_context import state_for
from stair_local_replay import load_chain, replay
from stair_module_geometry import old_surfaces
from stair_module_inputs import inputs, read_arrays
from stair_module_roots import bind_root_pad
from stair_module_verify import saved_planning
from stair_patch_inputs import fresh
from stair_root_contact import new_through_root
from stair_shared_service_reference import unchanged_assets
from stair_shared_service_study import render_scenes
from stair_terrain_experiment import frozen


def held(args):
    before, old_assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    after, assets, _, _, _, report, saved = replay(
        args.resident, args.local, args.prior_module
    )
    recipe, rows, _, sources, sample, _ = inputs(args.fixture)
    if (
        saved is None
        or not report["valid_local_module"]
        or report["fixture_sha256"] != digest(args.fixture)
        or len(rows) != 1
        or not new_through_root(recipe, sources)
        or report["candidate_source_ids"]
        != manifest["source_ids"] + recipe["source_ids"]
        or rows[0]["original"]["source"]["feature"]
        != sources[rows[0]["source_id"]]["original_feature"]
    ):
        raise ValueError(
            "One independently verified whole original through source required"
        )
    state, _, _, _ = state_for(before, old_assets, supports, contacts, sources, recipe)
    descriptor, line, ground, upper = old_surfaces(state, recipe, sources)
    planned_rows, planning = saved_planning(args.local, rows, ground)
    root = next(n for n in planning[0] if np.array_equal(n["xy"], line[1, :2]))
    try:
        bind_root_pad(copy.deepcopy(planned_rows), root, line, include_interiors=True)
    except ValueError as error:
        if "no original endpoint" not in str(error):
            raise
        old_reason = str(error)
    else:
        raise ValueError("Control must reproduce the old primary endpoint rejection")
    return {
        "before": before,
        "after": after,
        "assets": assets,
        "manifest": manifest,
        "state": state,
        "recipe": recipe,
        "rows": rows,
        "sources": sources,
        "sample": sample,
        "data": saved["data"],
        "ground": ground,
        "upper": upper,
        "line": line,
        "planning": planning,
        "descriptor": descriptor,
        "old_assets_exact": unchanged_assets(old_assets, assets),
        "old_primary_guard": old_reason,
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    context = held(args)
    manifest, recipe, rows = (context[k] for k in ("manifest", "recipe", "rows"))
    support_path = Path(manifest["support_mesh"])
    if digest(support_path) != manifest["support_sha256"]:
        raise ValueError("Complete cached support mesh changed")
    support = read_arrays(support_path)
    args.output.mkdir()
    scenes = render_scenes(
        args.output,
        context["before"],
        context["after"],
        context["assets"],
        Mesh(support["vertices"], support["triangles"]),
        rows[0],
        context["data"],
    )
    for name in ("before", "after"):
        scene = json.loads((args.output / (name + "-scene.json")).read_text())
        center = context["line"][1]
        scene.update(
            id="through-root-detail-" + name,
            span_m=18.0,
            target=center.tolist(),
            eye=(center + [0, -25, 25]).tolist(),
            output=str(args.output / ("detail-" + name + ".png")),
        )
        (args.output / ("detail-" + name + "-scene.json")).write_text(
            json.dumps(scene, indent=2) + "\n"
        )
    receipt = json.loads((args.local / "verification.json").read_text())
    profiles = receipt["module"]["profiles"]
    if (
        not profiles.get("new_through_root")
        or not profiles["old_profile_completed"]
        or len(profiles["new_source_interior_profiles"]) != 1
        or profiles["new_source_interior_profiles"][0]["reference"]
        != "opposite_through_sides"
    ):
        raise ValueError("Independent complete old and bilateral new profiles required")
    result = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "saved_product_sha256": digest(args.local / "comparison.json"),
        "saved_reader_sha256": digest(args.local / "verification.json"),
        "sources_before": len(manifest["source_ids"]),
        "sources_after": len(manifest["source_ids"]) + 1,
        "source_id": rows[0]["source_id"],
        "old_port": recipe["port"],
        "source_length_m": float(
            np.linalg.norm(np.diff(rows[0]["xy"], axis=0), axis=1).sum()
        ),
        "old_assets_exact": context["old_assets_exact"],
        "old_primary_guard": context["old_primary_guard"],
        "profiles": profiles,
        "scenes": scenes,
        "native_admitted_source_ids": receipt["admitted_source_ids"],
        "scope": "Actual cached old endpoint and whole new through source, preserved old assets and complete terrain; diagnostic software images, no public routing/native/world/device admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sources": result["sources_after"],
                "old_assets_exact": result["old_assets_exact"],
                "output": str(args.output),
            }
        )
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "local", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    return parser.parse_args()


if __name__ == "__main__":
    run(arguments())

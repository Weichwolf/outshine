"""Export the actual whole requested seed product and its unchanged preceding assets."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_local_replay import load_chain, replay
from stair_module_inputs import inputs, read_arrays
from stair_occupancy_display import whole_camera
from stair_patch_inputs import fresh
from stair_shared_service_reference import unchanged_assets
from stair_shared_service_study import render_scenes
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    fresh(args.output)
    receipt = json.loads((args.study / "report.json").read_text())
    selection = json.loads((args.study / "selection.json").read_text())["index"]
    if (
        not receipt["valid_automatic_module"]
        or receipt.get("selection_family") != "seed"
        or receipt["selection_sha256"] != digest(args.study / "selection.json")
    ):
        raise ValueError("Independently read whole requested seed study required")
    chosen = receipt["chosen_module"]
    local = args.study / f"outshine-module-{chosen}"
    fixture = args.study / f"recipe-{chosen}.json"
    before, old_assets, _, _, manifest = load_chain(args.resident, args.prior_module)
    after, assets, _, _, _, report, saved = replay(
        args.resident, local, args.prior_module
    )
    recipe, rows, context, _sources, _sample, _ = inputs(fixture)
    if (
        saved is None
        or recipe["contact_mode"] != "seed"
        or not report["valid_local_module"]
        or report["fixture_sha256"] != digest(fixture)
        or context["origin_lat_lon"] != manifest["origin_lat_lon"]
        or manifest["source_ids"] != selection["before_source_ids"]
        or report["candidate_source_ids"]
        != manifest["source_ids"] + recipe["source_ids"]
    ):
        raise ValueError(
            "Same complete physical seed product and preceding chain required"
        )
    exact = unchanged_assets(old_assets, assets)
    support_path = Path(manifest["support_mesh"])
    if digest(support_path) != manifest["support_sha256"]:
        raise ValueError("Actual complete support mesh changed")
    support = read_arrays(support_path)
    args.output.mkdir()
    scenes = render_scenes(
        args.output,
        before,
        after,
        assets,
        Mesh(support["vertices"], support["triangles"]),
        rows[0],
        saved["data"],
    )
    after_scene = json.loads((args.output / "after-scene.json").read_text())
    with np.load(args.output / "after.npz", allow_pickle=False) as shown:
        whole = whole_camera(after_scene, shown["vertices"], ())
    whole.update(detail_view=False, id="requested-seed-whole")
    (args.output / "whole-scene.json").write_text(json.dumps(whole, indent=2) + "\n")
    proof = {
        "commit": commit,
        "study_report_sha256": digest(args.study / "report.json"),
        "local_report_sha256": digest(local / "comparison.json"),
        "reader_sha256": digest(local / "verification.json"),
        "old_assets_exact": exact,
        "sources_before": len(manifest["source_ids"]),
        "sources_after": len(report["candidate_source_ids"]),
        "new_source_ids": recipe["source_ids"],
        "native_admitted_source_ids": report["admitted_source_ids"],
        "scenes": scenes,
        "scope": "Actual complete held terrain/support/road arrays; contact and whole1280x720 diagnostics. Existing family material assumptions, no native/world/routing/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "study", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

"""Replay the whole repaired network and preserve its actual pre-cleanup terrain."""

import argparse
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
import stair_module_soil as soil
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_local_context import state_for
from stair_local_replay import load_chain, replay
from stair_module_geometry import old_surfaces
from stair_module_inputs import inputs, read_arrays
from stair_patch_inputs import fresh
from stair_shared_service_reference import unchanged_assets
from stair_shared_service_study import render_scenes
from stair_terrain_experiment import frozen


def compare_arrays(expected, actual):
    if expected.keys() != actual.keys():
        raise ValueError("Complete array fields changed")
    for key in expected:
        np.testing.assert_array_equal(expected[key], actual[key])
    return {"fields": len(expected), "bytes": sum(v.nbytes for v in expected.values())}


def run(args):
    commit = frozen()
    fresh(args.output)
    before, old_assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    after, assets, _, _, _, report, saved = replay(
        args.resident, args.local, args.prior_module
    )
    recipe, rows, context, sources, sample, _ = inputs(args.fixture)
    if (
        saved is None
        or not report["valid_local_module"]
        or report["fixture_sha256"] != digest(args.fixture)
        or context["origin_lat_lon"] != manifest["origin_lat_lon"]
        or report["candidate_source_ids"]
        != manifest["source_ids"] + recipe["source_ids"]
    ):
        raise ValueError(
            "Complete same-frame independently accepted whole-source product required"
        )
    exact_assets = unchanged_assets(old_assets, assets)
    state, _, _, _ = state_for(before, old_assets, supports, contacts, sources, recipe)
    _, _, ground, _ = old_surfaces(state, recipe, sources)
    original, captured = soil.remove_covered_duplicates, {}

    def capture(xy, faces):
        captured.update(xy=xy.copy(), faces=faces.copy())
        return original(xy, faces)

    with patch.object(soil, "remove_covered_duplicates", capture):
        local, chosen, terrain = soil.prepare(
            state, saved["data"], ground, recipe, sample
        )
    layout = compare_arrays(saved["local"], local)
    np.testing.assert_array_equal(saved["chosen"], chosen)
    proof = terrain["work"].get("covered_duplicate_facets", {})
    if not proof.get("removed_facets") or not captured:
        raise ValueError("Actual saved network must require a proved duplicate removal")
    body_control = compare_arrays(read_arrays(args.before_body), saved["data"])
    support_path = Path(manifest["support_mesh"])
    if digest(support_path) != manifest["support_sha256"]:
        raise ValueError("Original complete support mesh changed")
    support = read_arrays(support_path)
    args.output.mkdir()
    np.savez_compressed(args.output / "pre-cleanup.npz", **captured)
    scenes = render_scenes(
        args.output,
        before,
        after,
        assets,
        Mesh(support["vertices"], support["triangles"]),
        rows[0],
        saved["data"],
    )
    result = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "product_sha256": digest(args.local / "comparison.json"),
        "reader_sha256": digest(args.local / "verification.json"),
        "before_body_sha256": digest(args.before_body),
        "new_body_unchanged": body_control,
        "pre_cleanup_sha256": digest(args.output / "pre-cleanup.npz"),
        "layout_exact_reproduction": layout,
        "old_assets_exact": exact_assets,
        "sources_before": len(manifest["source_ids"]),
        "sources_after": len(report["candidate_source_ids"]),
        "new_source_ids": recipe["source_ids"],
        "removed_overlap": proof,
        "scenes": scenes,
        "native_admitted_source_ids": report["admitted_source_ids"],
        "scope": "Whole alternative three-source module from the fixed 107-source baseline; immutable old assets, unchanged new body, exact terrain reproduction. Actual diagnostic meshes, no native/world/routing/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sources": result["sources_after"],
                "old_assets_exact": exact_assets,
                "output": str(args.output),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "local", "fixture", "before-body", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

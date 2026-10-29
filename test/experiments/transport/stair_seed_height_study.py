"""Replay the repaired whole source and its original continuous height opponent."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights
from stair_local_replay import load_chain, replay
from stair_module_inputs import inputs, read_arrays
from stair_module_roads import face_frames
from stair_patch_inputs import fresh
from stair_seed_height_rounding import repair, verify
from stair_shared_service_reference import unchanged_assets
from stair_shared_service_study import render_scenes
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    fresh(args.output)
    before, old_assets, _, _, baseline = load_chain(args.resident, args.prior_module)
    after, assets, _, _, manifest, report, saved = replay(
        args.resident, args.local, args.prior_module
    )
    if saved is None or report["fixture_sha256"] != digest(args.fixture):
        raise ValueError(
            "Accepted whole repaired product bound to the fixture required"
        )
    _, rows, _, sources, _, _ = inputs(args.fixture)
    data = saved["data"]
    vertices, faces = data["surface_vertices"], data["surface_faces"]
    nominal = face_frames(rows, vertices[:, :2], faces, data["owners"])
    witness = verify(data, nominal, rows, [])
    continuous = fit_surface_heights(
        Mesh(np.c_[vertices[:, :2], data["targets"]], faces),
        data["grade_frames"],
        budget=HeightBudget(max_vertices=4096, max_faces=8192),
    )
    if continuous.mesh is None:
        raise ValueError(
            "Independent original continuous fit did not terminate validly"
        )
    np.testing.assert_array_equal(continuous.mesh.vertices[:, :2], vertices[:, :2])
    np.testing.assert_array_equal(continuous.mesh.triangles, faces)
    np.testing.assert_array_equal(
        continuous.mesh.vertices[:, 2], data["unrounded_seed_heights"]
    )
    repeated, proof = repair(continuous.mesh.vertices, faces, nominal, seconds=8)
    np.testing.assert_array_equal(repeated, vertices)
    if proof is None:
        raise ValueError("The original stored-height failure was not reproduced")
    old = unchanged_assets(old_assets, assets)
    supports = read_arrays(Path(manifest["support_mesh"]))
    if digest(Path(manifest["support_mesh"])) != manifest["support_sha256"]:
        raise ValueError("The original support mesh changed")
    args.output.mkdir()
    scenes = render_scenes(
        args.output,
        before,
        after,
        assets,
        Mesh(supports["vertices"], supports["triangles"]),
        rows[0],
        data,
    )
    result = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "product_sha256": digest(args.local / "comparison.json"),
        "source_id": rows[0]["source_id"],
        "source_properties": sources[rows[0]["source_id"]]["properties"],
        "sources_before": len(baseline["source_ids"]),
        "sources_after": len(report["candidate_source_ids"]),
        "old_assets_exact": old,
        "original_continuous_fit_exact": True,
        "xy_and_topology_exact": True,
        "contraction_replay_exact": True,
        "witness": witness,
        "scenes": scenes,
        "added_witness_bytes": data["unrounded_seed_heights"].nbytes,
        "native_admitted_source_ids": report["admitted_source_ids"],
        "scope": "Complete local transport/terrain/support control; no native/world/routing/dynamics/device admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sources": result["sources_after"],
                "old_assets_exact": old,
                "contraction_replay_exact": True,
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "local", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

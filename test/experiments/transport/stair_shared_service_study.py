"""Replay the complete saved shared-service product and export actual diagnostic meshes."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from stair_local_replay import load_chain, replay
from stair_module_inputs import inputs, read_arrays
from stair_module_materials import colour_component
from stair_patch_inputs import fresh, whole
from stair_service_vehicle import inspect_service
from stair_shared_service_reference import equal_checks, separate_caps, unchanged_assets
from stair_terrain_experiment import frozen
from transport_surface_experiment import PALETTE


def display_mesh(view, assets, supports, palette):
    result = whole(view)
    shown = {
        "vertices": np.empty((0, 3), float),
        "triangles": np.empty((0, 3), int),
        "normals": np.empty((0, 3), float),
        "materials": np.empty(0, int),
        "face_base_colour_linear_rgba": np.empty((0, 4), float),
    }
    shown = append(shown, [(result.mesh, 2), (supports, 6)], palette)
    for asset in assets:
        first = len(shown["triangles"])
        body = Mesh(asset.data["body_vertices"], asset.data["body_faces"])
        shown = append(shown, [(body, 3)], palette)
        colour_component(shown, first, asset.data, palette)
    return shown


def render_scenes(output, before, after, assets, support_mesh, row, data):
    classes = {
        a["name"]: a["albedo"] for a in json.loads(PALETTE.read_text())["classes"]
    }
    # Use existing physical material values for this geometry diagnostic.
    soil, paving = classes["earth_moist"], classes["asphalt"]
    palette = np.array([[*soil, 1.0]] * 9)
    palette[[3, 8], :3] = paving
    palette[6, :3] = classes.get("concrete", paving)
    vertices = data["surface_vertices"]
    center = (vertices.min(0) + vertices.max(0)) / 2
    axis = np.asarray(row["xy"][-1]) - row["xy"][0]
    axis /= np.linalg.norm(axis)
    sideways = np.array([-axis[1], axis[0]])
    camera = {
        "projection": "orthographic",
        "profile": [1280, 720],
        "samples": 16,
        "colours": palette.tolist(),
        "target": center.tolist(),
        "eye": (center + np.r_[sideways * 100, 100]).tolist(),
        "span_m": float(np.linalg.norm(np.ptp(vertices[:, :2], axis=0)) * 1.4),
        "detail_view": True,
    }
    records = []
    for name, view, roads in (
        ("before", before, assets[:-1]),
        ("after", after, assets),
    ):
        shown = display_mesh(view, roads, support_mesh, palette)
        mesh = output / (name + ".npz")
        np.savez_compressed(mesh, **shown)
        scene = {
            **camera,
            "id": "shared-service-" + name,
            "mesh": str(mesh),
            "mesh_sha256": digest(mesh),
            "output": str(output / (name + ".png")),
        }
        (output / (name + "-scene.json")).write_text(json.dumps(scene, indent=2) + "\n")
        records.append(
            {
                "name": name,
                "mesh_sha256": digest(mesh),
                "vertices": len(shown["vertices"]),
                "triangles": len(shown["triangles"]),
                "scope": "Actual whole held transport/terrain/support meshes; source materials are diagnostic family assumptions, not native scene acceptance",
            }
        )
    return records


def run(args):
    commit = frozen()
    fresh(args.output)
    before, old_assets, _, _, old_manifest = load_chain(
        args.resident, args.prior_module
    )
    after, assets, _, _, manifest, report, saved = replay(
        args.resident, args.local, args.prior_module
    )
    if (
        saved is None
        or not report["valid_local_module"]
        or report["fixture_sha256"] != digest(args.fixture)
    ):
        raise ValueError(
            "The complete saved source product must be independently accepted"
        )
    recipe, rows, _, sources, _, _ = inputs(args.fixture)
    if (
        len(rows) != 1
        or report["candidate_source_ids"]
        != old_manifest["source_ids"] + recipe["source_ids"]
    ):
        raise ValueError("One whole original source must extend the complete baseline")
    old = unchanged_assets(old_assets, assets)
    row = rows[0]
    proof = inspect_service(saved["data"], rows)
    equal_checks(
        proof["checks"], separate_caps(saved["data"], row, proof["curves"]["vehicle"])
    )
    if (
        not row["original"]["source"]["feature"]
        == sources[row["source_id"]]["original_feature"]
    ):
        raise ValueError("Supplied complete original feature changed")
    supports = read_arrays(Path(manifest["support_mesh"]))
    if digest(Path(manifest["support_mesh"])) != manifest["support_sha256"]:
        raise ValueError("The actual complete support mesh changed")
    args.output.mkdir()
    scenes = render_scenes(
        args.output,
        before,
        after,
        assets,
        Mesh(supports["vertices"], supports["triangles"]),
        row,
        saved["data"],
    )
    result = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "saved_product_sha256": digest(args.local / "comparison.json"),
        "saved_reader_sha256": digest(args.local / "verification.json"),
        "sources_before": len(old_manifest["source_ids"]),
        "sources_after": len(report["candidate_source_ids"]),
        "source_id": row["source_id"],
        "source_properties": sources[row["source_id"]]["properties"],
        "source_length_m": float(
            np.linalg.norm(np.diff(row["xy"], axis=0), axis=1).sum()
        ),
        "old_assets_exact": old,
        "complete_direction_checks": len(proof["checks"]),
        "independent_checks_identical": True,
        "physical_lanes": row["traffic_profile"]["physical_lanes"],
        "opposing_traffic_policy": row["traffic_profile"]["opposing_traffic_policy"],
        "scenes": scenes,
        "native_admitted_source_ids": report["admitted_source_ids"],
        "scope": "Independent saved geometry and separate-cap body controls; missing direction is procedural, no simultaneous traffic, public routing or native/world/device admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "source_id": row["source_id"],
                "sources": result["sources_after"],
                "old_assets_exact": old,
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

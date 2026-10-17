"""Inspect a saved resident snapshot against original payloads, patches and profiles."""

import argparse
import json
from pathlib import Path

from camera_height_inputs import digest
from stair_local_assets import load
from stair_local_reference import baseline_binding
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_experiment import compare
from stair_resident_profile_verify import exact_array
from stair_terrain_experiment import frozen


def artifacts(output, report):
    manifest = json.loads((output / "resident.json").read_text())
    if Path(manifest["terrain"]).resolve() != (output / "terrain").resolve():
        raise ValueError("snapshot must own its terrain representation")
    terrain = json.loads((output / "terrain/patches.json").read_text())
    expected = {
        "resident.json",
        "terrain/patches.json",
        "world-layout.npz",
        "world-field.npz",
        "world-body.npz",
    }
    expected.update(r["file"] for r in manifest["roads"])
    expected.update("terrain/" + r["file"] for r in terrain["patches"])
    if set(report["artifacts"]) != expected:
        raise ValueError("complete resident snapshot artifacts required")
    for name, sha in report["artifacts"].items():
        path = (output / name).resolve()
        if not path.is_relative_to(output.resolve()) or digest(path) != sha:
            raise ValueError("saved resident snapshot artifact changed: " + name)


def inputs(report):
    source = report["inputs"]
    old = Path(source["resident"])
    modules = [Path(p) for p in source["prior_modules"]]
    if (
        digest(old / "resident.json") != source["resident_sha256"]
        or [digest(p / "comparison.json") for p in modules] != source["parent_reports"]
        or [digest(p / "verification.json") for p in modules]
        != source["parent_receipts"]
    ):
        raise ValueError("snapshot original resident/module history changed")
    state = load_chain(old, modules)
    profiles = Path(source["profile_products"])
    receipt = Path(source["profile_receipt"])
    if (
        digest(profiles / "resident.json") != source["profile_manifest_sha256"]
        or digest(receipt) != source["profile_receipt_sha256"]
    ):
        raise ValueError("snapshot original profile product changed")
    proof = json.loads(receipt.read_text())
    if (
        proof.get("valid_prepared_root_profiles") is not True
        or proof["prepared_manifest_sha256"] != source["profile_manifest_sha256"]
        or proof["frame"] != state[4]["frame"]
    ):
        raise ValueError("previously verified same-frame root profiles required")
    return state, load(profiles)[1][0], proof


def equal_assets(actual, original, root):
    if len(actual) != len(original):
        raise ValueError("snapshot changed immutable road asset count")
    facets = 0
    for index, (after, before) in enumerate(zip(actual, original, strict=True)):
        if (
            after.identity != before.identity
            or after.source_ids != before.source_ids
            or after.rows != (root.rows if index == 0 else before.rows)
            or set(after.data) != set(before.data)
            or not all(exact_array(after.data[k], before.data[k]) for k in before.data)
        ):
            raise ValueError("snapshot changed old road identity/geometry/metadata")
        facets += len(after.data["body_faces"])
    return facets


def equal_patches(actual, original):
    if len(actual.patches) != len(original.patches):
        raise ValueError("snapshot changed terrain patch count")
    for after, before in zip(actual.patches, original.patches, strict=True):
        if (
            after.patch_id != before.patch_id
            or after.frame != before.frame
            or not exact_array(after.mesh.vertices, before.mesh.vertices)
            or not exact_array(after.mesh.triangles, before.mesh.triangles)
            or set(after.face_data) != set(before.face_data)
            or not all(
                exact_array(after.face_data[k], before.face_data[k])
                for k in before.face_data
            )
            or not exact_array(
                actual.active_faces[after.patch_id],
                original.active_faces[before.patch_id],
            )
        ):
            raise ValueError("snapshot changed actual terrain or active face masks")


def verify(args):
    commit = frozen()
    receipt_path = getattr(args, "output", None) or args.products / "verification.json"
    if receipt_path.exists() or not receipt_path.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("fresh resident snapshot verification receipt required")
    report = json.loads((args.products / "comparison.json").read_text())
    artifacts(args.products, report)
    before, root, profile = inputs(report)
    after = load(args.products)
    body_facets = equal_assets(after[1], before[1], root)
    equal_patches(after[0], before[0])
    for key in (
        "frame",
        "origin_lat_lon",
        "source_ids",
        "support_sha256",
        "catalog_sha256",
        "height_floor_m",
    ):
        if after[4][key] != before[4][key]:
            raise ValueError("snapshot changed original world binding: " + key)
    if (
        after[1][0].identity != root.identity
        or root.source_ids != after[1][0].source_ids
        or len(root.rows) != profile["root_profiles"]
        or report["candidate_source_ids"] != before[4]["source_ids"]
    ):
        raise ValueError("snapshot root profile/source ledger changed")
    baseline = Path(report["inputs"]["baseline"])
    if (
        digest(baseline / "comparison.json")
        != report["inputs"]["baseline_report_sha256"]
    ):
        raise ValueError("snapshot whole reference report changed")
    old_report = baseline_binding(
        argparse.Namespace(
            baseline=baseline,
            resident=Path(report["inputs"]["resident"]),
            prior_module=[Path(p) for p in report["inputs"]["prior_modules"]],
        ),
        before[4],
    )
    for name in ("world-layout.npz", "world-field.npz", "world-body.npz"):
        if digest(args.products / name) != old_report["artifacts"][name]:
            raise ValueError("snapshot whole reference geometry changed: " + name)
    equality = compare(
        after[0],
        read_arrays(args.products / "world-layout.npz"),
        read_arrays(args.products / "world-field.npz"),
    )
    receipt = {
        "reader_commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "prepared_manifest_sha256": digest(args.products / "resident.json"),
        "source_count": len(after[4]["source_ids"]),
        "old_road_body_facets_byte_exact": body_facets,
        "root_profiles": len(root.rows),
        "same_patch_identities_and_active_masks": True,
        "equality": equality,
        "valid_prepared_snapshot": True,
        "scope": "Exact original road payload identities/arrays, patches/masks and verified root metadata; "
        "no new source, geometry solve or native runtime admission.",
    }
    write_json(receipt_path, receipt)
    print(json.dumps(receipt), flush=True)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path)
    verify(parser.parse_args())

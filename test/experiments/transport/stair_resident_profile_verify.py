"""Replay prepared root profiles against their original recipe and physical assets."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_context_inputs import PROFILES
from stair_cycle_inputs import load as original_rows
from stair_local_assets import load
from stair_module_inputs import FIXTURE, cached
from stair_terrain_experiment import frozen


def audit_rows(rows, expected, recipe_sha, body_sha):
    if len(rows) != len(expected) or not 1 <= len(rows) <= 32:
        raise ValueError("complete bounded original root profile ledger required")
    fields = {"source_id", "xy", "width_m", "resident_profile_provenance"}
    provenance = {"fixture_sha256": recipe_sha, "body_sha256": body_sha}
    for actual, original in zip(rows, expected, strict=True):
        if set(actual) != fields or actual["source_id"] != original["source_id"]:
            raise ValueError("ordered original root profile identities required")
        xy = np.asarray(actual["xy"], float)
        if not np.isfinite(xy).all() or not np.array_equal(xy, original["xy"]):
            raise ValueError("actual historical root axis differs from its recipe")
        if (
            type(actual["width_m"]) not in (int, float)
            or actual["width_m"] != original["width_m"]
        ):
            raise ValueError("actual historical root profile width changed")
        if actual["resident_profile_provenance"] != provenance:
            raise ValueError("root recipe/body provenance changed")


def exact_array(actual, expected):
    return (
        actual.dtype == expected.dtype
        and actual.shape == expected.shape
        and actual.tobytes() == expected.tobytes()
    )


def verify(args):
    commit = frozen()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("fresh explicit private /tmp receipt required")
    view, assets, _piers, _contacts, manifest = load(args.products)
    base = Path(manifest["base_display"]).parents[1]
    support = Path(manifest["support_mesh"]).parent
    state = cached(
        base,
        support,
        {
            "reference_fixture": args.reference.name,
            "reference_fixture_sha256": digest(args.reference),
        },
    )
    relative = PROFILES.relative_to(Path(__file__).resolve().parents[3]).as_posix()
    historical = subprocess.check_output(
        ["git", "show", state["report"]["commit"] + ":" + relative]
    )
    if historical != PROFILES.read_bytes():
        raise ValueError("historical procedural profile catalog changed")
    _case, expected, _nodes, _stairs, context, _sample = original_rows(args.reference)
    root = assets[0]
    keys = state["report"]["candidate_source_ids"]
    if (
        list(root.source_ids) != keys
        or keys != [r["source_id"] for r in expected]
        or context["origin_lat_lon"] != manifest["origin_lat_lon"]
        or state["support_report"]["origin_lat_lon"] != manifest["origin_lat_lon"]
    ):
        raise ValueError("original root source order/coordinate frame changed")
    audit_rows(
        root.rows,
        expected,
        digest(args.reference),
        state["report"]["after"]["body_sha256"],
    )
    fields = ("body_vertices", "body_faces", "top_faces", "bottom_faces")
    if not all(exact_array(root.data[k], state["road"][k]) for k in fields):
        raise ValueError("actual original root physical arrays changed")
    mask = state["road"]["face_sources"].astype(np.uint64)
    expected_owners = np.column_stack(
        [(mask & (1 << i)) != 0 for i in range(len(keys))]
    )
    if not exact_array(root.data["owners"], expected_owners):
        raise ValueError("actual original root facet owners changed")
    plan = state["plan"]
    xyz = np.c_[plan["xy"], state["heights"]]
    expected_beds = xyz[plan["faces"][plan["ground"]]]
    actual_beds = root.data["body_vertices"][
        root.data["bottom_faces"][root.data["ground"]]
    ]
    if sorted(p.tobytes() for p in expected_beds) != sorted(
        p.tobytes() for p in actual_beds
    ):
        raise ValueError("actual original root ground-facet identities changed")
    receipt = {
        "reader_commit": commit,
        "prepared_manifest_sha256": digest(args.products / "resident.json"),
        "reference_fixture_sha256": digest(args.reference),
        "original_body_sha256": state["report"]["after"]["body_sha256"],
        "original_profile_catalog_sha256": digest(PROFILES),
        "root_profiles": len(expected),
        "root_geometry_arrays_byte_exact": True,
        "root_ownership_and_ground_exact": True,
        "valid_prepared_root_profiles": True,
        "frame": view.patches[0].frame.identifier,
        "scope": "Cold independent source recipe replay and original physical arrays; "
        "not module/route/world admission. Normal prepared consumers do not repeat this decode.",
    }
    args.output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    verify(parser.parse_args())

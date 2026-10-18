"""Real resident-asset corruption, incomplete neighbours and retained-world controls."""

import hashlib
import json
import resource
import shutil
import tempfile
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_frontier_sources import module_recipe
from stair_local_assets import load
from stair_local_context import selected_keys, state_for
from stair_local_generate import arguments, run
from stair_local_physics import frozen_beds
from stair_local_replay import apply_product, load_chain, replay
from stair_local_verify import verify
from stair_module_experiment import write_json
from stair_module_inputs import inputs
from stair_terrain_experiment import frozen


def refused(call, contains):
    try:
        call()
    except ValueError as error:
        if contains not in str(error):
            raise
        return str(error)
    raise ValueError("negative resident control unexpectedly admitted")


def fingerprint(view, assets):
    value = hashlib.sha256()
    for patch, active in zip(view.patches, view.active_faces.values(), strict=True):
        value.update(
            patch.mesh.vertices.tobytes()
            + patch.mesh.triangles.tobytes()
            + active.tobytes()
        )
    for asset in assets:
        for key, data in sorted(asset.data.items()):
            value.update(key.encode() + data.tobytes())
    return value.hexdigest()


def corruptions(args, tmp):
    cases = {}
    resident = tmp / "resident"
    resident.mkdir()
    manifest = json.loads((args.resident / "resident.json").read_text())
    for row in manifest["roads"]:
        shutil.copy2(args.resident / row["file"], resident / row["file"])
    bad = dict(manifest, frame="wrong-actual-ENU-frame")
    write_json(resident / "resident.json", bad)
    cases["wrong_frame"] = refused(lambda: load(resident), "frames differ")
    write_json(resident / "resident.json", manifest)
    target = resident / manifest["roads"][0]["file"]
    raw = target.read_bytes()
    target.write_bytes(raw[:-1] + bytes([raw[-1] ^ 1]))
    cases["damaged_old_road_payload"] = refused(
        lambda: load(resident), "payload changed"
    )
    clone = tmp / "product"
    shutil.copytree(args.output, clone, ignore=shutil.ignore_patterns("display"))
    keys = json.loads((clone / "local-keys.json").read_text())
    old = load_chain(args.resident, args.prior_module)
    patch_id, active = next(
        (p, a) for p, a in old[0].active_faces.items() if not a.all()
    )
    stale = int(np.flatnonzero(~active)[0])
    keys["removals"].setdefault(patch_id, [0])[0] = stale
    write_json(clone / "local-keys.json", keys)
    report = json.loads((clone / "comparison.json").read_text())
    report["artifacts"]["local-keys.json"] = digest(clone / "local-keys.json")
    write_json(clone / "comparison.json", report)
    cases["stale_old_face_with_fresh_checksum"] = refused(
        lambda: apply_product(old, clone), "active local face IDs"
    )
    cases["old_verification_cannot_authorize_changed_product"] = refused(
        lambda: replay(args.resident, clone, args.prior_module),
        "verification receipt required",
    )
    return cases


def geometry_controls(args):
    old_view, assets, supports, contacts, _manifest = load_chain(
        args.resident, args.prior_module
    )
    recipe, _rows, _context, sources, _sample, _layer = inputs(args.fixture)
    state, context, _axes, _note = state_for(
        old_view, assets, supports, contacts, sources, recipe
    )
    updated, _new_assets, _supports, _contacts, _manifest, _report, saved = replay(
        args.resident, args.output, args.prior_module
    )
    footprint = shapely.union_all(
        shapely.polygons(
            saved["data"]["surface_vertices"][saved["data"]["surface_faces"], :2]
        )
    )
    required = old_view.nearby(footprint, 24).face_keys[0]
    kept = [i for i, key in enumerate(context.face_keys) if key != required]
    incomplete = old_view.faces([context.face_keys[i] for i in kept], max_faces=8192)
    cases = {
        "omitted_required_terrain_face": refused(
            lambda: selected_keys(
                old_view, incomplete, saved["chosen"][kept], footprint, contacts
            ),
            "omitted required terrain",
        )
    }
    changed = dict(state)
    changed["road"] = dict(state["road"])
    changed["road"]["body_vertices"] = state["road"]["body_vertices"].copy()
    ids = np.unique(state["road"]["bottom_faces"][state["road_ground"]])
    changed["road"]["body_vertices"][ids, 2] += 0.001
    field = Mesh(
        np.c_[saved["local"]["xy"], saved["field"]["heights"]], saved["local"]["faces"]
    )
    cases["shifted_original_frozen_body_underside"] = refused(
        lambda: frozen_beds(updated, changed, field), "Double contact differs"
    )
    return cases


def retained_after_reentry(args, tmp):
    prior = [*args.prior_module, args.output]
    view, assets, _supports, _contacts, manifest = load_chain(args.resident, prior)
    original = fingerprint(view, assets)
    recipe, _rows, _context, sources, _sample, _layer = inputs(args.fixture)
    root_key = "openfreemap:14/8937/5678:transportation:249866752:0"
    root = {
        "port": {"source_id": root_key, "vertex": 2},
        "node": sources[root_key]["mvt_xy"][2],
    }
    rows = {r["source_id"]: r for a in assets for r in a.rows}
    ref = {k: recipe[k] for k in ("reference_fixture", "reference_fixture_sha256")}
    trial = module_recipe(
        ref,
        {"cached_module_rows": rows},
        sources,
        root,
        {"source_ids": ["openfreemap:14/8937/5678:transportation:538150640:8"]},
    )
    path = tmp / "new-original-source.json"
    write_json(path, trial)
    output = tmp / "outshine-bounded-reentry"
    bounded = SimpleNamespace(
        resident=args.resident,
        fixture=path,
        output=output,
        prior_module=prior,
        max_context_faces=1,
    )
    report = run(bounded)
    if (
        report["valid_local_module"]
        or not report["valid_retained_baseline"]
        or report["candidate_source_ids"] != manifest["source_ids"]
    ):
        raise ValueError("bounded reentry did not preserve the accepted world")
    verify(bounded)
    after, after_assets, _supports, _contacts, _manifest, _report, _saved = replay(
        args.resident, output, prior
    )
    if fingerprint(after, after_assets) != original:
        raise ValueError("bounded failed reentry changed resident world geometry")
    return {
        "retained_sources": len(manifest["source_ids"]),
        "reason": report["reason"],
        "original_new_recipe": trial,
        "before_after_geometry_sha256": original,
        "retained_reader_report_sha256": digest(output / "verification.json"),
    }


def controls(args):
    commit, began = frozen(), time.perf_counter()
    target = args.output / "local-controls.json"
    if target.exists():
        raise ValueError("fresh local control receipt required")
    view, assets, _supports, _contacts, _manifest = load_chain(
        args.resident, args.prior_module
    )
    original = fingerprint(view, assets)
    with tempfile.TemporaryDirectory(
        prefix="outshine-transport-local-controls-"
    ) as directory:
        tmp = Path(directory)
        failures = corruptions(args, tmp)
        failures.update(geometry_controls(args))
        reentry = retained_after_reentry(args, tmp)
    if fingerprint(view, assets) != original:
        raise ValueError("adverse control mutated the held old resident snapshot")
    report = {
        "reader_commit": commit,
        "local_report_sha256": digest(args.output / "comparison.json"),
        "refused_cases": failures,
        "reentry": reentry,
        "held_old_geometry_sha256": original,
        "held_old_geometry_unchanged": True,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Real source/geometry/adverse-reader controls; temporary corrupted copies removed, original inputs and accepted assets preserved.",
    }
    write_json(target, report)
    print(json.dumps(report), flush=True)
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    controls(arguments())

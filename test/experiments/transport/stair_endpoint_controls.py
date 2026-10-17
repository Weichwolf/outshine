"""Replay real end/through proposals and reject undeclared or mismatched contacts."""

import copy
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

from camera_height_inputs import digest
from stair_frontier_sources import module_recipe, proposals
from stair_module_audit import audit
from stair_module_cache import load_cached
from stair_module_experiment import argument_parser, write_json
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import inputs
from stair_module_loops import frozen_contacts
from stair_terrain_experiment import frozen


def fingerprint(road):
    value = hashlib.sha256()
    for key in sorted(road):
        value.update(key.encode())
        value.update(road[key].tobytes())
    return value.hexdigest()


def refusal(call, expected):
    try:
        call()
    except ValueError as error:
        if expected not in str(error):
            raise
        return str(error)
    raise ValueError("negative control unexpectedly admitted")


def geometry(state, recipe, path):
    write_json(path, recipe)
    _recipe, rows, _context, sources, sample, _layer = inputs(path)
    old, line, ground, upper = old_surfaces(state, recipe, sources)
    data, _note, planning = build(
        rows,
        sources,
        old,
        line,
        ground,
        sample,
        contact_mode=recipe["contact_mode"],
        junction_mode=recipe["junction_mode"],
    )
    return audit(state, recipe, data, rows, planning, line, upper)


def original_selection(state, sources, recipe):
    ref = {k: recipe[k] for k in ("reference_fixture", "reference_fixture_sha256")}
    indexed = proposals(state, sources, ref, method="index", strategy="prefix")
    scanned = proposals(state, sources, ref, method="scan", strategy="prefix")
    if indexed[0]["proposals"] != scanned[0]["proposals"] or indexed[1:] != scanned[1:]:
        raise ValueError("indexed and scanned original-source proposals differ")
    choices = [
        r
        for r in indexed[-1]
        if r["source_ids"] == recipe["source_ids"] and r["port"] == recipe["port"]
    ]
    if len(choices) != 1 or any(
        choices[0].get(k, []) != recipe.get(k, [])
        for k in ("interior_contacts", "endpoint_contacts")
    ):
        raise ValueError("fixture differs from actual automatic prefix contacts")
    return ref, indexed, scanned


def controls(args):
    commit, started = frozen(), time.perf_counter()
    receipt = args.output / "endpoint-controls.json"
    if receipt.exists():
        raise ValueError("fresh endpoint control receipt required")
    verified = json.loads((args.output / "verification.json").read_text())
    if verified.get("valid_local_module") is not True or verified[
        "products_report_sha256"
    ] != digest(args.output / "comparison.json"):
        raise ValueError("independently verified saved product required")
    recipe, _rows, _context, sources, _sample, _layer = inputs(args.fixture)
    state = load_cached(args, recipe)
    before = fingerprint(state["road"])
    ref, indexed, scanned = original_selection(state, sources, recipe)
    cases = {}
    with tempfile.TemporaryDirectory(
        prefix="outshine-transport-endpoint-controls-"
    ) as tmp:
        path = Path(tmp) / "trial.json"
        missing = copy.deepcopy(recipe)
        del missing["endpoint_contacts"]
        cases["missing_actual_endpoint"] = refusal(
            lambda: geometry(state, missing, path), "undeclared cached asset"
        )
        wrong = copy.deepcopy(recipe)
        wrong["endpoint_contacts"][0]["vertex"] = 0
        cases["different_original_node"] = refusal(
            lambda: frozen_contacts(state, wrong, sources), "exact shared original node"
        )
        ground = state["road_ground"]
        try:
            state["road_ground"] = ground.copy()
            state["road_ground"][:] = False
            cases["nonground_cached_endpoint"] = refusal(
                lambda: frozen_contacts(state, recipe, sources), "actual cached ground"
            )
        finally:
            state["road_ground"] = ground
        key = "openfreemap:14/8937/5678:transportation:249866752:0"
        root = {
            "port": {"source_id": key, "vertex": 2},
            "node": sources[key]["mvt_xy"][2],
        }
        near = module_recipe(
            ref,
            state,
            sources,
            root,
            {"source_ids": ["openfreemap:14/8937/5678:transportation:538150640:8"]},
        )
        cases["nearby_undeclared_overlap"] = refusal(
            lambda: geometry(state, near, path), "undeclared cached asset"
        )
        root = {
            "port": recipe["port"],
            "node": sources[recipe["port"]["source_id"]]["mvt_xy"][
                recipe["port"]["vertex"]
            ],
        }
        whole = module_recipe(
            ref,
            state,
            sources,
            root,
            {"source_ids": recipe["source_fallback"]["full_source_ids"]},
        )
        cases["whole_five_source_proposal"] = refusal(
            lambda: geometry(state, whole, path), "collapses or inverts"
        )
    if fingerprint(state["road"]) != before:
        raise ValueError("negative control mutated immutable old road geometry")
    report = {
        "reader_commit": commit,
        "fixture_sha256": digest(args.fixture),
        "verification_sha256": digest(args.output / "verification.json"),
        "original_selection_equal": True,
        "prefix_matches_automatic_selection": True,
        "selection_work": {"index": indexed[0]["work"], "scan": scanned[0]["work"]},
        "refused_cases": cases,
        "old_road_arrays_sha256": before,
        "old_road_arrays_unchanged": True,
        "seconds": time.perf_counter() - started,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Real source geometry ablations; not new whole-world or native admission.",
    }
    write_json(receipt, report)
    print(json.dumps(report), flush=True)
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    controls(argument_parser().parse_args())

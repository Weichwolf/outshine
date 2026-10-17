"""Replay independently verified saved local assets without source decoding or solving."""

import json
from pathlib import Path

from camera_height_inputs import digest
from stair_local_assets import load, module_road, road_asset
from stair_module_inputs import read_arrays
from stair_patch_inputs import from_field
from stair_shared_headroom import declarations


def apply_product(state, output):
    output = Path(output)
    view, assets, supports, contacts, manifest = state
    report = json.loads((output / "comparison.json").read_text())
    if (
        report["resident_sha256"] != manifest["root_resident_sha256"]
        or report.get("parent_reports", []) != manifest["parent_reports"]
        or report["admitted_source_ids"]
    ):
        raise ValueError("local product resident/frame/non-admission binding changed")
    expected = {"source-catalog.json"}
    if report["valid_local_module"]:
        expected.update(
            {
                "module.npz",
                "local-layout.npz",
                "local-field.npz",
                "selection.npz",
                "planning.json",
                "local-keys.json",
            }
        )
    if set(report["artifacts"]) != expected:
        raise ValueError("complete local product artifact manifest required")
    for name, expected in report["artifacts"].items():
        path = (output / name).resolve()
        if not path.is_relative_to(output.resolve()) or digest(path) != expected:
            raise ValueError("saved local product bytes changed: " + name)
    if not report["valid_local_module"]:
        if report["candidate_source_ids"] != manifest["source_ids"] or not report.get(
            "valid_retained_baseline"
        ):
            raise ValueError("refused local module changed the old source ledger")
        return view, assets, supports, contacts, manifest, report, None
    data = read_arrays(output / "module.npz")
    local, field = (
        read_arrays(output / "local-layout.npz"),
        read_arrays(output / "local-field.npz"),
    )
    keys = json.loads((output / "local-keys.json").read_text())
    patch = from_field(local, field, view.patches[0].frame)
    updated, _note = view.replace(patch, keys["removals"])
    rows = json.loads((output / "planning.json").read_text())["rows"]
    source_ids = [r["source_id"] for r in rows]
    if (
        report["before_source_ids"] != manifest["source_ids"]
        or report["candidate_source_ids"] != manifest["source_ids"] + source_ids
        or set(source_ids) & set(manifest["source_ids"])
        or report["origin_lat_lon"] != manifest["origin_lat_lon"]
    ):
        raise ValueError("saved local source or frame ledger changed")
    new_asset = road_asset(
        digest(output / "module.npz"), source_ids, rows, module_road(data)
    )
    saved = {
        "data": data,
        "local": local,
        "field": field,
        "keys": keys,
        "chosen": read_arrays(output / "selection.npz")["old_selected_faces"],
    }
    return updated, (*assets, new_asset), supports, contacts, manifest, report, saved


def verified(output):
    output = Path(output)
    receipt = json.loads((output / "verification.json").read_text())
    if receipt["products_report_sha256"] != digest(output / "comparison.json") or not (
        receipt.get("valid_local_module") or receipt.get("valid_retained_baseline")
    ):
        raise ValueError("successful independent local verification receipt required")


def load_chain(resident, prior=()):
    if len(prior) > 32 or len({Path(p).resolve() for p in prior}) != len(prior):
        raise ValueError("bounded unique ordered local-module inputs required")
    view, assets, supports, contacts, manifest = load(resident)
    manifest = dict(
        manifest,
        root_resident_sha256=digest(Path(resident) / "resident.json"),
        parent_reports=[],
        borrowed_headroom_contacts=list(manifest.get("borrowed_headroom_contacts", [])),
    )
    state = view, assets, supports, contacts, manifest
    for path in prior:
        verified(path)
        view, assets, supports, contacts, before, report, _saved = apply_product(
            state, path
        )
        catalog = Path(path) / "source-catalog.json"
        manifest = dict(
            before,
            source_ids=report["candidate_source_ids"],
            catalog=str(catalog.resolve()),
            catalog_sha256=digest(catalog),
            parent_reports=[
                *before["parent_reports"],
                digest(Path(path) / "comparison.json"),
            ],
            borrowed_headroom_contacts=[
                *before["borrowed_headroom_contacts"],
                *declarations(report, report["artifacts"].get("module.npz")),
            ],
        )
        if len(manifest["borrowed_headroom_contacts"]) > 128:
            raise ValueError("resident borrowed headroom declaration budget exhausted")
        state = view, assets, supports, contacts, manifest
    return state


def products(resident, output, prior=()):
    return apply_product(load_chain(resident, prior), output)


def replay(resident, output, prior=()):
    verified(output)
    return products(resident, output, prior)

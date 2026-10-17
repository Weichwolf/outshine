"""Prepare a fresh resident snapshot from verified unchanged assets and root profiles."""

import argparse
import json
import os
import resource
import time
from pathlib import Path

from camera_height_inputs import digest
from stair_local_assets import load
from stair_local_reference import baseline_binding
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_experiment import compare
from stair_patch_inputs import fresh, save_patch
from stair_resident_profile_verify import exact_array
from stair_terrain_experiment import frozen


def profile_rows(root, source, receipt, source_sha, frame):
    if (
        not receipt.get("valid_prepared_root_profiles")
        or receipt["prepared_manifest_sha256"] != source_sha
        or receipt["frame"] != frame
        or root.identity != source.identity
        or root.source_ids != source.source_ids
        or [r["source_id"] for r in source.rows] != list(root.source_ids)
        or receipt["root_profiles"] != len(source.rows)
        or set(root.data) != set(source.data)
        or not all(exact_array(root.data[k], source.data[k]) for k in root.data)
    ):
        raise ValueError("verified profiles for the same immutable root required")
    return source.rows


def profiles(args, state):
    source = load(args.profile_products)
    receipt = json.loads(args.profile_receipt.read_text())
    if source[4]["frame"] != state[4]["frame"]:
        raise ValueError("profile snapshot coordinate frame changed")
    return profile_rows(
        state[1][0],
        source[1][0],
        receipt,
        digest(args.profile_products / "resident.json"),
        state[4]["frame"],
    )


def terrain(output, view, old_path, modules, source_ids, commit):
    old = json.loads((old_path / "patches.json").read_text())
    target = output / "terrain"
    (target / "assets").mkdir(parents=True)
    rows = []
    for row in old["patches"]:
        path = target / row["file"]
        path.parent.mkdir(parents=True, exist_ok=True)
        os.link(old_path / row["file"], path)
        rows.append(row)
    if len(view.patches) != len(rows) + len(modules):
        raise ValueError("complete resident patch history required")
    history = list(old["source_reports"])
    materialized = 0
    for module in modules:
        row = save_patch(target, len(rows), view.patches[len(rows)])
        row["removals"] = json.loads((module / "local-keys.json").read_text())[
            "removals"
        ]
        report = json.loads((module / "comparison.json").read_text())
        materialized += (target / row["file"]).stat().st_size
        history.append(
            {
                "report_sha256": digest(module / "comparison.json"),
                "fixture_sha256": report["fixture_sha256"],
            }
        )
        rows.append(row)
    write_json(
        target / "patches.json",
        {
            "version": 1,
            "commit": commit,
            "frame": old["frame"],
            "origin_lat_lon": old["origin_lat_lon"],
            "source_ids": source_ids,
            "source_reports": history,
            "patches": rows,
            "scope": "Same original patches/removals; no new field solve or source decode.",
        },
    )
    return target, materialized


def roads(output, resident, modules, manifest, assets, root_rows):
    result = []
    for row in manifest["roads"]:
        os.link(resident / row["file"], output / row["file"])
        result.append(dict(row))
    result[0]["rows"] = root_rows
    first = len(result)
    if len(assets) != first + len(modules):
        raise ValueError("complete immutable road history required")
    for module, asset in zip(modules, assets[first:], strict=True):
        name = "road-" + str(len(result)) + ".npz"
        os.link(module / "module.npz", output / name)
        result.append(
            {
                "file": name,
                "sha256": asset.identity,
                "payload": "local-module",
                "source_ids": asset.source_ids,
                "rows": asset.rows,
            }
        )
    return result


def before_display(module, baseline):
    receipt = json.loads((module / "display/verification.json").read_text())
    path = module / "display/after-display.npz"
    if (
        not receipt["exact_independent_whole_body"]
        or receipt["local_report_sha256"] != digest(module / "comparison.json")
        or receipt["reference_report_sha256"] != digest(baseline / "comparison.json")
        or receipt["display"]["after_display_sha256"] != digest(path)
    ):
        raise ValueError("verified current whole diagnostic display required")
    return path


def run(args):
    commit, began = frozen(), time.perf_counter()
    fresh(args.output)
    if not 1 <= len(args.prior_module) <= 32:
        raise ValueError("bounded nonempty verified snapshot history required")
    state = load_chain(args.resident, args.prior_module)
    view, assets, _supports, _contacts, current = state
    original = json.loads((args.resident / "resident.json").read_text())
    root_rows = profiles(args, state)
    before = before_display(args.prior_module[-1], args.baseline)
    reference = baseline_binding(args, current)
    for name in ("world-layout.npz", "world-field.npz", "world-body.npz"):
        if digest(args.baseline / name) != reference["artifacts"][name]:
            raise ValueError("whole snapshot baseline field changed")
    equal = compare(
        view,
        read_arrays(args.baseline / "world-layout.npz"),
        read_arrays(args.baseline / "world-field.npz"),
    )
    args.output.mkdir()
    target, materialized = terrain(
        args.output,
        view,
        Path(original["terrain"]),
        args.prior_module,
        current["source_ids"],
        commit,
    )
    rows = roads(
        args.output, args.resident, args.prior_module, original, assets, root_rows
    )
    manifest = dict(
        original,
        commit=commit,
        roads=rows,
        source_ids=current["source_ids"],
        terrain=str(target.resolve()),
        terrain_sha256=digest(target / "patches.json"),
        catalog=current["catalog"],
        catalog_sha256=current["catalog_sha256"],
        before_display=str(before.resolve()),
        before_display_sha256=digest(before),
    )
    write_json(args.output / "resident.json", manifest)
    for name in ("world-layout.npz", "world-field.npz", "world-body.npz"):
        os.link(args.baseline / name, args.output / name)
    inputs = {
        "resident": str(args.resident.resolve()),
        "resident_sha256": digest(args.resident / "resident.json"),
        "prior_modules": [str(p.resolve()) for p in args.prior_module],
        "parent_reports": current["parent_reports"],
        "parent_receipts": [digest(p / "verification.json") for p in args.prior_module],
        "profile_products": str(args.profile_products.resolve()),
        "profile_manifest_sha256": digest(args.profile_products / "resident.json"),
        "profile_receipt": str(args.profile_receipt.resolve()),
        "profile_receipt_sha256": digest(args.profile_receipt),
        "baseline": str(args.baseline.resolve()),
        "baseline_report_sha256": digest(args.baseline / "comparison.json"),
    }
    artifacts = {
        p.relative_to(args.output).as_posix(): digest(p)
        for p in args.output.rglob("*")
        if p.is_file()
    }
    report = {
        "commit": commit,
        "inputs": inputs,
        "artifacts": artifacts,
        "equality": equal,
        "candidate_source_ids": current["source_ids"],
        "origin_lat_lon": current["origin_lat_lon"],
        "source_count": len(current["source_ids"]),
        "road_assets": len(rows),
        "root_profiles": len(root_rows),
        "newly_materialized_patch_file_bytes": materialized,
        "road_file_copy_bytes": 0,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Fresh prepared research snapshot, same immutable road payload hashes and field; "
        "not new geometry, a runtime storage format or new source admission.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("commit", "source_count", "root_profiles", "equality")
            }
        ),
        flush=True,
    )
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "resident",
        "profile-products",
        "profile-receipt",
        "baseline",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

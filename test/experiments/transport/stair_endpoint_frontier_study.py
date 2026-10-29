"""Select original sources from a geometric request and run bounded local admission."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_frontier_dispositions import source_dispositions
from stair_frontier_sources import proposals
from stair_local_context import state_for_axes
from stair_local_replay import load_chain
from stair_module_inputs import FIXTURE, source_context
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def select(args, *, method="index"):
    query = json.loads(args.query.read_text())
    xy = np.asarray(query["query_xy_m"], float)
    if (
        query.get("version") != 1
        or xy.ndim != 2
        or xy.shape[1] != 2
        or not 2 <= len(xy) <= 256
        or not np.isfinite(xy).all()
        or abs(xy).max() > 1e6
    ):
        raise ValueError("Bounded finite explicit request polyline required")
    reference = FIXTURE.with_name(query["reference_fixture"])
    if digest(reference) != query["reference_fixture_sha256"]:
        raise ValueError("Original request reference changed")
    context, sources, _, _ = source_context(reference)
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    if (
        context["origin_lat_lon"] != query["origin_lat_lon"]
        or context["origin_lat_lon"] != manifest["origin_lat_lon"]
    ):
        raise ValueError(
            "Original request, sources and actual resident frame must match"
        )
    state, _, _, work = state_for_axes(
        view, assets, supports, contacts, shapely.LineString(xy), allow_empty=True
    )
    ref = {k: query[k] for k in ("reference_fixture", "reference_fixture_sha256")}
    plan, records, roots, deferred, recipes = proposals(
        state, sources, ref, method=method, strategy="prefix"
    )
    return {
        "plan": plan,
        "roots": roots,
        "deferred": deferred,
        "recipes": recipes,
        "source_count": len(records),
        "eligible_sources": sum(r["eligible"] for r in records),
        "before_source_ids": manifest["source_ids"],
        "context": work,
        "resident_sha256": manifest["root_resident_sha256"],
        "parent_reports": manifest["parent_reports"],
    }


def child_command(args, output, fixture, script):
    command = [
        sys.executable,
        "-B",
        str(Path(__file__).with_name(script)),
        "--resident",
        str(args.resident),
        "--fixture",
        str(fixture),
        "--output",
        str(output),
    ]
    for prior in args.prior_module:
        command.extend(["--prior-module", str(prior)])
    return command


def run(args):
    commit = frozen()
    fresh(args.output)
    selection = select(args)
    scanned = select(args, method="scan")
    for key in (
        "roots",
        "deferred",
        "recipes",
        "before_source_ids",
        "resident_sha256",
        "parent_reports",
    ):
        if selection[key] != scanned[key]:
            raise ValueError("Indexed and full-scan source selection differ")
    for a, b in zip(
        selection["plan"]["proposals"], scanned["plan"]["proposals"], strict=True
    ):
        if {k: v for k, v in a.items() if k != "work"} != {
            k: v for k, v in b.items() if k != "work"
        }:
            raise ValueError("Indexed and full-scan original parent witnesses differ")
    args.output.mkdir()
    (args.output / "selection.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "query_sha256": digest(args.query),
                "index": selection,
                "scan": scanned,
            },
            indent=2,
        )
        + "\n"
    )
    attempts, chosen = [], None
    for index, recipe in enumerate(selection["recipes"][:3]):
        fixture = args.output / f"recipe-{index}.json"
        fixture.write_text(json.dumps(recipe, indent=2) + "\n")
        output = args.output / f"outshine-module-{index}"
        with (args.output / f"generate-{index}.log").open("w") as log:
            result = subprocess.run(
                child_command(args, output, fixture, "stair_local_generate.py"),
                stdout=log,
                check=False,
                stderr=subprocess.STDOUT,
            )
        if result.returncode:
            raise RuntimeError(
                f"Local generator process failed; original baseline retained: {index}, exit {result.returncode}"
            )
        with (args.output / f"verify-{index}.log").open("w") as log:
            result = subprocess.run(
                child_command(args, output, fixture, "stair_local_verify.py"),
                stdout=log,
                check=False,
                stderr=subprocess.STDOUT,
            )
        if result.returncode:
            raise RuntimeError(
                f"Independent local reader failed: {index}, exit {result.returncode}"
            )
        report = json.loads((output / "comparison.json").read_text())
        reader = json.loads((output / "verification.json").read_text())
        if report["fixture_sha256"] != digest(fixture) or reader[
            "products_report_sha256"
        ] != digest(output / "comparison.json"):
            raise ValueError("Actual automatic recipe and fresh reader binding changed")
        attempts.append(
            {
                "index": index,
                "source_ids": recipe["source_ids"],
                "valid_local_module": reader["valid_local_module"],
                "reason": report.get("reason", ""),
                "product_sha256": digest(output / "comparison.json"),
                "reader_sha256": digest(output / "verification.json"),
            }
        )
        if reader["valid_local_module"]:
            chosen = index
            break
    report = {
        "commit": commit,
        "query_sha256": digest(args.query),
        "selection_sha256": digest(args.output / "selection.json"),
        "attempts": attempts,
        "chosen_module": chosen,
        "valid_automatic_module": chosen is not None,
        "source_dispositions": source_dispositions(selection["plan"], attempts, chosen),
        "scope": "Bounded whole-source proposals from the original geometric request; each actual product independently read. No native/routing/world/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": commit,
                "chosen_module": chosen,
                "attempts": len(attempts),
                "output": str(args.output),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "query", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

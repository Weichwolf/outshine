"""Generate a bounded ground extension through an existing whole cached interface."""

import argparse
import json
import resource
import shutil
import tempfile
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_module_audit import audit
from stair_module_cache import load_cached
from stair_module_catalog import catalog
from stair_module_display import display
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import FIXTURE, inputs
from stair_module_soil import assemble, fit, prepare
from stair_module_world import inspect_world, source_ground
from stair_terrain_experiment import frozen


def write_json(path, value):
    def convert(v):
        if isinstance(v, np.ndarray):
            return v.tolist()
        if isinstance(v, np.generic):
            return v.item()
        raise TypeError(type(v).__name__)

    path.write_text(json.dumps(value, indent=2, default=convert) + "\n")


def save_planning(output, rows, planning, inventory):
    saved_rows = [{k: v for k, v in row.items() if k != "original"} for row in rows]
    write_json(
        output / "planning.json",
        {
            "rows": saved_rows,
            "nodes": planning[0],
            "masks_wkt": [
                shapely.to_wkt(p, rounding_precision=-1) for p in planning[3]
            ],
        },
    )
    write_json(output / "source-catalog.json", inventory)


def announce(report):
    fields = ("commit", "valid_local_module", "seconds", "peak_rss_bytes")
    print(json.dumps({key: report[key] for key in fields}), flush=True)


def artifact_hashes(output, arrays):
    names = [*arrays, "planning.json", "source-catalog.json"]
    names.extend(
        phase + "-" + view + "-scene.json"
        for phase in ("before", "after")
        for view in ("overview", "network")
    )
    return {name: digest(output / name) for name in names}


def module_inputs(args, timed):
    recipe, rows, context, sources, sample, layer = timed(
        "new_provider", lambda: inputs(args.fixture)
    )
    state = timed("cached_inputs", lambda: load_cached(args, recipe))
    if state["support_report"]["origin_lat_lon"] != context["origin_lat_lon"]:
        raise ValueError("cached module origin/frame changed")
    return recipe, rows, context, sources, sample, layer, state


def run(args):
    commit = frozen()
    if args.output.exists():
        raise ValueError("fresh explicit temporary module output required")
    started = time.perf_counter()
    times = {}

    def timed(name, call):
        begin = time.perf_counter()
        result = call()
        times[name] = time.perf_counter() - begin
        return result

    recipe, rows, context, sources, sample, layer, state = module_inputs(args, timed)
    old, line, ground, upper = timed(
        "cached_surface_selection", lambda: old_surfaces(state, recipe, sources)
    )
    data, network_note, planning = timed(
        "new_surface_and_body",
        lambda: build(
            rows,
            sources,
            old,
            line,
            ground,
            sample,
            contact_mode=recipe.get("contact_mode", "flat"),
        ),
    )
    geometry = timed(
        "module_admission",
        lambda: audit(state, recipe, data, rows, planning, line, upper),
    )
    local, chosen, terrain_note = timed(
        "local_terrain_plan", lambda: prepare(state, data, ground, recipe, sample)
    )
    local["variable_mode"] = np.array(args.variable_mode)
    actual_ground = source_ground(ground, data)
    footprint = shapely.union_all(
        shapely.polygons(actual_ground.vertices[actual_ground.triangles, :2])
    )
    local_h, local_roles, solve_note = timed(
        "local_terrain_fit", lambda: fit(local, upper, footprint)
    )
    plan, heights, body, parts, assembly = timed(
        "world_diagnostic_assembly", lambda: assemble(state, local, local_h, chosen)
    )
    roles, world = timed(
        "whole_world_admission",
        lambda: inspect_world(
            state,
            local,
            local_h,
            chosen,
            plan,
            heights,
            body,
            parts,
            actual_ground,
            upper,
            footprint,
            expected_local_roles=local_roles,
        ),
    )
    inventory = timed(
        "new_catalog_overlaps",
        lambda: catalog(args.base, state, rows, sources, layer, planning[1], context),
    )
    args.output.mkdir(parents=True)
    arrays = {
        "module.npz": data,
        "local-layout.npz": local,
        "local-field.npz": {"heights": local_h, "constructed": local_roles},
        "world-layout.npz": plan,
        "world-field.npz": {"heights": heights, "constructed": roles},
        "world-body.npz": {"vertices": body.vertices, "faces": body.triangles, **parts},
        "selection.npz": {"old_selected_faces": chosen},
    }
    for name, values in arrays.items():
        np.savez_compressed(args.output / name, **values)
    save_planning(args.output, rows, planning, inventory)
    shown = timed(
        "display_arrays",
        lambda: display(
            args.output, args.base, args.supports, state, data, body, parts, roles
        ),
    )
    artifacts = artifact_hashes(args.output, arrays)
    report = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "base_report_sha256": state["baseline_report_sha256"],
        "support_report_sha256": state["support_report_sha256"],
        "cache_parent_reports": state["cache_parent_reports"],
        "origin_lat_lon": context["origin_lat_lon"],
        "network": network_note,
        "module": geometry,
        "local_terrain": terrain_note,
        "solve": solve_note,
        "assembly": assembly,
        "world": world,
        "display": shown,
        "artifacts": artifacts,
        "phase_seconds": times,
        "seconds": time.perf_counter() - started,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "source_work": {
            "cached_sources": len(state["report"]["after"]["source_ids"]),
            "new_sources": len(rows),
        },
        "candidate_source_ids": state["report"]["candidate_source_ids"]
        + [r["source_id"] for r in rows],
        "admitted_source_ids": [],
        "valid_local_module": True,
        "scope": "Explicit locally flat cached interface; bounded whole-source ground extension with actual old bodies retained. Global terrain assembly is diagnostic work. No complete/native world, statutory vehicle envelope or structural acceptance.",
    }
    write_json(args.output / "comparison.json", report)
    announce(report)
    return report


def refused_catalog(base, recipe, reason, *, catalog_path=None):
    inventory = json.loads((catalog_path or base / "source-catalog.json").read_text())
    keys = recipe["source_ids"]
    unused = {row["source_id"]: row for row in inventory["unadmitted_sources"]}
    if (
        not isinstance(keys, list)
        or not 1 <= len(keys) <= 8
        or len(set(keys)) != len(keys)
        or not set(keys) <= unused.keys()
    ):
        raise ValueError("refused module must identify original unused sources")
    for key in keys:
        unused[key]["reason"] = "module_recipe_rejected"
        unused[key]["failed_recipe_trial"] = {"source_id": key, "reason": reason}
    return inventory


def generate(args):
    if args.output.exists():
        raise ValueError("fresh explicit temporary module output required")
    if not args.output.resolve().is_relative_to(
        tempfile.gettempdir()
    ) or not args.output.name.startswith("outshine-"):
        raise ValueError(
            "private module output must be an outshine-* system-temp directory"
        )
    started = time.perf_counter()
    commit = frozen()
    try:
        return run(args)
    except ValueError as error:
        recipe = json.loads(args.fixture.read_text())
        state = load_cached(args, recipe)
        reason = str(error)[:2048]
        inventory = refused_catalog(
            args.base, recipe, reason, catalog_path=state["catalog_path"]
        )
        # run accepted only a fresh private output; any contents are this attempt's.
        if args.output.exists():
            shutil.rmtree(args.output)
        args.output.mkdir(parents=True)
        write_json(args.output / "source-catalog.json", inventory)
        report = {
            "commit": commit,
            "fixture_sha256": digest(args.fixture),
            "base_report_sha256": state["baseline_report_sha256"],
            "support_report_sha256": state["support_report_sha256"],
            "cache_parent_reports": state["cache_parent_reports"],
            "valid_local_module": False,
            "valid_retained_baseline": True,
            "refused_sources": recipe["source_ids"],
            "reason": reason,
            "candidate_source_ids": state["report"]["candidate_source_ids"],
            "admitted_source_ids": [],
            "artifacts": {
                "source-catalog.json": digest(args.output / "source-catalog.json")
            },
            "seconds": time.perf_counter() - started,
            "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        }
        write_json(args.output / "comparison.json", report)
        announce(report)
        return report


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=FIXTURE)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    parser.add_argument(
        "--cached-module",
        type=Path,
        nargs=2,
        action="append",
        default=[],
        metavar=("OUTPUT", "FIXTURE"),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--variable-mode",
        choices=("heights", "fixed_displacement"),
        default="fixed_displacement",
    )
    return parser


def arguments():
    return argument_parser().parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    generate(arguments())

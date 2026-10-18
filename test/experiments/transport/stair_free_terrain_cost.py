"""Equal-input terrain recipes with physical checks and identical DEM controls."""

import argparse
import contextlib
import json
import resource
import statistics
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_local_context import selected_keys, state_for
from stair_local_cost import result_hash
from stair_local_physics import piers, validate
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_family import audit, surfaces
from stair_module_inputs import inputs, read_arrays
from stair_module_soil import fit, prepare
from stair_module_world import source_ground
from stair_patch_inputs import from_field
from stair_seed_geometry import prepared
from stair_terrain_experiment import frozen


def preload(args):
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    recipe, rows, context, sources, sample, _layer = inputs(args.fixture)
    report = json.loads((args.products / "comparison.json").read_text())
    receipt = json.loads((args.products / "verification.json").read_text())
    if (
        receipt.get("valid_local_module") is not True
        or receipt["products_report_sha256"]
        != digest(args.products / "comparison.json")
        or report["fixture_sha256"] != digest(args.fixture)
        or report["before_source_ids"] != manifest["source_ids"]
        or report["parent_reports"] != manifest["parent_reports"]
        or report["resident_sha256"] != manifest["root_resident_sha256"]
        or context["origin_lat_lon"] != manifest["origin_lat_lon"]
        or report["artifacts"]["module.npz"] != digest(args.products / "module.npz")
    ):
        raise ValueError("verified same-source body and original resident required")
    data = read_arrays(args.products / "module.npz")
    state, selected, _axes, _note = state_for(
        view, assets, supports, contacts, sources, recipe
    )
    _old, line, ground, upper = surfaces(state, recipe, sources)
    planning = prepared(rows, sources, ground)
    audit(state, recipe, data, rows, planning, line, upper, sources=sources)
    reference = source_ground(ground, data)
    footprint = shapely.union_all(
        shapely.polygons(reference.vertices[reference.triangles, :2])
    )
    return (
        view,
        contacts,
        state,
        selected,
        data,
        ground,
        upper,
        recipe,
        sample,
        footprint,
    )


def construct(context, mode):
    _view, _contacts, state, _selected, data, ground, upper, recipe, sample, foot = (
        context
    )
    local, chosen, terrain = prepare(
        state, data, ground, dict(recipe, terrain_layout_mode=mode), sample
    )
    height, roles, solve = fit(local, upper, foot)
    arrays = {"layout": local, "field": {"heights": height, "constructed": roles}}
    return arrays, chosen, terrain, solve


def qualify(context, arrays, chosen, controls):
    view, contacts, state, selected, data, _ground, upper, _recipe, sample, _foot = (
        context
    )
    local, field = arrays["layout"], arrays["field"]
    new_foot = shapely.union_all(
        shapely.polygons(data["surface_vertices"][data["surface_faces"], :2])
    )
    removals = selected_keys(view, selected, chosen, new_foot, contacts)
    updated, replacement = view.replace(
        from_field(local, field, view.patches[0].frame), removals
    )
    physical = validate(
        updated,
        state,
        local,
        field["heights"],
        field["constructed"],
        chosen,
        data,
        upper,
    )
    mesh = Mesh(np.c_[local["xy"], field["heights"]], local["faces"])
    heights = (
        SurfaceSampler(mesh, xy_tolerance_m=1e-7)
        .sample(controls, max_queries=16384, max_candidates=2000000)
        .points_enu_m[:, 2]
    )
    if not np.isfinite(heights).all():
        raise ValueError("both actual fields must cover every identical control point")
    difference = heights - sample(controls)
    return heights, {
        "physical": physical,
        "piers": piers(updated, contacts),
        "replacement": replacement,
        "control_points": len(controls),
        "DEM_max_abs_m": float(abs(difference).max()),
        "DEM_MAE_m": float(abs(difference).mean()),
        "DEM_RMSE_m": float(np.sqrt(np.mean(difference**2))),
    }


def run(args):
    commit = frozen()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(Path("/tmp"))
        or args.repeats not in (1, 3, 5)
    ):
        raise ValueError("fresh explicit /tmp output and bounded repetitions required")
    context = preload(args)
    args.output.mkdir()
    measurements, values, controls, expected_chosen = {}, {}, None, None
    for mode in ("legacy", "free_region"):
        times, expected = [], None
        for _repeat in range(args.repeats):
            began = time.perf_counter()
            arrays, chosen, terrain, solve = construct(context, mode)
            times.append(time.perf_counter() - began)
            actual = result_hash(arrays)
            if expected is not None and actual != expected:
                raise ValueError("repeated terrain recipe changed its result")
            expected = actual
        tracker = contextlib.nullcontext()
        if args.profile:
            import memray

            tracker = memray.Tracker(
                str(args.output / (mode + "-profile.bin")),
                trace_python_allocators=True,
                native_traces=False,
            )
        if args.profile:
            with tracker:
                traced = construct(context, mode)
            if result_hash(traced[0]) != expected:
                raise ValueError("profiled terrain recipe changed its result")
        old = context[2]["plan"]
        if controls is None:
            expected_chosen = chosen.copy()
            controls = np.vstack(
                [
                    old["xy"][np.unique(old["faces"][chosen])],
                    old["xy"][old["faces"][chosen]].mean(axis=1),
                ]
            )
        if not np.array_equal(chosen, expected_chosen):
            raise ValueError("terrain alternatives selected different original regions")
        values[mode], quality = qualify(context, arrays, chosen, controls)
        measurements[mode] = {
            "seconds": times,
            "median_seconds": statistics.median(times),
            "result_sha256": expected,
            "vertices": len(arrays["layout"]["xy"]),
            "faces": len(arrays["layout"]["faces"]),
            "terrain": terrain,
            "solve": solve,
            **quality,
        }
        np.savez_compressed(
            args.output / (mode + "-field.npz"), **arrays["layout"], **arrays["field"]
        )
    difference = values["free_region"] - values["legacy"]
    report = {
        "commit": commit,
        "body_sha256": digest(args.products / "module.npz"),
        "fixture_sha256": digest(args.fixture),
        "resident_sha256": digest(args.resident / "resident.json"),
        "measurements": measurements,
        "same_selected_original_region": True,
        "identical_control_points": len(controls),
        "maximum_height_difference_m": float(abs(difference).max()),
        "mean_abs_height_difference_m": float(abs(difference).mean()),
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "profiled": args.profile,
        "scope": "Same verified whole body and original baseline. Only local terrain preparation and fit timed/traced; preload, body audit, full physical/whole-pier checks, sampling, hashes and exports excluded. Shapes may differ: DEM residuals are reported at identical points. Requested bytes are not copy traffic; process RSS includes preload. No runtime/device or equal-geometry cost claim.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "commit",
                    "identical_control_points",
                    "maximum_height_difference_m",
                )
            }
        ),
        flush=True,
    )
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "fixture", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--repeats", type=int, choices=(1, 3, 5), default=3)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(parser.parse_args())

"""Equal-recipe indexed/scanned resident selection, with optional allocation traces."""

import argparse
import hashlib
import json
import resource
import time
from functools import partial
from pathlib import Path

from camera_height_inputs import digest
from stair_frontier_experiment import reference
from stair_growth_sources import select
from stair_local_controls import fingerprint
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import FIXTURE, source_context
from stair_patch_inputs import fresh
from stair_seed_sources import select as select_seed
from stair_terrain_experiment import frozen


def compare(args):
    commit = frozen()
    fresh(args.output)
    view, assets, _supports, _contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    context, sources, _sample, _layer = source_context(args.reference)
    if context["origin_lat_lon"] != manifest["origin_lat_lon"]:
        raise ValueError("source and actual resident frames differ")
    ref, before = reference(args), fingerprint(view, assets)
    args.output.mkdir()
    results, selected = {}, {}
    selector = (
        partial(select_seed, grid_m=args.seed_grid_m)
        if args.family == "seed"
        else select
    )
    for method in ("index", "scan"):
        start = time.perf_counter()
        selected[method] = selector(view, assets, manifest, sources, ref, method=method)
        results[method] = {
            "seconds": time.perf_counter() - start,
            "profile_work": selected[method]["profile_work"],
            "plan": selected[method]["plan"],
        }
    for key in ("roots", "deferred_profiles", "recipes"):
        if selected["index"][key] != selected["scan"][key]:
            raise ValueError("indexed/scanned original-source selection differs")
    if (
        selected["index"]["plan"]["source_reasons"]
        != selected["scan"]["plan"]["source_reasons"]
    ):
        raise ValueError("indexed/scanned source dispositions differ")
    if args.profile:
        import memray

        for method in ("index", "scan"):
            start = time.perf_counter()
            with memray.Tracker(
                str(args.output / (method + "-profile.bin")),
                trace_python_allocators=True,
                native_traces=False,
            ):
                actual = selector(view, assets, manifest, sources, ref, method=method)
            if actual != selected[method]:
                raise ValueError("profiled/unprofiled selection differs")
            results[method]["profiled_seconds"] = time.perf_counter() - start
    if fingerprint(view, assets) != before:
        raise ValueError("resident geometry changed during source selection")
    report = {
        "commit": commit,
        "resident_sha256": digest(args.resident / "resident.json"),
        "parent_reports": manifest["parent_reports"],
        "reference": ref,
        "measurements": results,
        "same_roots_recipes_reasons": True,
        "unchanged_geometry_sha256": before,
        "recipe_sha256": hashlib.sha256(
            json.dumps(selected["index"]["recipes"], sort_keys=True).encode()
        ).hexdigest(),
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Selection only; verified/source preload and geometry fingerprint outside timers/tracker. Process RSS includes preload; not copied bytes/device/runtime proof.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps({"same_recipes": True, "recipe_sha256": report["recipe_sha256"]}),
        flush=True,
    )
    return report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    parser.add_argument("--family", choices=("attached", "seed"), default="attached")
    parser.add_argument(
        "--seed-grid-m", type=float, choices=(0.0, 2**-11), default=2**-11
    )
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    compare(arguments())

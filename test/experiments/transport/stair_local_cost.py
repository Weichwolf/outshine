"""Equal-result local/whole construction costs, with optional native/Python allocation traces."""

import argparse
import contextlib
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

from stair_local_generate import construct as local_construct
from stair_local_reference import construct as whole_construct
from stair_local_reference import reference_state
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import inputs
from stair_terrain_experiment import frozen


def result_hash(arrays):
    digest = hashlib.sha256()
    for name, data in sorted(arrays.items()):
        for key, value in sorted(data.items()):
            digest.update(
                (
                    name + ":" + key + ":" + str(value.shape) + ":" + value.dtype.str
                ).encode()
            )
            digest.update(value.tobytes())
    return digest.hexdigest()


def run(args):
    commit = frozen()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(tempfile.gettempdir())
        or (
            args.profile
            and (
                args.profile.exists()
                or not args.profile.resolve().is_relative_to(tempfile.gettempdir())
            )
        )
    ):
        raise ValueError("fresh explicit cost report and allocation trace required")
    load_start = time.perf_counter()
    recipe, rows, _context, sources, sample, _layer = inputs(args.fixture)
    if args.mode == "local":
        view, assets, supports, contacts, manifest = load_chain(
            args.resident, args.prior_module
        )
    else:
        state, manifest, _road_note = reference_state(args)
    load_seconds = time.perf_counter() - load_start
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=False
        )
    with tracker:
        began = time.perf_counter()
        if args.mode == "local":
            result = local_construct(
                view,
                assets,
                supports,
                contacts,
                recipe,
                rows,
                sources,
                sample,
                height_floor_m=manifest["height_floor_m"],
            )
            arrays = {
                "module.npz": result["data"],
                "local-layout.npz": result["local"],
                "local-field.npz": result["field"],
            }
            proof = result["proof"]
        else:
            built, proof = whole_construct(state, recipe, rows, sources, sample)
            arrays = {
                k: built[k]
                for k in ("module.npz", "local-layout.npz", "local-field.npz")
            }
        seconds = time.perf_counter() - began
    report = {
        "commit": commit,
        "mode": args.mode,
        "result_sha256": result_hash(arrays),
        "source_and_resident_load_seconds": load_seconds,
        "construction_seconds": seconds,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "profiled": bool(args.profile),
        "proof": proof,
        "scope": "Same whole-source module/local field after source/resident preloading. Local proof preserves unchanged outside; whole control reconstructs/rechecks full terrain. Allocation trace excludes inputs/imports/hash/export, includes native/Python allocations without native call stacks; not GPU/frame/native runtime acceptance.",
    }
    write_json(args.output, report)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "mode",
                    "result_sha256",
                    "construction_seconds",
                    "peak_rss_bytes",
                    "profiled",
                )
            }
        ),
        flush=True,
    )
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "baseline", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mode", choices=("local", "whole"), required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())

"""Same whole77 volume output: fit every facet or reuse a verified immutable prefix."""

import argparse
import hashlib
import json
import resource
import time
from contextlib import nullcontext
from pathlib import Path

from stair_local_replay import load_chain
from stair_occupancy_experiment import prepare_volumes
from stair_occupancy_inputs import actual_sources
from stair_terrain_experiment import frozen


def measured(args, state, sources, method, trace=None):
    if trace is None:
        tracker = nullcontext()
    else:
        import memray

        tracker = memray.Tracker(
            str(trace), trace_python_allocators=True, native_traces=False
        )
    _view, assets, supports, contacts, manifest = state
    with tracker:
        began = time.perf_counter()
        rows, owners, error, report = prepare_volumes(
            assets,
            supports,
            contacts,
            sources,
            manifest,
            args.prior_module,
            args.prior_occupancy if method == "reuse" else None,
        )
        seconds = time.perf_counter() - began
    sha = hashlib.sha256()
    for row, keys in zip(rows, owners, strict=True):
        sha.update(row.geometry.wkb)
        sha.update(
            json.dumps(
                [
                    row.owner_id,
                    row.purpose,
                    row.origin_xy_m,
                    row.lower_plane_m,
                    row.upper_plane_m,
                    keys,
                ],
                separators=(",", ":"),
            ).encode()
        )
    return {
        "method": method,
        "seconds": seconds,
        "result_sha256": sha.hexdigest(),
        "volume_count": len(rows),
        "preparation": report["preparation"],
        "headroom_work": report["work"],
        "physical_plane_padding_m": error,
    }


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("explicit private /tmp cost output required")
    args.output.mkdir(parents=True, exist_ok=False)
    state = load_chain(args.resident, args.prior_module)
    _, sources, *_ = actual_sources()
    runs = [
        measured(args, state, sources, method)
        for method in ("fresh", "reuse", "reuse", "fresh")
    ]
    profiles = {}
    if args.profile:
        for method in ("fresh", "reuse"):
            path = args.output / (method + ".bin")
            profiles[method] = measured(args, state, sources, method, path)
    if len({r["result_sha256"] for r in [*runs, *profiles.values()]}) != 1:
        raise ValueError("same-input preparation paths changed the volume output")
    report = {
        "commit": commit,
        "source_count": len(state[-1]["source_ids"]),
        "runs": runs,
        "profiles": profiles,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Input sources/assets preloaded. Timers and allocation traces cover preparation only, including prefix JSON decode/validation; output digest/export excluded. Process RSS spans both alternatives/preload, not separate per-method peaks. Background load and profiler overhead require separate interpretation. No runtime/device/world-load claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--prior-occupancy", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", action="store_true")
    run(parser.parse_args())

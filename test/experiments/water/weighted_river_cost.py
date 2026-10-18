"""Fresh-process cost of each explicit objective under the same directed constraints."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from pathlib import Path

from directional_river_fit import fit_surface
from rail_instance_cost import profile_note
from stair_terrain_experiment import frozen
from weighted_river_compare import inputs
from weighted_river_fit import fit_weighted_surface


def run(args):
    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh private /tmp output required")
    args.output.mkdir(parents=True)
    mesh, flow, rows, _source, weights, *_rest = inputs(args.input, 20.0, "area")
    tracker = contextlib.nullcontext()
    trace = args.output / "allocations.bin"
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(trace), trace_python_allocators=True, native_traces=False
        )
    start = time.perf_counter()
    with tracker:
        result = (
            fit_surface(mesh, flow, directed_rows=rows)
            if args.method == "minimax"
            else fit_weighted_surface(mesh, flow, directed_rows=rows, weights=weights)
        )
    elapsed = time.perf_counter() - start
    if result.mesh is None:
        raise ValueError("bounded fit failed: " + str(dict(result.report)))
    allocation = profile_note(trace) if args.profile else None
    if allocation:
        allocation.update(
            native_traces=False,
            scope="One whole bounded fit after imports, original input decode/mesh preparation. Cumulative allocation requests are not copied/live bytes; traced heap excludes existing inputs.",
        )
    report = {
        "commit": commit,
        "method": args.method,
        "profiled": args.profile,
        "elapsed_seconds": elapsed,
        "fit": dict(result.report),
        "vertices": len(mesh.vertices),
        "faces": len(mesh.triangles),
        "output_vertices_sha256": hashlib.sha256(result.mesh.vertices).hexdigest(),
        "explicit_mesh_copy_bytes": 2 * result.mesh.vertices.nbytes
        + result.mesh.triangles.nbytes,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "objective_weight_bytes": weights.nbytes,
        "allocations": allocation,
        "scope": "Different objectives, same feasible directional set. Normal process priority/shared host; no solver/objective-independent speed claim or target-device admission.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("minimax", "weighted"), required=True)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())

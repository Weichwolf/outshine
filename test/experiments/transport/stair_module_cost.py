"""Whole equal-quality module construction and explicit optional allocation tracing."""

import contextlib
import json
import resource
import subprocess
import sys
from pathlib import Path

from stair_module_experiment import argument_parser, generate, write_json
from stair_terrain_experiment import frozen


def measure(args):
    commit = frozen()
    if args.output.exists():
        raise ValueError("fresh explicit module cost output required")
    tracker = contextlib.nullcontext()
    if args.profile:
        if args.profile.exists():
            raise ValueError("fresh explicit allocation trace required")
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    with tracker:
        report = generate(args)
    if not report["valid_local_module"]:
        raise ValueError(
            "cost variant retained only the old world; no equal-quality module"
        )
    note = {
        "commit": commit,
        "variable_mode": args.variable_mode,
        "generation_seconds": report["seconds"],
        "generation_peak_rss_bytes": report["peak_rss_bytes"],
        "phase_seconds": report["phase_seconds"],
        "source_work": report["source_work"],
        "scope": "Complete generate call after imports: new provider, cached inputs, geometry/fields, all actual module/world/pier checks, inventory, display arrays, serialization and report. PNG rendering and independent reader excluded. Normal and profiled times stay separate; internal total copies remain unobserved.",
    }
    if args.profile:
        stats_path = args.output / "allocation-stats.json"
        with (args.output / "allocation-stats.log").open("w") as log:
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "memray",
                    "stats",
                    "--json",
                    "-o",
                    str(stats_path),
                    str(args.profile),
                ],
                check=True,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=120,
            )
        stats = json.loads(stats_path.read_text())
        metadata = stats["metadata"]
        if (
            not metadata["has_native_traces"]
            or not metadata["trace_python_allocators"]
            or not metadata["end_time"]
        ):
            raise ValueError("complete native/Python allocation trace required")
        note["allocations"] = {
            "profiler": "memray",
            "version": memray.__version__,
            "allocation_events": stats["total_num_allocations"],
            "cumulative_requested_bytes": stats["total_bytes_allocated"],
            "tracked_peak_heap_bytes": metadata["peak_memory"],
            "scope": note["scope"],
            "statistics": "Official native stats CLI in a separate bounded process; requested activity is not unique/live memory.",
        }
    note["process_peak_after_statistics_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    write_json(args.output / "cost.json", note)
    return note


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argument_parser()
    parser.description = __doc__
    parser.add_argument("--profile", type=Path)
    measure(parser.parse_args())

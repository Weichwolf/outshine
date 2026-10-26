"""Compare owned sleeper-instance buffers with fully expanded equivalent geometry."""

import argparse
import contextlib
import gc
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from outshine_ref.transport.rail_hardware import bake_rail_sleepers as bake_sleepers
from outshine_ref.transport.rail_hardware._geometry import SLEEPER


def input_transforms(products):
    receipt = json.loads((products / "generation.json").read_text())
    values = []
    for name, row in receipt["cases"].items():
        path = products / (name + ".npz")
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["mesh_sha256"]:
            raise ValueError("measured hardware input changed")
        with np.load(path) as data:
            values.append(data["sleeper_transforms"])
    transforms = np.concatenate(values)
    transforms.setflags(write=False)
    return transforms, receipt["generation_commit"]


def profile_note(path):
    import memray

    reader = memray.FileReader(path)
    free = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    count, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in free and event.size > 0:
            count += event.n_allocations
            requested += event.size
    return {
        "profiler": "memray",
        "version": memray.__version__,
        "trace_python_allocators": True,
        "native_traces": True,
        "allocation_events": count,
        "cumulative_requested_bytes": requested,
        "tracked_peak_heap_bytes": reader.metadata.peak_memory,
        "scope": "One representation construction after imports/input decode. Allocation events include realloc requests; cumulative bytes are not unique/live memory. Profiling times are separate from normal runs. Internal copy totals are not observed.",
    }


def measure(transforms, mode, profile):
    tracker = contextlib.nullcontext()
    if profile is not None:
        import memray

        tracker = memray.Tracker(
            profile, trace_python_allocators=True, native_traces=True
        )
    times, output = [], None
    for _ in range(1 if profile else 7):
        del output
        gc.collect()
        began = time.perf_counter()
        with tracker:
            if mode == "instances":
                output = np.array(transforms, copy=True)
                output.setflags(write=False)
            else:
                output = bake_sleepers(transforms)
        times.append((time.perf_counter() - began) * 1000)
    count = len(transforms)
    output_bytes = (
        output.nbytes + SLEEPER.vertices.nbytes + SLEEPER.triangles.nbytes
        if mode == "instances"
        else output.vertices.nbytes + output.triangles.nbytes
    )
    return {
        "mode": mode,
        "sleepers": count,
        "phase_ms": times,
        "profiled": profile is not None,
        "output_payload_bytes": output_bytes,
        "explicit_owned_copy_bytes_per_call": transforms.nbytes
        if mode == "instances"
        else output_bytes,
        "expanded_vertices_per_call": 0 if mode == "instances" else 8 * count,
        "remapped_triangle_indices_per_call": 0 if mode == "instances" else 36 * count,
        "logical_transform_multiplications_per_call": 0
        if mode == "instances"
        else 72 * count,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "input_transform_sha256": hashlib.sha256(transforms).hexdigest(),
        "scope": "Owned representation only; source/rail/contact generation and input decoding excluded from phase. Equivalent geometry verified separately over every saved real instance. Deferred GPU vertex transforms and rendering are not free and are not measured here.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("instances", "baked"), required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    if args.output.exists() or (args.profile and args.profile.exists()):
        parser.error("fresh external report/profile required")
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    transforms, commit = input_transforms(args.products)
    result = measure(transforms, args.mode, args.profile)
    result["generation_commit"] = commit
    if args.profile:
        result["allocations"] = profile_note(args.profile)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()

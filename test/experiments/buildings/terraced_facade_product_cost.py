"""Same-input polygonal/batched/reused exterior costs with optional original heap traces."""

import argparse
from contextlib import nullcontext
import gc
import hashlib
import json
from pathlib import Path
import resource
import time
from unittest.mock import patch

import numpy as np
import shapely

from demand import View
from evidence import revision
from terraced_facade_geometry import build_facade
import terraced_facade_flat as flat
from terraced_facade_prepare import prepare
from terraced_facade_product_compare import reference
from terraced_facade_products import FacadeProducts
import terraced_facade_surface as surface
from terraced_mass_plan import plan
from terraced_mass_sources import sources


def payload(value):
    if hasattr(value, "floors"):
        arrays = [
            a
            for part in (value.floors.shell, value.floors.stairs, value.floors.landing)
            for a in (
                *part.mesh.values(),
                part.vertex_roots,
                part.source_indices,
                part.source_triangles,
            )
        ]
        arrays.extend(value.frames.values())
        triangles = len(value.frames["faces"]) + sum(
            len(p.mesh["faces"])
            for p in (value.floors.shell, value.floors.stairs, value.floors.landing)
        )
    else:
        arrays = [*value.mesh.values(), value.triangle_owners]
        triangles = len(value.mesh["faces"])
    unique = {(int(a.__array_interface__["data"][0]), a.nbytes): a for a in arrays}
    return {
        "output_numpy_bytes": sum(a.nbytes for a in unique.values()),
        "output_numpy_buffers": len(unique),
        "triangles": triangles,
    }


def run(args):
    mass = plan(sources()[args.source], args.programme)
    began = time.perf_counter()
    prepared = prepare(mass)
    preparation = time.perf_counter() - began
    module = reference(args.reference_module) if args.method == "polygonal" else surface
    owner_setup = None
    if args.method == "near":
        call = lambda: build_facade(mass, prepared=prepared)
    elif args.method == "reuse":
        began = time.perf_counter()
        owner = FacadeProducts(prepared)
        owner_setup = time.perf_counter() - began
        centre = np.r_[
            shapely.from_wkb(mass.source_wkb).centroid.coords[0], mass.top_m / 2
        ]
        view = View(centre + [0, -1400, 700], centre, width=1920, height=1080)
        call = lambda: owner.resolve(view)[0]
    else:
        call = lambda: module.compact(prepared)
    times, value = [], None
    for _ in range(args.repetitions):
        del value
        gc.collect()
        cpu, began = time.process_time(), time.perf_counter()
        value = call()
        times.append(
            {
                "wall_seconds": time.perf_counter() - began,
                "process_cpu_seconds": time.process_time() - cpu,
            }
        )
    result = payload(value)
    del value
    gc.collect()
    if args.trace is not None:
        import memray

        tracker = memray.Tracker(
            args.trace, trace_python_allocators=True, native_traces=False
        )
    else:
        tracker = nullcontext()
    with tracker:
        profiled = call()
    if payload(profiled) != result:
        raise ValueError("Profiled replay changed the measured output payload")
    del profiled
    work = {
        "direct_polygon_faces": 0,
        "direct_polygon_input_coordinate_bytes": 0,
        "flat_template_builds": 0,
    }
    compile_call, template_call = module.compile_faces, flat.template

    def compiled(vertices, faces, *a, **kw):
        work["direct_polygon_faces"] += len(faces)
        work["direct_polygon_input_coordinate_bytes"] += vertices.nbytes
        return compile_call(vertices, faces, *a, **kw)

    def template(*a, **kw):
        work["flat_template_builds"] += 1
        return template_call(*a, **kw)

    with (
        patch.object(module, "compile_faces", compiled),
        patch.object(flat, "template", template),
    ):
        counted = call()
    if payload(counted) != result:
        raise ValueError("Work-counter replay changed the measured payload")
    return (
        revision()
        | result
        | {
            "source_index": args.source,
            "programme": args.programme,
            "method": args.method,
            "windows": len(prepared.facade.windows),
            "facade_preparation_seconds": preparation,
            "resident_owner_setup_seconds": owner_setup,
            "reference_module_sha256": hashlib.sha256(
                args.reference_module.read_bytes()
            ).hexdigest()
            if args.reference_module
            else None,
            "unprofiled_runs": times,
            "work_counter_replay": work,
            "trace": str(args.trace) if args.trace else None,
            "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
            "scope": "Linux fresh process. Imports/source/mass planning excluded from timed build; facade preparation separate. Polygonal and batch have equivalent metrical window coverage, near has additional interior/depth quality. Reuse includes a camera selection query on an already owned coarse product; no changed source. Heap trace is a separate build with Python allocator hooks and native allocations, without native stacks; profiler time is not a normal build time. RSS spans the entire worker including profiler/counter replay. Final compact immutability writes equal its output NumPy bytes; other copy totals and GPU/device times remain unknown.",
        }
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=int, choices=range(4), required=True)
    parser.add_argument(
        "--programme", choices=("cascade", "paired-wings"), required=True
    )
    parser.add_argument(
        "--method", choices=("near", "polygonal", "batch", "reuse"), required=True
    )
    parser.add_argument("--reference-module", type=Path)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.repetitions <= 11 or (
        args.method == "polygonal" and args.reference_module is None
    ):
        parser.error("bounded repetitions and an explicit polygonal reference required")
    for p in (args.output, args.trace):
        if p is not None and not p.resolve().is_relative_to(Path("/tmp")):
            parser.error("output/trace require files under /tmp")
    if revision()["experiment_tree_dirty"]:
        parser.error("Freeze the experiment before recording costs")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(run(args), indent=2, allow_nan=False) + "\n")


if __name__ == "__main__":
    main()

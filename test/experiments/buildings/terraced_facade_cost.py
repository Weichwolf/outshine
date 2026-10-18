"""Fresh-process incremental work/storage diagnostic; native allocation counts remain open."""

import argparse
import gc
import json
from pathlib import Path
import resource
import time
import tracemalloc

import numpy as np

from evidence import revision
from terraced_facade_geometry import build_facade
from terraced_mass_plan import plan
from terraced_mass_rooms import build
from terraced_mass_sources import sources


def measure(index, kind, mode):
    mass = plan(sources()[index], kind)
    constructor = build if mode == "before" else build_facade
    start_cpu, start = time.process_time(), time.perf_counter()
    product = constructor(mass)
    seconds = time.perf_counter() - start
    cpu_seconds = time.process_time() - start_cpu
    floors = product if mode == "before" else product.floors
    components = (floors.shell, floors.stairs, floors.landing)
    arrays = [
        a
        for part in components
        for a in (
            *part.mesh.values(),
            part.vertex_roots,
            part.source_indices,
            part.source_triangles,
        )
    ]
    triangles = sum(len(part.mesh["faces"]) for part in components)
    windows, details = 0, 0
    if mode == "after":
        arrays.extend(product.frames.values())
        triangles += len(product.frames["faces"])
        windows = len(product.facade.windows)
        details = len(np.unique(product.frames["parts"]))
    unique = {(int(a.__array_interface__["data"][0]), a.nbytes): a for a in arrays}
    stored = sum(a.nbytes for a in unique.values())
    returned_buffers = len(unique)
    del arrays, unique, components, floors, product
    gc.collect()
    tracemalloc.start()
    replay = constructor(mass)
    _current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    del replay
    return revision() | {
        "source_index": index,
        "source_id": mass.identifier,
        "programme": kind,
        "mode": mode,
        "storeys": len(mass.storeys),
        "windows": windows,
        "closed_detail_parts": details,
        "triangles": triangles,
        "returned_unique_array_buffers": returned_buffers,
        "returned_array_bytes": stored,
        "untraced_wall_seconds": seconds,
        "untraced_process_cpu_seconds": cpu_seconds,
        "traced_replay_peak_bytes": peak,
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Linux fresh process. Input acquisition/import/mass planning excluded from build time; facade planning included. Process RSS includes imports/planning. Traced replay is a separate instrumented build and excludes untracked native/GPU allocations. Output buffers are retained-array counts, not cumulative allocation/copy counts. Before/after have deliberately different facade quality; incremental cost, not an equivalent-quality speed comparison or frame/device gate.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=int, choices=range(4), required=True)
    parser.add_argument(
        "--programme", choices=("cascade", "paired-wings"), required=True
    )
    parser.add_argument("--mode", choices=("before", "after"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    if revision()["experiment_tree_dirty"]:
        parser.error("Freeze the diagnostic before recording costs")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            measure(args.source, args.programme, args.mode), indent=2, allow_nan=False
        )
        + "\n"
    )


if __name__ == "__main__":
    main()

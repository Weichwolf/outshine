"""Compare selected complete original source ranges with resident original words."""

import argparse
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from source_domain_prepared_view import select_sources
from source_original_ranges import original_rows, read_ranges, write_ranges
from source_patch_inputs import read_bound_sources

ROOT = Path(__file__).resolve().parents[3]


def measured(call):
    tracemalloc.start()
    start_cpu, start_wall = time.process_time(), time.perf_counter()
    value = call()
    cpu, wall = time.process_time() - start_cpu, time.perf_counter() - start_wall
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return value, {"cpu_s": cpu, "wall_s": wall, "incremental_traced_peak_bytes": peak}


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    bound_raw = args.bounds.read_bytes()
    view = json.loads(args.view.read_text())
    if hashlib.sha256(bound_raw).hexdigest() != view["bounds_sha256"]:
        raise ValueError("Actual prepared bounds required")
    with np.load(args.bounds, allow_pickle=False) as z:
        prepared = {key: z[key] for key in ("source_ids", "bounds", "normal")}
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        placements = z["placements/words"].copy()
    rows, row_cost = measured(
        lambda: original_rows(source, context["triangle_binding"])
    )
    index, build_cost = measured(
        lambda: write_ranges(args.output, rows, indexed=args.indexed)
    )
    index_raw = (args.output / "index.npy").read_bytes()
    loaded_index = np.load(args.output / "index.npy", allow_pickle=False)
    if not np.array_equal(index, loaded_index):
        raise ValueError("Exact stored source range index required")
    cases = []
    for angle in (0, 65, 90):
        selected = select_sources(prepared, views[angle], placements)
        row = next(case for case in view["cases"] if case["angle_degrees"] == angle)
        if set(selected) != set(row["selected_source_ids"]):
            raise ValueError("Recorded selected source set changed")
        (loaded, io), query_cost = measured(
            lambda selected=selected: read_ranges(
                args.output, loaded_index, selected, indexed=args.indexed
            )
        )
        reference = rows[np.isin(rows["binding"][:, 1], selected)]
        if not np.array_equal(loaded.view("u1"), reference.view("u1")):
            raise ValueError(
                "Original triangles, order, bindings or corner words changed"
            )
        cases.append(
            {
                "angle": angle,
                "sources": len(selected),
                "triangles": len(loaded),
                **io,
                "index_bytes": len(index_raw),
                "output_array_bytes": loaded.nbytes,
                **query_cost,
            }
        )
    native_bytes = source["positions"].nbytes + 4 * source["indices"].size
    native_bytes += sum(
        a.nbytes for part in source["attributes"] for a in part.values()
    )
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "bounds_sha256": view["bounds_sha256"],
        "encoding": "source_indexed" if args.indexed else "direct_corners",
        "index_sha256": hashlib.sha256(index_raw).hexdigest(),
        "sources": len(index),
        "triangles": len(rows),
        "native_original_array_bytes": native_bytes,
        "source_range_bytes": (args.output / "rows.bin").stat().st_size,
        "index_bytes": len(index_raw),
        "row_build": row_cost,
        "range_build": build_cost,
        "cases": cases,
        "scope": "Experimental original per-triangle ranges retain actual global indices, native attribute corner words, six binding words and original triangle order. Native part metadata, bounds, recipes, asset format metadata and live revisions are separate. Inputs were resident and file reads warm; bytes are application reads, not physical IO. Traced incremental peaks exclude existing inputs and native/OS allocations. Selection is conditional view-only: no shadows, simulation, production cache, native raster, World admission or device timing.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--view", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--indexed", action="store_true")
    run(parser.parse_args())

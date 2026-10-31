"""Reproduce source-range storage and original material inputs on bound native data."""

import argparse
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_triangle_materials import DATA, bound_selection
from source_original_range_parts import part_inputs, write_part_inputs
from source_original_ranges import original_rows, read_ranges, write_ranges
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts, write_source

ROOT = Path(__file__).resolve().parents[3]


def measure(call):
    tracemalloc.start()
    cpu, wall = time.process_time(), time.perf_counter()
    value = call()
    costs = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
        "incremental_traced_peak_bytes": tracemalloc.get_traced_memory()[1],
    }
    tracemalloc.stop()
    return value, costs


def check_parts(actual, reference, bindings):
    offset = 0
    for (part, arrays, labels), (old_part, old_arrays) in zip(
        actual, reference, strict=True
    ):
        if part != old_part or any(
            arrays[k].tobytes() != v.tobytes() for k, v in old_arrays.items()
        ):
            raise ValueError("Restored material inputs differ from original adapter")
        if not np.array_equal(labels, bindings[offset : offset + len(labels)]):
            raise ValueError("Original triangle binding changed")
        offset += len(labels)
    if offset != len(bindings):
        raise ValueError("Incomplete original source bindings")


def same_files(reference, actual):
    paths = {p.relative_to(reference) for p in reference.rglob("*") if p.is_file()}
    if paths != {p.relative_to(actual) for p in actual.rglob("*") if p.is_file()}:
        raise ValueError("Native material input inventory changed")
    if any((reference / p).read_bytes() != (actual / p).read_bytes() for p in paths):
        raise ValueError("Native material input file changed")
    return len(paths)


def run(output):
    if output.exists():
        raise ValueError("Fresh system-temp output required")
    output.mkdir(parents=True)
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    selection, metadata = bound_selection(DATA, source)
    binding = context["triangle_binding"]
    demands = {
        "all": context["source_ordinals"],
        "native_triangle_sources": np.unique(binding[selection, 1]),
        "none": np.empty(0, "<i4"),
    }
    rows, row_cost = measure(lambda: original_rows(source, binding))
    cases, gaps = [], []
    for name, indexed in (("direct", False), ("indexed", True)):
        directory = output / name
        index, build_cost = measure(
            lambda indexed=indexed, directory=directory: write_ranges(
                directory, rows, indexed=indexed
            )
        )
        index_bytes = (directory / "index.npy").stat().st_size
        for demand, identities in demands.items():
            (loaded, io), read_cost = measure(
                lambda identities=identities, indexed=indexed, directory=directory, index=index: (
                    read_ranges(directory, index, identities, indexed=indexed)
                )
            )
            expected_rows = rows[np.isin(binding[:, 1], identities)]
            if not np.array_equal(loaded.view("u1"), expected_rows.view("u1")):
                raise ValueError("Original triangle words or order differ")
            parts, part_cost = measure(
                lambda loaded=loaded: part_inputs(loaded, source["parts"])
            )
            reference = triangle_parts(
                source, loaded["ordinal"], referenced_vertices=True
            )
            check_parts(parts, reference, binding[loaded["ordinal"]])
            if demand == "native_triangle_sources":
                write_part_inputs(parts, output / (name + "-renderer"))
                if not indexed:
                    write_source(
                        source,
                        loaded["ordinal"],
                        output / "reference",
                        referenced_vertices=True,
                    )
                for gap in (4096, 16384):
                    (coalesced, gap_io), gap_cost = measure(
                        lambda gap=gap, identities=identities, indexed=indexed, directory=directory, index=index: (
                            read_ranges(
                                directory,
                                index,
                                identities,
                                indexed=indexed,
                                max_gap_bytes=gap,
                            )
                        )
                    )
                    if not np.array_equal(coalesced.view("u1"), loaded.view("u1")):
                        raise ValueError("Coalescing must preserve requested originals")
                    gaps.append(
                        {
                            "layout": name,
                            "max_gap_bytes": gap,
                            **gap_io,
                            "read_cost": gap_cost,
                        }
                    )
            cases.append(
                {
                    "layout": name,
                    "demand": demand,
                    "sources": len(identities),
                    "triangles": len(loaded),
                    "read_calls": io["read_calls"],
                    "read_bytes": io["read_bytes"],
                    "index_bytes": index_bytes,
                    "output_row_bytes": loaded.nbytes,
                    "native_array_bytes": sum(
                        a.nbytes for _, values, _ in parts for a in values.values()
                    ),
                    "read_cost": read_cost,
                    "part_cost": part_cost,
                }
            )
        cases[-3]["build_cost"] = build_cost
    count = same_files(output / "reference", output / "direct-renderer")
    if same_files(output / "reference", output / "indexed-renderer") != count:
        raise ValueError("Native input inventory changed")
    native_bytes = source["positions"].nbytes + 4 * source["indices"].size
    native_bytes += sum(
        a.nbytes for part in source["attributes"] for a in part.values()
    )
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "native_reference_selected_triangles": metadata["selected_triangles"],
        "native_input_files_exact": count,
        "original_native_array_bytes": native_bytes,
        "original_raw_binding_bytes": binding.nbytes,
        "row_build": row_cost,
        "cases": cases,
        "coalesced_native_triangle_sources": gaps,
        "scope": "Original-source word preservation for a bound finite fixture. Native triangle IDs seed whole-source demand, not general view, shadow or World admission. Native renderer input files are identical, but no native execution, live revision, device timing or physical IO is asserted. Traced incremental peaks exclude held inputs and native/OS allocations.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)

"""Compare compact source material assembly with the original expanded path."""

import argparse
import hashlib
import json
import os
import statistics
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from source_domain_prepared_view import select_sources
from source_original_range_parts import compact_part_inputs, part_inputs
from source_original_ranges import read_ranges
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts

ROOT = Path(__file__).resolve().parents[3]


def check_parts(parts, expected, binding):
    offset = 0
    for (metadata, arrays, labels), (reference, original) in zip(
        parts, expected, strict=True
    ):
        if metadata != reference or any(
            arrays[k].tobytes() != v.tobytes() for k, v in original.items()
        ):
            raise ValueError("Original material part words changed")
        if not np.array_equal(labels, binding[offset : offset + len(labels)]):
            raise ValueError("Original source binding changed")
        offset += len(labels)
    if offset != len(binding):
        raise ValueError("Incomplete original source bindings")


def pipeline(directory, index, identities, metadata, *, compact):
    data, work = read_ranges(
        directory, index, identities, indexed=True, compact=compact
    )
    parts = (
        compact_part_inputs(data, metadata) if compact else part_inputs(data, metadata)
    )
    array_bytes = sum(t.nbytes + v.nbytes for t, v in data) if compact else data.nbytes
    return parts, work, array_bytes


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    native, _ = bound_selection(DATA, source)
    report = json.loads((args.ranges / "report.json").read_text())
    index_raw = (args.ranges / "index.npy").read_bytes()
    if (
        report["source_binding_sha256"] != provenance["source_binding_sha256"]
        or report["index_sha256"] != hashlib.sha256(index_raw).hexdigest()
    ):
        raise ValueError("Bound original source-range index required")
    index = np.load(args.ranges / "index.npy", allow_pickle=False)
    view = json.loads(args.view.read_text())
    if hashlib.sha256(args.bounds.read_bytes()).hexdigest() != view["bounds_sha256"]:
        raise ValueError("Bound actual view selection required")
    with np.load(args.bounds, allow_pickle=False) as z:
        prepared = {key: z[key] for key in ("source_ids", "bounds", "normal")}
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        selected65 = select_sources(prepared, views[65], z["placements/words"])
    recorded = next(row for row in view["cases"] if row["angle_degrees"] == 65)
    if set(selected65) != set(recorded["selected_source_ids"]):
        raise ValueError("Recorded source selection changed")
    binding = context["triangle_binding"]
    demands = {
        "empty": [],
        "view65": selected65,
        "native_sources": np.unique(binding[native, 1]).tolist(),
        "full": index["source"].tolist(),
    }
    results = []
    for name, identities in demands.items():
        ordinals = np.flatnonzero(np.isin(binding[:, 1], identities)).astype("<u4")
        expected = triangle_parts(source, ordinals, referenced_vertices=True)
        elapsed = {"expanded": [], "compact": []}
        measurements = {}
        for _ in range(7):
            for label, compact in (("expanded", False), ("compact", True)):
                start = time.process_time_ns()
                parts, work, held = pipeline(
                    args.ranges, index, identities, source["parts"], compact=compact
                )
                elapsed[label].append((time.process_time_ns() - start) / 1e6)
                check_parts(parts, expected, binding[ordinals])
                measurements[label] = {"work": work, "intermediate_array_bytes": held}
        if measurements["expanded"]["work"] != measurements["compact"]["work"]:
            raise ValueError("Compared paths must read identical ranges")
        for label, compact in (("expanded", False), ("compact", True)):
            tracemalloc.start()
            retained = pipeline(
                args.ranges, index, identities, source["parts"], compact=compact
            )
            measurements[label].update(
                cpu_ms_median=statistics.median(elapsed[label]),
                cpu_ms_max=max(elapsed[label]),
                incremental_traced_peak_bytes=tracemalloc.get_traced_memory()[1],
            )
            tracemalloc.stop()
            del retained
        results.append(
            {
                "demand": name,
                "sources": len(identities),
                "triangles": len(ordinals),
                **measurements,
            }
        )
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "index_sha256": report["index_sha256"],
        "loadavg": list(os.getloadavg()),
        "cases": results,
        "scope": "Recorded finite whole-source demand and original native material inputs. Warm application reads, resident metadata and original fixtures; traced peaks exclude held inputs and native/OS allocations. No physical IO, live World, shadows, native raster/PBR or device claim.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("ranges", "view", "bounds", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())

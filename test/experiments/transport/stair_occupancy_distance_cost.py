"""Actual whole SITE requests: equal distances, cold/warm work and allocations."""

import argparse
import gc
import hashlib
import json
import os
import statistics
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from process_memory import peak_rss_bytes
from rail_consumers_plants import adapter
from stair_consumer_inputs import source_rows
from stair_terrain_experiment import frozen


def measured(query, inputs):
    work, values = {}, []
    cpu, wall = time.process_time(), time.perf_counter()
    for geometry in inputs:
        distance, note = query.transport.nearest_distance(geometry)
        nearest = int(query.buildings.tree.nearest(geometry))
        combined = min(
            distance, float(geometry.distance(query.buildings.shapes[nearest]))
        )
        values.append((distance, combined))
        for key in (
            "products",
            "face_tests",
            "geometry_candidates",
            "volume_materializer_calls",
            "index_queries",
            "returned_metadata_boxes",
            "preparation_candidates",
            "preparation_metadata_boxes",
            "preparation_cpu_s",
            "preparation_wall_s",
        ):
            work[key] = work.get(key, 0) + note.get(key, 0)
    cost = {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
    values = np.asarray(values)
    cost.update(work=work, signature=hashlib.sha256(values.tobytes()).hexdigest())
    cost["face_tests_are_charged_upper_bound"] = (
        query.transport.distance_method == "indexed"
    )
    cost["tree_internal_visits"] = None
    return values, cost


def profile(query, inputs, path, signature):
    import memray

    gc.collect()
    with memray.Tracker(str(path), trace_python_allocators=True):
        _values, note = measured(query, inputs)
    if note["signature"] != signature:
        raise ValueError("Profiled complete distances changed")
    reader = memray.FileReader(path)
    events = requested = 0
    for record in reader.get_allocation_records():
        if record.size > 0:
            events += record.n_allocations
            requested += record.size
    return {
        "allocation_events": events,
        "requested_allocation_bytes": requested,
        "tracked_peak_heap_bytes": reader.metadata.peak_memory,
        "trace_sha256": digest(path),
        "work": note["work"],
    }


def run(args):
    commit = frozen()
    report = json.loads((args.plants / "comparison.json").read_text())
    if report.get("transport_model") != "raised-body-headroom-piers/1":
        raise ValueError("Whole raised consumer input required")
    for name, expected in report["artifacts"].items():
        path = args.plants / name
        if (
            not path.resolve().is_relative_to(args.plants.resolve())
            or digest(path) != expected
        ):
            raise ValueError("Actual plant cost input changed: " + name)
    buildings, receipt = Path(report["buildings"]), Path(report["building_receipt"])
    if (
        digest(buildings / "comparison.json") != report["building_report_sha256"]
        or digest(receipt) != report["building_receipt_sha256"]
    ):
        raise ValueError("Saved original whole Buildings binding changed")
    prepare, query_for, reference_for, _held = adapter()
    data, state, baseline, bodies = prepare(buildings, receipt)
    polygons = source_rows(data, "landcover")[0]
    pool = json.loads((args.plants / "proposals.json").read_text())["canonical_pool"]
    inputs = [r["polygon"] for r in polygons] + [
        shapely.Point(r["root_xy_m"]) for r in pool
    ]
    reference = reference_for(baseline, state, bodies)
    expected = np.asarray([reference.transport.nearest_distance(g)[0] for g in inputs])

    def fresh(name):
        query, _info = query_for(state, bodies)
        query.transport.distance_method = name
        return query

    args.output.mkdir(parents=True, exist_ok=False)
    methods = ("flat", "indexed")
    operations = {name: fresh(name) for name in methods}
    result = {
        "commit": commit,
        "plant_report_sha256": digest(args.plants / "comparison.json"),
        "queries": len(inputs),
        "habitat_polygons": len(polygons),
        "proposal_points": len(pool),
        "source_products": len(operations["flat"].transport.catalog.sources),
        "source_facets": sum(
            len(s.bounds) for s in operations["flat"].transport.catalog.sources
        ),
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "methods": {},
    }
    signature = None
    with patch.object(
        Mesh,
        "__post_init__",
        side_effect=AssertionError("Demand must not clone meshes"),
    ):
        for name, query in operations.items():
            values, cold = measured(query, inputs)
            np.testing.assert_array_equal(values[:, 0], expected)
            np.testing.assert_array_equal(
                values[len(polygons) :, 1], [r["distance_m"] for r in pool]
            )
            if signature is not None and signature != cold["signature"]:
                raise ValueError("Whole transport/Buildings distance changed")
            signature = cold["signature"]
            np.save(args.output / (name + "-distances.npy"), values)
            result["methods"][name] = {"cold": cold, "rounds": []}
        for pair in range(args.pairs):
            for name in methods if pair % 2 == 0 else methods[::-1]:
                gc.collect()
                _values, note = measured(operations[name], inputs)
                if note["signature"] != signature:
                    raise ValueError("Measured complete distance changed")
                result["methods"][name]["rounds"].append(note)
        for name, note in result["methods"].items():
            for kind in ("cpu_s", "wall_s"):
                note["median_" + kind] = statistics.median(
                    r[kind] for r in note["rounds"]
                )
            if args.profile:
                for state_name, query in (
                    ("cold", fresh(name)),
                    ("warm", operations[name]),
                ):
                    note[state_name + "_profile"] = profile(
                        query,
                        inputs,
                        args.output / (name + "-" + state_name + ".bin"),
                        signature,
                    )
        index = operations["indexed"].transport._box_distance.layout
        result["index"] = {
            "boxes": len(index.bounds),
            "numeric_bounds_bytes": index.bounds.nbytes,
            "facet_id_bytes": index.ids.nbytes,
            "source_offsets_bytes": index.starts.nbytes,
            "tree_geometry_pointer_bytes": index.tree.geometries.nbytes,
            "prepared_nonempty_logical_facets": len(index.shapes),
            "total_GEOS_live_tree_bytes": None,
        }
    result.update(
        process_peak_rss_bytes=peak_rss_bytes(),
        all_transport_values_exact_to_independent_full_reference=True,
        all_combined_point_values_exact_to_saved_pool=True,
        mesh_constructor_calls=0,
        scope="Same actual whole polygons/points, independent whole physical reference. Preparation/decode and full producer excluded; cold includes index/unknown footprints, warm reuses caller-owned immutable metadata. Allocation traces include measurement bookkeeping; requested bytes are not copied/live/RSS. Index leaf work is a charged bound, GEOS internal node visits and total GEOS copying/live tree bytes unknown. Python only; no native/device/world/p99/UMA claim.",
    )
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({name: result["methods"][name]["median_cpu_s"] for name in methods})
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plants", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 8), default=5)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp cost output required")
    run(args)

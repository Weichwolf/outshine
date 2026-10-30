"""Measure the redundant full replay versus the same verified in-call snapshot."""

import argparse
import gc
import hashlib
import json
import os
import statistics
import time
from pathlib import Path
from unittest.mock import patch

from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from process_memory import peak_rss_bytes
from rail_consumers_buildings import bound_products
from rail_occupancy_inputs import bound_state
from stair_local_replay import load_chain
from stair_terrain_experiment import frozen


def signature(state):
    view, assets, supports, contacts, manifest = state
    content = hashlib.sha256()

    def feed(value):
        content.update(len(value).to_bytes(8, "little"))
        content.update(value)

    def text(value):
        feed(json.dumps(value, sort_keys=True).encode())

    def array(value):
        text((value.dtype.str, value.shape))
        feed(value.tobytes())

    def mesh(value):
        array(value.vertices)
        array(value.triangles)

    text(manifest)
    text(view.max_patches)
    for row in view.patches:
        text((row.patch_id, row.frame.identifier, row.kind, row.max_faces))
        mesh(row.mesh)
        array(view.active_faces[row.patch_id])
        for key, value in sorted(row.face_data.items()):
            text(key)
            array(value)
    for asset in assets:
        text((asset.identity, asset.source_ids, asset.rows, asset.xy_rounding_m))
        for key, value in sorted(asset.data.items()):
            text(key)
            array(value)
    for value in supports:
        mesh(value)
    for lower, upper in contacts:
        mesh(lower)
        mesh(upper)
    return content.hexdigest()


def measured(operation):
    work = {"mesh_constructors": 0, "known_owned_mesh_copy_bytes": 0}
    original = Mesh.__post_init__

    def constructed(value):
        original(value)
        work["mesh_constructors"] += 1
        work["known_owned_mesh_copy_bytes"] += (
            value.vertices.nbytes + value.triangles.nbytes
        )

    cpu, wall = time.process_time(), time.perf_counter()
    with patch.object(Mesh, "__post_init__", constructed):
        state = operation()
    return state, {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
        "work": work,
    }


def run(args):
    commit = frozen()
    report = json.loads((args.buildings / "comparison.json").read_text())
    receipt = json.loads(args.receipt.read_text())
    if receipt["report_sha256"] != digest(
        args.buildings / "comparison.json"
    ) or not receipt.get("float32_bodies_against_actual_raised_volumes"):
        raise ValueError("Whole independently read raised Buildings input required")
    rail = bound_products(report)
    resident, prior = (
        Path(report["resident"]),
        tuple(map(Path, report["prior_modules"])),
    )
    operations = {
        "replay": lambda: load_chain(resident, prior),
        "bound": lambda: bound_state(rail, resident, prior),
    }
    expected = signature(rail.state)
    args.output.mkdir(parents=True, exist_ok=False)
    result = {
        "commit": commit,
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "source_ids": rail.state[4]["source_ids"],
        "state_signature": expected,
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "methods": {name: {"rounds": []} for name in operations},
    }
    for pair in range(args.pairs):
        for name in tuple(operations) if pair % 2 == 0 else tuple(operations)[::-1]:
            gc.collect()
            actual, note = measured(operations[name])
            if signature(actual) != expected:
                raise ValueError("Whole replay/snapshot semantic state differs")
            if name == "bound" and actual is not rail.state:
                raise ValueError("Bound loader must retain exactly the owned snapshot")
            result["methods"][name]["rounds"].append(note)
            del actual
    for name, operation in operations.items():
        note = result["methods"][name]
        for kind in ("cpu_s", "wall_s"):
            note["median_" + kind] = statistics.median(
                row[kind] for row in note["rounds"]
            )
        if args.profile:
            import memray

            path = args.output / (name + ".bin")
            gc.collect()
            with memray.Tracker(str(path), trace_python_allocators=True):
                actual, profiled = measured(operation)
            if signature(actual) != expected:
                raise ValueError("Profiled whole state changed")
            del actual
            reader = memray.FileReader(path)
            events = requested = 0
            for record in reader.get_allocation_records():
                if record.size > 0:
                    events += record.n_allocations
                    requested += record.size
            note["profile"] = {
                "allocation_events": events,
                "requested_allocation_bytes": requested,
                "tracked_peak_heap_bytes": reader.metadata.peak_memory,
                "trace_sha256": digest(path),
                "work": profiled["work"],
            }
    result.update(
        process_peak_rss_bytes=peak_rss_bytes(),
        all_complete_semantic_state_signatures_equal=True,
        scope="Actual resident/ordered five-module inputs, full terrain patch meshes/active masks/face data, road arrays/attributes, all supports/contacts and manifests equal. Both operations start with the same already validated caller-owned snapshot; initial necessary loading is excluded. Constructor buffer copies are an explicit subset, total decoder/NumPy/GEOS copies unknown. Allocation profiles exclude semantic signature work; requested bytes are not live/RSS. No native/World/device/p99/UMA or source-free asset-hit admission.",
    )
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {name: note["median_cpu_s"] for name, note in result["methods"].items()}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 8), default=5)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit private system-temp output required")
    run(args)

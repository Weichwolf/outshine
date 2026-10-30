"""Equal whole consumer data: complete fallback versus verified prefix demand."""

import argparse
import gc
import hashlib
import json
import os
import statistics
import time
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

import numpy as np
import stair_consumer_inputs as consumer
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from process_memory import peak_rss_bytes
from rail_consumers_buildings import bound_products
from stair_occupancy_geometry import AffineOccupancy
from stair_terrain_experiment import frozen


def signature(data, state):
    for key, index in (
        ("terrain_patches", 0),
        ("assets", 1),
        ("supports", 2),
        ("contacts", 3),
    ):
        if data[key] is not state[index]:
            raise ValueError("Same whole current input products required")
    if (
        "reservations" in data
        or "report" in data
        or data["sampler"].mesh is not data["terrain"]
    ):
        raise ValueError("Exact candidate demand contract required")
    content = hashlib.sha256()

    def feed(value):
        content.update(len(value).to_bytes(8, "little"))
        content.update(value)

    def array(value):
        feed(json.dumps((value.dtype.str, value.shape)).encode())
        feed(value.tobytes())

    feed(
        json.dumps(
            (data["context"], data["layers"], data["frame"].identifier), sort_keys=True
        ).encode()
    )
    mesh = data["terrain"]
    array(mesh.vertices)
    array(mesh.triangles)
    extent = data["context"]["vector"]["extent"]
    array(
        data["project"](
            np.array([[0, 0], [extent, extent], [extent / 2, extent / 2]]), extent
        )
    )
    faces = mesh.triangles[np.linspace(0, len(mesh.triangles) - 1, 8, dtype=int)]
    samples = data["sampler"].sample(mesh.vertices[faces, :2].mean(1))
    for value in (samples.points_enu_m, samples.normals_enu, samples.face_indices):
        array(value)
    feed(json.dumps(samples.reasons).encode())
    return content.hexdigest()


def measured(baseline, state, method):
    work = {
        "mesh_constructors": 0,
        "known_mesh_buffer_copy_bytes": 0,
        "affine_constructors": 0,
    }
    original_mesh, original_volume = Mesh.__post_init__, AffineOccupancy.__post_init__

    def mesh(value):
        original_mesh(value)
        work["mesh_constructors"] += 1
        work["known_mesh_buffer_copy_bytes"] += (
            value.vertices.nbytes + value.triangles.nbytes
        )

    def volume(value):
        original_volume(value)
        work["affine_constructors"] += 1

    cpu, wall = time.process_time(), time.perf_counter()
    with ExitStack() as stack:
        stack.enter_context(patch.object(Mesh, "__post_init__", mesh))
        stack.enter_context(patch.object(AffineOccupancy, "__post_init__", volume))
        if method == "full":
            stack.enter_context(
                patch.object(consumer, "_baseline_is_prefix", return_value=False)
            )
        data = consumer.candidate_inputs(baseline, state)
    return data, {
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
        raise ValueError("Whole independently read raised Buildings required")
    rail, baseline = bound_products(report), Path(report["baseline"])
    args.output.mkdir(parents=True, exist_ok=False)
    methods = ("full", "demand")
    result = {
        "commit": commit,
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "baseline_report_sha256": digest(baseline / "comparison.json"),
        "current_sources": len(rail.state[4]["source_ids"]),
        "loadavg_before": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "methods": {name: {"rounds": []} for name in methods},
    }
    expected = None
    for pair in range(args.pairs):
        for name in methods if pair % 2 == 0 else methods[::-1]:
            gc.collect()
            data, note = measured(baseline, rail.state, name)
            actual = signature(data, rail.state)
            if expected is not None and expected != actual:
                raise ValueError(
                    "Whole consumer inputs/projection/height samples differ"
                )
            expected = actual
            result["methods"][name]["rounds"].append(note)
            del data
    for name, note in result["methods"].items():
        for kind in ("cpu_s", "wall_s"):
            note["median_" + kind] = statistics.median(
                row[kind] for row in note["rounds"]
            )
        if args.profile:
            import memray

            path = args.output / (name + ".bin")
            gc.collect()
            with memray.Tracker(str(path), trace_python_allocators=True):
                data, profiled = measured(baseline, rail.state, name)
            if signature(data, rail.state) != expected:
                raise ValueError("Profiled whole consumer data changed")
            del data
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
        data_signature=expected,
        all_whole_consumer_inputs_projection_and_samples_equal=True,
        scope="Same already verified current state. Actual complete fallback selected as control, including all baseline geometry/volume checks; demand uses proved prefix plus unchanged independently checked export bytes. Source context/decode, current whole terrain and sampler included. Signatures/sampling excluded from measurement. Constructor copies are a subset; total decoder/NumPy/GEOS copies unknown. Allocation profiles include counter bookkeeping; requested bytes are not live/copied/RSS. No native/device/World/p99/UMA/source-free asset-hit admission.",
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

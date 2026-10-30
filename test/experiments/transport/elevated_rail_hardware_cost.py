"""Equal complete hardware as owned compact instances or vectorized baked meshes."""

import argparse
import gc
import json
import os
import statistics
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from elevated_rail_hardware import bound_input, construct
from elevated_rail_hardware_verify import arrays
from experiment_output import create_output
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import bake_rail_sleepers
from outshine_ref.transport.rail_hardware._geometry import SLEEPER
from process_memory import peak_rss_bytes
from rail_occupancy_state_cost import measured
from rail_place_verify import independent_sleepers
from stair_terrain_experiment import frozen


def representation(inputs, baked):
    result = []
    for data in inputs:
        rail = Mesh(data["rail_vertices"], data["rail_triangles"])
        if baked:
            sleepers = bake_rail_sleepers(data["sleeper_transforms"])
        else:
            sleepers = data["sleeper_transforms"].copy()
            sleepers.setflags(write=False)
        result.append((rail, sleepers))
    return result


def payload(products, baked):
    total = 0 if baked else SLEEPER.vertices.nbytes + SLEEPER.triangles.nbytes
    for rail, sleepers in products:
        total += rail.vertices.nbytes + rail.triangles.nbytes
        total += (
            sleepers.vertices.nbytes + sleepers.triangles.nbytes
            if baked
            else sleepers.nbytes
        )
    return total


def run(args):
    commit = frozen()
    create_output(args.output)
    report = json.loads((args.products / "report.json").read_text())
    receipt = json.loads(args.receipt.read_text())
    if receipt["report_sha256"] != digest(
        args.products / "report.json"
    ) or not receipt.get("whole_hardware_verified"):
        raise ValueError("Same actual independently verified whole hardware required")
    for name, pin in report["artifacts"].items():
        if digest(args.products / name) != pin:
            raise ValueError("Actual cost-control hardware changed")
    inputs = [
        arrays(
            args.products / r["file"],
            ("rail_vertices", "rail_triangles", "sleeper_transforms"),
        )
        for r in report["components"]
    ]
    source = bound_input(Path(report["buildings"]), Path(report["building_receipt"]))
    construction = []
    for _ in range(3):
        gc.collect()
        value, note = measured(lambda: construct(source))
        current, omitted, _triangle = value
        if (
            omitted
            or len(current) != len(inputs)
            or any(
                not np.array_equal(row["arrays"][key], expected[key])
                for row, expected in zip(current, inputs, strict=True)
                for key in expected
            )
        ):
            raise ValueError("Complete constructor differs from actual saved geometry")
        construction.append(note)
        del value, current
    operations = {
        name: lambda baked=baked: representation(inputs, baked)
        for name, baked in (("compact", False), ("baked", True))
    }

    def check(name, products):
        for (rail, sleepers), data in zip(products, inputs, strict=True):
            if not np.array_equal(
                rail.vertices, data["rail_vertices"]
            ) or not np.array_equal(rail.triangles, data["rail_triangles"]):
                raise ValueError("Original whole rail geometry changed")
            expected = independent_sleepers(data["sleeper_transforms"])
            vertices = (
                sleepers.vertices if name == "baked" else independent_sleepers(sleepers)
            )
            if not np.array_equal(vertices, expected):
                raise ValueError("Every complete sleeper instance must match the bake")

    methods = {name: {"rounds": []} for name in operations}
    for pair in range(args.pairs):
        order = tuple(operations) if pair % 2 == 0 else tuple(operations)[::-1]
        for name in order:
            gc.collect()
            value, note = measured(operations[name])
            check(name, value)
            methods[name]["geometry_payload_bytes"] = payload(value, name == "baked")
            methods[name]["rounds"].append(note)
            del value
    for name, operation in operations.items():
        note = methods[name]
        for kind in ("cpu_s", "wall_s"):
            note["median_" + kind] = statistics.median(
                row[kind] for row in note["rounds"]
            )
        if args.profile:
            import memray

            path = args.output / (name + ".bin")
            gc.collect()
            with memray.Tracker(str(path), trace_python_allocators=True):
                value, profiled = measured(operation)
            check(name, value)
            del value
            reader = memray.FileReader(path)
            events = requested = 0
            for record in reader.get_allocation_records():
                if record.size > 0:
                    events += record.n_allocations
                    requested += record.size
            note["profile"] = {
                "allocation_events": events,
                "requested_bytes": requested,
                "scoped_peak_heap_bytes": reader.metadata.peak_memory,
                "known_mesh_work": profiled["work"],
                "explicit_transform_copy_bytes": sum(
                    d["sleeper_transforms"].nbytes for d in inputs
                )
                if name == "compact"
                else 0,
                "trace_sha256": digest(path),
            }
    result = {
        "commit": commit,
        "report_sha256": digest(args.products / "report.json"),
        "receipt_sha256": digest(args.receipt),
        "methods": methods,
        "whole_constructor_rounds": construction,
        "held_source_inputs_excluded_from_construction": True,
        "sleepers": sum(len(d["sleeper_transforms"]) for d in inputs),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "Three complete constructor rounds from the same already held qualified sources, then seven alternating equivalent owned representations. Vectorized public bake, no scalar strawman. Input loading/validation, source preparation, serialization, signatures and rendering excluded from representation measurements. Material roles and one common object pose are shared metadata in both. Compact payload counts the shared unit template once. Known Mesh/transform copies are a subset; total NumPy copying unknown. Scoped peaks exclude held inputs. No native/GPU/frame/cache-hit/UMA claim.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({name: note["median_cpu_s"] for name, note in methods.items()}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=int, choices=range(1, 10), default=7)
    parser.add_argument("--profile", action="store_true")
    run(parser.parse_args())

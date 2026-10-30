"""Equal physical recipe: sample fit versus bounded HiGHS, and owned product reuse."""

import argparse
import hashlib
import json
import os
from pathlib import Path

import numpy as np
from experiment_output import create_output
from field_view_cost import measured
from ground_area_bounds import verify_geometry
from ground_area_inputs import FIXTURE, load
from ground_area_recipe import construct, prepare
from outshine_ref.contracts import Mesh
from rail_occupancy_state_cost import measured as mesh_work
from stair_terrain_experiment import frozen


def owned(product):
    body, upper, lower, plane = product
    return (
        Mesh(body.vertices, body.triangles),
        Mesh(upper.vertices, upper.triangles),
        Mesh(lower.vertices, lower.triangles),
        {k: v.copy() if isinstance(v, np.ndarray) else v for k, v in plane.items()},
    )


def run(args):
    commit = frozen()
    create_output(args.output)
    inputs = load(args.case)
    prepared = prepare(inputs.original, inputs.surface, inputs.recipe)
    held = construct(prepared, inputs.recipe, "affine")
    functions = {
        "prepare-held-local-terrain": lambda: prepare(
            inputs.original, inputs.surface, inputs.recipe
        ),
        "construct-affine": lambda: construct(prepared, inputs.recipe, "affine"),
        "construct-highs": lambda: construct(prepared, inputs.recipe, "highs"),
        "restore-owned-product": lambda: owned(held),
    }
    # Imports and correctness precede timing; no solver import is hidden in a hot round.
    for name, function in functions.items():
        value = function()
        if name != "prepare-held-local-terrain":
            verify_geometry(*value[:3], inputs.surface, inputs.original, inputs.recipe)
    methods = {}
    for name, function in functions.items():
        value, cost = measured(function, args.output, name)
        _repeat, work = mesh_work(function)
        if name == "prepare-held-local-terrain":
            if not np.array_equal(
                value["control_xy"], prepared["control_xy"]
            ) or not np.array_equal(value["control_z"], prepared["control_z"]):
                raise ValueError(
                    "Measured whole-area preparation changed its actual controls"
                )
        else:
            proof = verify_geometry(
                *value[:3], inputs.surface, inputs.original, inputs.recipe
            )
            cost.update(
                physical_bounds=proof["stored_caps"],
                body_vertices=len(value[0].vertices),
                body_triangles=len(value[0].triangles),
                geometry_payload_bytes=value[0].vertices.nbytes
                + value[0].triangles.nbytes,
            )
        methods[name] = {"cost": cost, "separate_known_mesh_work": work["work"]}
    report = {
        "commit": commit,
        "input_sha256": hashlib.sha256(args.case.read_bytes()).hexdigest(),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "local_terrain_faces": len(inputs.surface.triangles),
        "refinement_control_points": len(prepared["control_xy"]),
        "query_work_at_original_capture": inputs.case["query_work"],
        "methods": methods,
        "source_sha256": {
            p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
            for p in (
                "test/experiments/transport/ground_area_recipe.py",
                "test/experiments/transport/ground_area_bounds.py",
                "test/experiments/transport/ground_area_cost.py",
                "lib/src/outshine_ref/transport/canonical_ports/_affine.py",
            )
        },
        "scope": "Seven normal-priority rounds per method and separate native Memray traces. Same complete physical rise/embed/grade/footprint requirements; outputs need not be bit-identical. Held local inputs and expected products precede timing; source/MVT/old-world preparation, verification, serialization, imports and rendering excluded. Owned restore copies all three ready meshes and plane arrays, not a free alias. Known Mesh copies exclude other NumPy copies; total traffic/process/UMA peaks, actual IO/cache hits and native/frame performance unmeasured.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

"""Same complete qualified side bodies from construction or a held verified product."""

import argparse
import json
import os
import platform
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from experiment_output import create_output
from field_view_cost import measured
from outshine_ref.contracts import Mesh
from raised_barrier_study import bound_inputs, build, physical_guards
from raised_barrier_verify import verify
from raised_edge_barriers import DEFAULT_RECIPE
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    create_output(args.output)
    start = time.perf_counter()
    held, acceptance = verify(args.products)
    report = json.loads((args.products / "report.json").read_text())
    _parent, bed, _datum, clearance, cases, terrain = bound_inputs(
        Path(report["hardware"]), Path(report["hardware_receipt"])
    )
    preparation = time.perf_counter() - start

    def generate():
        rows = build(cases, bed, clearance, DEFAULT_RECIPE)
        physical_guards(rows, cases, terrain)
        return tuple(r["body"] for r in rows)

    def copy_held():
        return tuple(Mesh(r["body"].vertices, r["body"].triangles) for r in held)

    results = {}
    for name, call in (
        ("qualified-generation", generate),
        ("owned-held-product", copy_held),
    ):
        value, costs = measured(call, args.output, name)
        if any(
            not np.array_equal(actual.vertices, row["body"].vertices)
            or not np.array_equal(actual.triangles, row["body"].triangles)
            for actual, row in zip(value, held, strict=True)
        ):
            raise ValueError(
                "Equal complete qualified product required for cost comparison"
            )
        results[name] = costs
    body_bytes = sum(
        r["body"].vertices.nbytes + r["body"].triangles.nbytes for r in held
    )
    cap_bytes = sum(
        sum(m.vertices.nbytes + m.triangles.nbytes for m in (r["upper"], r["lower"]))
        for r in held
    )
    summary = {
        "commit": commit,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_average": list(os.getloadavg()),
        "recipe": asdict(DEFAULT_RECIPE),
        "input_binding_and_diagnostic_read_preparation_s": preparation,
        "costs": results,
        "whole_bodies": len(held),
        "output_body_bytes": body_bytes,
        "known_held_mesh_constructor_copy_bytes": body_bytes,
        "known_generation_mesh_constructor_copy_bytes_lower_bound": 3 * body_bytes
        + cap_bytes,
        "held_numeric_query_inputs_bytes": sum(
            v.nbytes for c in cases for v in c["hardware"].values()
        )
        + terrain.vertices.nbytes
        + terrain.triangles.nbytes
        + sum(c["axis"].nbytes for c in cases),
        "contact_work": acceptance["whole_physical_guards"],
        "scope": "Same complete qualified body buffers. Generation includes construction and independent rational full-cap/hardware checks. Held copy assumes immutable prior qualification; a diagnostic reader is NOT the native cache-hit path. Whole old source reconstruction/input preparation excluded and reported. Known mesh-constructor copies are a lower bound, not all temporaries; Memray counts scoped allocations/requested/peak. No physical IO/UMA/RSS/native/frame/device/p99 claim.",
    }
    (args.output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(
        json.dumps(
            {
                "body_bytes": body_bytes,
                "costs": {k: v["process_cpu_median_s"] for k, v in results.items()},
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

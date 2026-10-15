"""Compare real LP backends inside the unchanged whole-port planner checks."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine_ref.transport.port_heights import _fit
from port_height_library_experiment import run as run_library
from port_highs_cuts import fit_delayed
from scipy import sparse


def matrix_note(matrix):
    arrays = (matrix.data, matrix.indices, matrix.indptr)
    return {
        "shape": list(matrix.shape),
        "nonzeros": matrix.nnz,
        "payload_bytes": sum(a.nbytes for a in arrays),
        "buffer_sha256": [hashlib.sha256(a).hexdigest() for a in arrays],
    }


def snapshot(values, output):
    began = time.perf_counter()
    output.mkdir()
    for name, matrix in zip(
        ("height", "gradient", "equality"), values[:3], strict=True
    ):
        sparse.save_npz(output / (name + ".npz"), matrix)
    np.save(output / "targets.npy", values[3])
    return (time.perf_counter() - began) * 1000


def execute(args):
    original = _fit.fit_planes
    calls = []

    def fit(*values, **options):
        record = {
            "call": len(calls),
            "inputs": [matrix_note(m) for m in values[:3]],
            "target_bytes": values[3].nbytes,
            "solver_seconds_requested": options["seconds"],
            "max_iterations": options["max_iterations"],
            "snapshot_ms": 0.0,
        }
        if args.snapshot and not calls:
            record["snapshot_ms"] = snapshot(values, args.snapshot)
        began = time.perf_counter()
        if args.backend.startswith("highs"):
            _, solver, selection = args.backend.split("-")
            result = fit_delayed(
                *values,
                **options,
                solver=solver,
                selection=selection,
                initial_box=args.initial_box,
            )
        else:
            result = original(*values, **options)
        record.update(wall_ms=(time.perf_counter() - began) * 1000, report=result[1])
        calls.append(record)
        return result

    settings = SimpleNamespace(
        output=args.output,
        seconds=8.0,
        rounds=6,
        segments=8,
        method="lp" if args.backend == "polygon" else "cone",
    )
    with patch.object(_fit, "fit_planes", fit):
        valid = run_library(settings)
    path = args.output / "report.json"
    report = json.loads(path.read_text())
    report.update(
        backend=args.backend,
        initial_box=args.initial_box if args.backend.startswith("highs") else None,
        original_formulation=report["method"],
        backend_calls=calls,
        historical_frame="Frozen metric corpus; historical ENU label denotes the original scaled-Mercator source coordinates. No reprojecting or relabelling in this solver comparison.",
        constant_initialization="Original public planner only; never accepted without a successful complete data fit.",
        backend_scope="Experimental scoped replacement of _fit.fit_planes; unchanged public planner, finite quality, topology and contact checks.",
    )
    path.write_text(json.dumps(report, indent=2) + "\n")
    (args.output / "backend-work.json").write_text(json.dumps(calls, indent=2) + "\n")
    return valid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--backend",
        choices=(
            "clarabel",
            "polygon",
            "highs-ipm-support",
            "highs-ipm-all",
            "highs-simplex-support",
        ),
        required=True,
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path)
    parser.add_argument("--initial-box", choices=("support", "all"), default="support")
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("fresh empty external output required")
    if args.snapshot and args.snapshot.exists():
        parser.error("fresh external snapshot required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    raise SystemExit(0 if execute(args) else 2)


if __name__ == "__main__":
    main()

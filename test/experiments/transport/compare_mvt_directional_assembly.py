"""Compare scalar and batched assembly on an actual native-interface body plan."""

import argparse
import hashlib
import json
import math
import resource
import sys
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import mvt_directional_scalar_control as scalar
import mvt_t_directional_facets as directional
import numpy as np
from mvt_t_connection_inputs import FIXTURE, inputs
from stair_junction_solve import Conditions
from test_mvt_native_body import construct


class CountedConditions(Conditions):
    def __init__(self, variables):
        super().__init__(variables)
        self.calls = {"scalar_row_calls": 0, "row_block_calls": 0}

    def add(self, *args):
        self.calls["scalar_row_calls"] += 1
        return super().add(*args)

    def add_rows(self, *args):
        self.calls["row_block_calls"] += 1
        return super().add_rows(*args)


def matrix_hash(system):
    matrix = system.matrix()
    digest = hashlib.sha256()
    for array in (
        matrix.indptr,
        matrix.indices,
        matrix.data,
        np.asarray(system.bounds),
    ):
        digest.update(array.tobytes())
    return digest.hexdigest()


def measure(method, repeats, trace):
    if (
        method not in ("scalar", "batched")
        or type(repeats) is not int
        or not 1 <= repeats <= 256
    ):
        raise ValueError("known method and bounded repetitions required")
    case, _sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    fn = (
        scalar.directional_facets
        if method == "scalar"
        else directional.directional_facets
    )
    saved = []

    def capture(plan, system, *args, **kwargs):
        saved.append((plan, args, kwargs, system.variables))
        return fn(plan, system, *args, **kwargs)

    began = time.perf_counter()
    with patch.object(directional, "directional_facets", capture):
        constructed = construct(case, native)
    body_s = time.perf_counter() - began
    plan, args, kwargs, variables = saved.pop()
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    for _ in range(repeats):
        system = Conditions(variables)
        note = fn(plan, system, *args, **kwargs)
        matrix = system.matrix()
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _live, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
    inverse = np.linalg.inv
    work = {"inverse_dispatches": 0, "inverse_2x2_matrices": 0}

    def counted(value):
        work["inverse_dispatches"] += 1
        work["inverse_2x2_matrices"] += math.prod(value.shape[:-2])
        return inverse(value)

    counted_system = CountedConditions(variables)
    with patch.object(np.linalg, "inv", counted):
        fn(plan, counted_system, *args, **kwargs)
    if matrix_hash(system) != matrix_hash(counted_system):
        raise ValueError("work instrumentation changed program")
    report = {
        "method": method,
        "repeats": repeats,
        "isolated_assembly_s": elapsed,
        "single_body_and_consumers_s": body_s,
        "directional_work": note,
        "work": dict(work, **counted_system.calls),
        "program_sha256": matrix_hash(system),
        "rows": matrix.shape[0],
        "nonzeros": matrix.nnz,
        "csc_array_bytes": sum(
            a.nbytes for a in (matrix.data, matrix.indices, matrix.indptr)
        ),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "profile_audit": constructed[6],
        "fixed_contacts": constructed[7],
        "scope": "Prepared native-control plan assembly including Conditions and CSC conversion; full body separately timed. Imports, output and plotting excluded. No native/device/world admission or complete allocator/copy accounting.",
        **memory,
    }
    return constructed[4], constructed[8], report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("scalar", "batched"), required=True)
    parser.add_argument("--repeats", type=int, default=32)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data, products, report = measure(args.method, args.repeats, args.trace)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    (args.output / "products.json").write_text(json.dumps(products, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "method",
                    "repeats",
                    "isolated_assembly_s",
                    "work",
                    "program_sha256",
                )
            }
        )
    )


if __name__ == "__main__":
    main()

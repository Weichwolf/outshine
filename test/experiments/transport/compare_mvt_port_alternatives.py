"""Compare finite native-interface searches with unchanged physical qualification."""

import argparse
import hashlib
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import mvt_t_profile_solve as solver
import numpy as np
from compare_mvt_native_body import orientation_counts
from compare_native_surface_queries import display, sample_requests
from mvt_native_surface_queries import query_profiles
from mvt_port_alternatives import search_ports
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connections import plan_connections


def run(case, native, crop, strategy, trace=False, max_fits=64):
    proposal = plan_connections(source_rows(case["ways"]))
    seeds = {c["source_id"] for c in proposal["connections"]}
    if len(seeds) != 1:
        raise ValueError("one changed seed required for this finite comparison")
    seed = seeds.pop()
    work = {"lp_calls": 0, "rows": 0, "nonzeros": 0, "solver_input_array_bytes": 0}
    original = solver.linprog

    def counted(*args, **kwargs):
        matrix = kwargs["A_ub"]
        work["lp_calls"] += 1
        work["rows"] += matrix.shape[0]
        work["nonzeros"] += matrix.nnz
        work["solver_input_array_bytes"] += sum(
            a.nbytes
            for a in (
                matrix.data,
                matrix.indices,
                matrix.indptr,
                np.asarray(kwargs["b_ub"]),
                np.asarray(args[0]),
            )
        )
        return original(*args, **kwargs)

    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    candidates, _profiles, notes = query_profiles(
        native, crop, sample_requests(case, proposal, seed, (2, 4, 8, 16))
    )
    queried = time.perf_counter()
    with patch.object(solver, "linprog", counted):
        products, report = search_ports(
            case, candidates, proposal, seed, strategy=strategy, max_fits=max_fits
        )
    finished = time.perf_counter()
    report.update(
        query_notes=notes,
        work=work,
        queries_s=queried - began,
        total_s=finished - began,
    )
    if trace:
        _current, report["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
    report["process_peak_rss_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    if products is not None:
        data, _shared = products
        report["body"] = dict(
            orientation_counts(data), vertices=len(data["body_vertices"])
        )
        report["retained_array_bytes"] = sum(
            a.nbytes for a in data.values() if isinstance(a, np.ndarray)
        )
    return products, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--strategy", choices=("span_order", "outer_then_shrink"), required=True
    )
    parser.add_argument("--max-fits", type=int, default=64)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    case = json.loads(
        FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
    )
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    raw = FIXTURE.with_name("mvt_stair_native_surface_crop.json").read_bytes()
    if (
        hashlib.sha256(raw).hexdigest()
        != native["provenance"]["complete_cropped_export_sha256"]
    ):
        raise ValueError("original native crop hash mismatch")
    products, report = run(
        case, native, json.loads(raw), args.strategy, args.trace, args.max_fits
    )
    report["inputs"] = {
        name: hashlib.sha256(FIXTURE.with_name(name).read_bytes()).hexdigest()
        for name in (
            "mvt_stair_boundary_neighbours.json",
            "mvt_stair_native_crosscuts.json",
            "mvt_stair_native_surface_crop.json",
        )
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if products is not None:
        data, shared = products
        np.savez(args.output / "body.npz", **data)
        (args.output / "shared-products.json").write_text(
            json.dumps(shared, indent=2) + "\n"
        )
        if args.image:
            display(
                json.loads(raw),
                data,
                args.output / "body.png",
                replacement_title="Qualified compact interfaces / replacement",
            )
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("strategy", "valid", "reason", "fit_calls", "total_s")
            }
        )
    )


if __name__ == "__main__":
    main()

from outshine_ref.transport import PortHeightBudget
from outshine_ref.transport.port_heights._solve import (
    solve_slices,
)

"""Compare bounded parameter-aligned tessellation with an unchanged captured C1 plan."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.transport.port_heights._contacts import contact_audit
from outshine_ref.transport.port_heights._layout import prepare_plane_field
from port_plane_adaptive import initial_mesh
from port_plane_fixture import load_reference


def seed_plan(path, model):
    report = json.loads(path.with_name("report.json").read_text())
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != report["mesh_sha256"] or report["working_tree_dirty"]:
        raise ValueError("seed must match its clean-commit source report")
    if not report["audit"]["whole_world_valid"]:
        raise ValueError("seed needs a physically valid whole road/soil mesh")
    with np.load(path, allow_pickle=False) as source:
        seed = {name: source[name] for name in source.files}
    for name in (
        "plane_origins",
        "plane_ids",
        "cut_centres",
        "inward_normals",
        "scales",
    ):
        actual = getattr(model, "origins" if name == "plane_origins" else name)
        np.testing.assert_array_equal(actual, seed[name])
    return seed, {
        "path": str(path),
        "sha256": digest,
        "commit": report["generation_commit"],
    }


def run(args):
    began = time.monotonic()
    product, captured, metadata, sources = load_reference()
    model, _layout = prepare_plane_field(product)
    original = initial_mesh(product, captured)
    if args.seed:
        seed, provenance = seed_plan(args.seed, model)
    else:
        datum = float(np.median(captured["ground_targets"]))
        seed = {
            "plane_coefficients": np.zeros(3 * len(model.origins)),
            "vertices": np.c_[original["xy"], np.full(len(original["xy"]), datum)],
            "datum_m": datum,
        }
        provenance = {
            "kind": "constant feasible initialization only; never accepted without a successful data fit"
        }
    mesh, coefficients, seed, marked, errors, records = solve_slices(
        model,
        product,
        captured,
        original,
        seed,
        budget=PortHeightBudget(
            solver_time_limit_s=args.seconds,
            max_rounds=args.rounds,
            initial_segments=args.segments,
        ),
        method=args.method,
        fit=args.fit,
        began=began,
        progress=lambda record: print(json.dumps(record), flush=True),
    )
    args.output.mkdir(parents=True, exist_ok=True)
    audit = records[-1]["quality"]
    fit_ok = not args.fit or any(
        r["fitting"] and r["fitting"]["solver"]["valid"] for r in records
    )
    valid = bool(fit_ok and audit["geometry_complete"] and audit["whole_world_valid"])
    path = args.output / ("plan.npz" if valid else "diagnostic.npz")
    np.savez_compressed(
        path,
        **{k: v for k, v in mesh.items() if k != "port_roots"},
        marked=marked,
        height_errors=errors,
        coefficients=coefficients,
        datum_m=seed["datum_m"],
        port_edges=list(mesh["port_roots"]),
        port_ids=list(mesh["port_roots"].values()),
        plane_origins=model.origins,
        plane_ids=model.plane_ids,
        cut_centres=model.cut_centres,
        inward_normals=model.inward_normals,
        scales=model.scales,
    )
    report = {
        "approved": False,
        "valid_geometry": valid,
        "reason": "" if valid else "bounded_fit_or_geometry_incomplete",
        "scope": "Whole C1 analytic road fields and conforming C0 road/soil mesh. Original sources/ports retained, road height chord<=0.01m, nominal whole grades<=1+1e-7, finite port jump<=0.005. Offline Python; no second derivative/dynamics/network completeness/LOD/native proof. 3D review remains a separate acceptance condition.",
        "seed": provenance,
        "rounds": records,
        "generation_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    "test/experiments/transport",
                ],
                text=True,
            ).strip()
        ),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("port_plane_*.py")
        },
        "frame": metadata["frame"],
        "original_source_usage": sources["original_source_usage"],
        "source_fit_valid": fit_ok,
        "source_usage": sources["original_source_usage"]
        if valid
        else [
            {
                **row,
                "status": "not_used",
                "reason": row["reason"]
                if row["status"] == "not_used"
                else "bounded_height_plan_unresolved",
            }
            for row in sources["original_source_usage"]
        ],
        "analytic_contacts": contact_audit(
            model,
            seed["plane_coefficients"],
            product,
            mesh["heights"][mesh["original_vertex_map"][captured["road_vertex_map"]]],
        ),
        "maximum_original_prior_error_m": float(
            abs(
                mesh["heights"][mesh["original_vertex_map"]]
                - captured["ground_targets"]
            ).max()
        ),
        "mesh_file": path.name,
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "total_ms": (time.monotonic() - began) * 1000,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return valid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=Path)
    parser.add_argument("--segments", type=int, default=8)
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--fit", action="store_true")
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--method", choices=("cone", "lp"), default="cone")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/transport/port-planes/slice-probe"),
    )
    args = parser.parse_args()
    if not 1 <= args.rounds <= 6 or not 1 <= args.segments <= 64:
        parser.error("require rounds 1..6 and segments 1..64")
    if not args.fit and args.seed is None:
        parser.error(
            "require --fit or a SHA-verified --seed for a fixed-field comparison"
        )
    if not np.isfinite(args.seconds) or not 0 <= args.seconds <= 8:
        parser.error("require finite solver seconds in [0,8]")
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("output must be new or empty so stale products cannot pass")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    if args.fit:
        from port_height_library_experiment import run as run_library

        raise SystemExit(0 if run_library(args) else 2)
    raise SystemExit(0 if run(args) else 2)


if __name__ == "__main__":
    main()

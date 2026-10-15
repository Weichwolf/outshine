"""Compare bounded parameter-aligned tessellation with an unchanged captured C1 plan."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from port_plane_adaptive import initial_mesh, quality
from port_plane_contacts import contact_audit
from port_plane_fixture import load_reference
from port_plane_layout import prepare_plane_field
from port_plane_slice_fit import fit_sliced
from port_plane_slices import initial_knots, slice_mesh
from port_plane_system import continuous_matrix, vertex_matrices
from scipy.spatial import cKDTree


def topology_audit(original, mesh):
    faces = mesh["faces"]
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    unique, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    winding = np.bincount(inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1))
    if np.any(counts > 2) or np.any(winding[counts == 2] != 0):
        raise ValueError(
            "sliced world has nonmanifold or inconsistently oriented edges"
        )
    old_faces = original["faces"]
    old_edges = np.concatenate(
        [old_faces[:, [0, 1]], old_faces[:, [1, 2]], old_faces[:, [2, 0]]]
    )
    old_unique, old_counts = np.unique(
        np.sort(old_edges, axis=1), axis=0, return_counts=True
    )
    expected = old_unique[old_counts == 1]
    boundary = unique[counts == 1]
    old_lines = shapely.union_all(shapely.linestrings(original["xy"][expected]))
    new_lines = shapely.union_all(shapely.linestrings(mesh["xy"][boundary]))
    tolerance = 64 * np.finfo(float).eps * max(1, abs(original["xy"]).max())
    _, degree = np.unique(boundary, return_counts=True)
    if (
        np.any(degree != 2)
        or shapely.hausdorff_distance(old_lines, new_lines) > tolerance
        or abs(old_lines.length - new_lines.length) > tolerance * max(1, len(boundary))
    ):
        raise ValueError(
            "sliced world changed the original outer boundary or has a T-junction"
        )
    new_corners = mesh["xy"][faces]
    a, b = new_corners[:, 1] - new_corners[:, 0], new_corners[:, 2] - new_corners[:, 0]
    new_area = 0.5 * (a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
    old_corners = original["xy"][old_faces]
    a, b = old_corners[:, 1] - old_corners[:, 0], old_corners[:, 2] - old_corners[:, 0]
    old_area = 0.5 * (a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
    root_area = np.bincount(mesh["roots"], weights=new_area, minlength=len(old_faces))
    error = float(abs(root_area - old_area).max())
    if error > 1e-6 or np.any(new_area <= 0):
        raise ValueError("sliced world changed resolvable source areas")
    return {
        "outer_edges": len(boundary),
        "original_outer_edges": len(expected),
        "maximum_root_area_error_m2": error,
    }


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


def apply_seed(model, mesh, seed):
    h, equal, _, parts = vertex_matrices(
        model, mesh["xy"], mesh["faces"], mesh["parts"]
    )
    soil = np.flatnonzero(parts < 0)
    distance, nearest = cKDTree(seed["vertices"][:, :2]).query(mesh["xy"][soil])
    if np.max(distance, initial=0) != 0:
        raise ValueError("new unconstrained soil point needs a coupled refit")
    coefficients = np.r_[
        seed["plane_coefficients"], seed["vertices"][nearest, 2] - seed["datum_m"]
    ]
    mesh["heights"] = h @ coefficients + seed["datum_m"]
    error = float(abs(equal @ coefficients).max(initial=0))
    if error > 1e-7:
        raise ValueError("slice source contacts violate their shared plane equalities")
    return coefficients, error


def add_knots(knots, mesh, marked):
    changes = 0
    for part in np.unique(mesh["parts"][marked]):
        if part not in knots:
            continue
        intervals = mesh["intervals"][marked & (mesh["parts"] == part)]
        mid = np.unique(intervals.mean(axis=1))
        updated = np.unique(np.r_[knots[part], mid[np.isfinite(mid)]])
        if len(updated) > 512:
            raise ValueError("slice_part_station_budget")
        changes += len(updated) - len(knots[part])
        knots[part] = updated
    return changes


def solve_slices(args, model, product, captured, original, seed, began):
    knots = initial_knots(model, original, args.segments)
    records, fits = [], 0
    certificates = (
        continuous_matrix(
            model,
            product["vertices"],
            product["indices"],
            product["face_owners"],
            captured["road_frames"],
        )
        if args.fit
        else None
    )
    for iteration in range(args.rounds):
        mesh, note = slice_mesh(model, original, product, captured, knots)
        note["topology"] = topology_audit(original, mesh)
        coefficients, equality_error = apply_seed(model, mesh, seed)
        needs_fit = args.fit and iteration == 0
        if not needs_fit:
            marked, audit, errors = quality(
                model, seed["plane_coefficients"], mesh, product["part_roles"]
            )
            needs_fit = args.fit and audit["bad_soil_faces"] and fits < 3
        fitting = None
        if needs_fit:
            fitted, system, solver = fit_sliced(
                model,
                captured,
                mesh,
                certificates,
                seconds=args.seconds,
                method=args.method,
            )
            fitting = {"system": system, "solver": solver}
            fits += 1
            print(
                json.dumps(dict(stage="coupled-fit", iteration=iteration, **fitting)),
                flush=True,
            )
            if fitted is not None:
                seed, coefficients = fitted
            marked, audit, errors = quality(
                model, seed["plane_coefficients"], mesh, product["part_roles"]
            )
        record = {
            "iteration": iteration,
            "mesh": note,
            "quality": audit,
            "equality_error": equality_error,
            "elapsed_ms": (time.monotonic() - began) * 1000,
            "fitting": fitting,
        }
        records.append(record)
        print(json.dumps(record), flush=True)
        if fitting and not fitting["solver"]["valid"]:
            break
        if not marked.any() or iteration == args.rounds - 1:
            break
        if not add_knots(knots, mesh, marked):
            break
    return mesh, coefficients, seed, marked, errors, records


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
        args, model, product, captured, original, seed, began
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
    raise SystemExit(0 if run(args) else 2)


if __name__ == "__main__":
    main()

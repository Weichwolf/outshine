"""Compare the native height-program boundary with the unchanged Python body."""

import argparse
import json
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from scipy.optimize import OptimizeResult
from stair_junction_body import build
from stair_network_geometry import vertex_links


def native_fit(binary, receipts, objective, **options):
    matrix = options["A_ub"].tocsc(copy=True)
    matrix.sort_indices()
    bounds = options["bounds"]
    nonzero = np.flatnonzero(objective)
    if len(nonzero) != 1 or objective[nonzero[0]] != 1:
        raise ValueError("one declared minimax adjustment variable required")
    lines = [f"{matrix.shape[0]} {matrix.shape[1]} {matrix.nnz} {nonzero[0]}"]
    for array in (matrix.indptr, matrix.indices, matrix.data, options["b_ub"]):
        lines.append(" ".join(map(str, array)))
    lines.extend(
        f"{-np.inf if low is None else low} {np.inf if high is None else high}"
        for low, high in bounds
    )
    process = subprocess.run(
        [str(binary)],
        input="\n".join(lines) + "\n",
        text=True,
        capture_output=True,
        timeout=30,
        check=False,
    )
    output = process.stdout.splitlines()
    receipt = {"returncode": process.returncode, "stderr": process.stderr}
    receipts.append(receipt)
    if process.returncode == 1 and output == ["error 4"]:
        receipt["infeasible"] = True
        return OptimizeResult(success=False, status=2, message="native infeasible")
    if process.returncode != 0 or not output or not output[0].startswith("fit "):
        raise ValueError(f"native height fit failed: {receipt}, {output[:2]}")
    header = output[0].split()
    values = np.array([float(v) for v in output[1:]])
    if values.shape != objective.shape or not np.isfinite(values).all():
        raise ValueError(
            "finite native solution with exactly the declared variables required"
        )
    residual = float(np.max(matrix @ values - options["b_ub"]))
    violations = [
        max(
            -np.inf if low is None else low - value,
            -np.inf if high is None else value - high,
        )
        for value, (low, high) in zip(values, bounds, strict=True)
    ]
    if max(residual, max(violations)) > 1e-9:
        raise ValueError("native values fail the unchanged original program")
    receipt.update(
        adjustment_m=float(header[1]),
        native_maximum_violation=float(header[2]),
        setup_ms=float(header[3]),
        solve_ms=float(header[4]),
        iterations=int(header[5]),
        model_warning=header[6] == "true",
        original_maximum_row_violation=residual,
        original_maximum_variable_violation=max(violations),
        variables=matrix.shape[1],
        constraints=matrix.shape[0],
        nonzeros=matrix.nnz,
    )
    return OptimizeResult(
        success=True,
        status=0,
        message="native original-matrix verified",
        x=values,
        fun=float(objective @ values),
    )


def body(plan, heights, contacts):
    mesh, data, geometry = build(plan, heights)
    result, field = consumed_plan(plan, data)
    proof = audit_profiles(
        result, field, data, max_prior_change_m=3, gradient_model="axis_crossfall"
    )
    fixed = audit_rendered_contacts(result, data, contacts)
    vertex_links(mesh.vertices, mesh.triangles)
    products = shared_products(result, field, data)
    return data, {
        "geometry": geometry,
        "profile_audit": proof,
        "contacts_before_float32_packing": fixed,
        "navigation_nodes": len(products["navigation"]["nodes_xyz_m"]),
    }


def compare(binary, repetitions, output):
    case, sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    plan, contacts = prepare_native_body_plan(
        case, sources, plan_connections(sources), native
    )
    options = {
        "fixed_contacts": contacts,
        "max_prior_change_m": 3,
        "gradient_model": "axis_crossfall",
    }
    reference, baseline_fit = solve_profiles(plan, **options)
    if reference is None:
        raise ValueError("unchanged Python reference must remain feasible")
    baseline_data, baseline = body(plan, reference, contacts)
    receipts = []

    def call(objective, **kwargs):
        return native_fit(binary, receipts, objective, **kwargs)

    with patch("mvt_t_profile_solve.linprog", side_effect=call):
        for _ in range(repetitions):
            heights, fit = solve_profiles(plan, **options)
            if heights is None:
                raise ValueError(fit)
        native_data, native_body = body(plan, heights, contacts)
        controls = []
        for model, cap in (("whole_norm", 3), ("axis_crossfall", 2)):
            values, note = solve_profiles(
                plan,
                fixed_contacts=contacts,
                max_prior_change_m=cap,
                gradient_model=model,
            )
            if values is not None:
                raise ValueError(
                    "an unchanged negative profile control became feasible"
                )
            controls.append(note)
    report = {
        "scope": "Native profile LP only. Layout, mesh, navigation and terrain products still use Python. No native scene repair or frame-budget admission.",
        "native_binary": str(binary),
        "repetitions": repetitions,
        "native_calls": receipts,
        "native_fit": fit,
        "reference_fit": baseline_fit,
        "maximum_reference_height_difference_m": float(
            np.max(abs(heights - reference))
        ),
        "reference_body": baseline,
        "native_height_body": native_body,
        "reference_body_vertices": len(baseline_data["body_vertices"]),
        "native_body_vertices": len(native_data["body_vertices"]),
        "controls": controls,
    }
    output.mkdir(parents=True, exist_ok=True)
    np.savez(output / "native-height-body.npz", **native_data)
    (output / "report.json").write_text(json.dumps(report, indent=2))
    print(
        json.dumps(
            {
                "native_calls": receipts,
                "maximum_reference_height_difference_m": report[
                    "maximum_reference_height_difference_m"
                ],
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.repetitions <= 20:
        parser.error("repetitions must be in [1, 20]")
    compare(args.binary.resolve(), args.repetitions, args.output)


if __name__ == "__main__":
    main()

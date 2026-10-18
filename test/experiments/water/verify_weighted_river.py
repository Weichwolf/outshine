"""Fresh saved-output reader: full physical constraints, original bindings and QP dual."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from scipy import sparse
from stair_terrain_experiment import frozen
from weighted_river_compare import inputs, path_check
from weighted_river_fit import checked, constraints, directions, objective_weights


def verify(products, original):
    report = json.loads((products / "comparison.json").read_text())
    for path, expected in (
        (original, report["input_sha256"]),
        (original.with_suffix(".json"), report["provenance_sha256"]),
    ):
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError("original water geometry or source provenance changed")
    for name, expected in report["artifacts"].items():
        path = products / name
        if (
            not path.resolve().is_relative_to(products.resolve())
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected
        ):
            raise ValueError("saved complete water artifact changed")
    mesh, flow, rows, source, weights, _topology, _coverage, segments, levels = inputs(
        original, report["spacing_m"], report["weights"]
    )
    normalized, directed = directions(mesh, flow, rows)
    pins = {int(i): h for i, h in report["fixed_heights"].items()}
    for name in ("minimax", "weighted"):
        with np.load(products / (name + ".npz"), allow_pickle=False) as saved:
            for key, value in (
                ("triangles", mesh.triangles),
                ("prior", mesh.vertices[:, 2]),
                ("flow", flow),
                ("directed_rows", rows),
                ("directed_source_segment", source),
                ("objective_weights", weights),
            ):
                np.testing.assert_array_equal(saved[key], value)
            np.testing.assert_array_equal(
                saved["vertices"][:, :2], mesh.vertices[:, :2]
            )
            actual = Mesh(saved["vertices"], saved["triangles"])
        limits = checked(
            mesh, actual.vertices[:, 2], normalized, directed, pins, 0.1, 0.005
        )
        if any(
            not np.isfinite(limits[k]) or limits[k] > 1e-7
            for k in (
                "whole_face_violation",
                "source_face_violation",
                "fixed_height_error_m",
            )
        ):
            raise ValueError("saved whole-face/source direction or hard pin fails")
        paths = path_check(actual, segments, levels)
        if (
            paths != report["scores"][name]["source_path"]
            or paths["rising_pairs_over_1e_minus7_m"]
        ):
            raise ValueError("whole covered original source-path check changed")
        residual = actual.vertices[:, 2] - mesh.vertices[:, 2]
        scores = {
            "weighted_rms_m": float(np.sqrt(weights @ residual**2)),
            "maximum_error_m": float(np.abs(residual).max()),
        }
        if any(
            abs(value - report["scores"][name][key]) > 1e-12
            for key, value in scores.items()
        ):
            raise ValueError("saved objective score changed")
    with np.load(products / "problem.npz", allow_pickle=False) as saved:
        matrix = sparse.csc_matrix(
            (saved["matrix_data"], saved["matrix_indices"], saved["matrix_indptr"]),
            shape=tuple(saved["matrix_shape"]),
        )
        rhs, x, dual, slack = (
            saved["rhs"],
            saved["primal"],
            saved["dual"],
            saved["slack"],
        )
        centre = float(saved["reference_height_m"])
        p, q = saved["hessian_diagonal"], saved["linear"]
        count = int(saved["inequality_count"])
        np.testing.assert_array_equal(saved["fixed_indices"], list(pins))
        np.testing.assert_array_equal(saved["fixed_heights"], list(pins.values()))
        expected, bounds = constraints(mesh, normalized, directed, 0.1, 0.005)
        if (matrix[:count] - expected).nnz or not np.array_equal(rhs[:count], bounds):
            raise ValueError("serialized directional matrix differs from physical mesh")
        contact = sparse.coo_matrix(
            (np.ones(len(pins)), (np.arange(len(pins)), list(pins))),
            shape=(len(pins), len(weights)),
        ).tocsc()
        if (matrix[count:] - contact).nnz or not np.array_equal(
            rhs[count:], np.array(list(pins.values())) - centre
        ):
            raise ValueError("serialized distinct hard pins changed")
        np.testing.assert_array_equal(p, objective_weights(weights, len(weights)))
        np.testing.assert_array_equal(q, -p * (mesh.vertices[:, 2] - centre))
        with np.load(products / "weighted.npz") as fitted:
            np.testing.assert_array_equal(x + centre, fitted["vertices"][:, 2])
        stationarity = p * x + q + matrix.T @ dual
        residual = matrix @ x + slack - rhs
        conjugate = q + matrix.T @ dual
        primal = float(0.5 * np.dot(p * x, x) + q @ x)
        lower = float(-0.5 * np.dot(conjugate / p, conjugate) - rhs @ dual)
        metrics = [
            abs(primal - lower),
            np.abs(stationarity).max(initial=0),
            np.abs(residual).max(initial=0),
            np.maximum(-dual[:count], 0).max(initial=0),
            np.maximum(-slack[:count], 0).max(initial=0),
            np.abs(slack[count:]).max(initial=0),
        ]
        if not np.isfinite(metrics).all() or max(metrics) > 1e-7:
            raise ValueError("saved quadratic primal/dual certificate failed")
    return {
        "reader_commit": frozen(),
        "producer_commit": report["commit"],
        "report_sha256": hashlib.sha256(
            (products / "comparison.json").read_bytes()
        ).hexdigest(),
        "whole_constraints_paths_pins_bindings_and_dual_checked": True,
        "maximum_certificate_residual": float(max(metrics)),
        "scope": report["scope"],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh private /tmp receipt required")
    result = verify(args.products, args.input)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))

"""Replay original physical water inputs through an explicit source or wheel root."""

import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path

import numpy as np
import outshine_ref
from compare_linear_physical_river import captured_solve, plot
from compare_physical_river import audit, inputs
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from scipy import sparse


def replay(args):
    root = args.package_root.resolve()
    if not Path(outshine_ref.__file__).resolve().is_relative_to(root):
        raise ValueError(
            "reference package did not load from the requested source/wheel root"
        )
    mesh, anchor, rows, box, arrays, report = inputs(args.input)
    with np.load(args.reference, allow_pickle=False) as old:
        reference = {key: old[key].copy() for key in old.files}
    for key in arrays:
        if key in reference:
            np.testing.assert_array_equal(arrays[key], reference[key], err_msg=key)
    primary = np.c_[
        np.arange(len(mesh.triangles)),
        np.arange(len(mesh.triangles)) + len(mesh.triangles),
    ]
    results = {}
    for name in ("soc", "linear"):
        fit, captured, peak = captured_solve(mesh, anchor, rows, box, primary, name)
        if fit.mesh is None or not fit.report["finite_qp_optimal"]:
            raise ValueError(
                f"library {name} did not produce the checked reference: {fit.report}"
            )
        np.testing.assert_array_equal(fit.mesh.vertices, reference[name + "_vertices"])
        arrays[name + "_vertices"] = fit.mesh.vertices
        results[name] = dict(
            fit.report,
            original_vertices_bit_identical=True,
            python_traced_peak_bytes=peak,
        )
        if name == "linear":
            for key, filename in (
                ("p", "linear-hessian-upper.npz"),
                ("a", "linear-constraints.npz"),
            ):
                old_matrix = sparse.load_npz(args.reference.parent / filename)
                for field in ("data", "indices", "indptr"):
                    np.testing.assert_array_equal(
                        getattr(captured[key], field), getattr(old_matrix, field)
                    )
                sparse.save_npz(args.output / filename, captured[key])
            with np.load(args.reference.parent / "linear-vectors.npz") as vectors:
                np.testing.assert_array_equal(captured["q"], vectors["q"])
                np.testing.assert_array_equal(captured["b"], vectors["upper"])
                np.testing.assert_array_equal(
                    np.full(len(captured["b"]), -np.inf), vectors["lower"]
                )
                np.testing.assert_array_equal(
                    np.median(mesh.vertices[:, 2]), vectors["datum"]
                )
            np.savez_compressed(
                args.output / "linear-vectors.npz",
                q=captured["q"],
                upper=captured["b"],
                lower=np.full(len(captured["b"]), -np.inf),
                datum=np.median(mesh.vertices[:, 2]),
                solution=captured["solution"],
                dual=captured["dual"],
            )
    values = arrays["linear_vertices"][:, 2]
    pins = {
        int(values.argmin()): float(values.min()),
        int(values.argmax()): float(values.max()),
    }
    changed = mesh.vertices.copy()
    changed[:, 2] += 0.25
    pinned = fit_physical_surface(
        Mesh(changed, mesh.triangles),
        anchor,
        rows,
        up_bounds_m=box,
        fixed=pins,
        primary_rows=primary,
    )
    if pinned.mesh is None or not pinned.report["finite_qp_optimal"]:
        raise ValueError(f"synthetic shared-contact test failed: {pinned.report}")
    sampled, _ = audit(pinned.mesh, anchor, rows, box, arrays["row_tangent"])
    if sampled["sampled_physical_grade_violation"] > 1e-9:
        raise ValueError("installed pinned field violated the declared physical grade")
    arrays["pinned_vertices"] = pinned.mesh.vertices
    arrays["osqp_vertices"] = reference["osqp_vertices"]
    report.update(
        methods=results,
        pinned=dict(pinned.report, **sampled),
        synthetic_fixed_contacts=pins,
        actual_package=str(Path(outshine_ref.__file__).resolve()),
        reference_sha256=hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        scope="unchanged installed physical kernel; original metadata/geometry/source membership, actual QP matrices and soft target unchanged; synthetic contacts are not observed levels; frozen OSQP control reused without rerun",
    )
    report["water_module_paths"] = {
        name: module.__file__
        for name, module in sys.modules.copy().items()
        if name.startswith("outshine_ref.water") and getattr(module, "__file__", None)
    }
    if not all(
        Path(path).resolve().is_relative_to(root)
        for path in report["water_module_paths"].values()
    ):
        raise ValueError("water core leaked from another checkout/package")
    np.savez_compressed(args.output / "library-replay.npz", **arrays)
    plot(arrays, args.output)
    report["artifacts_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in args.output.iterdir()
        if p.is_file()
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "package": report["actual_package"],
                "methods": results,
                "pinned": report["pinned"],
            },
            indent=2,
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    if not args.output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or args.output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp outside the checkout")
    args.output.mkdir(parents=True, exist_ok=False)
    replay(args)


if __name__ == "__main__":
    main()

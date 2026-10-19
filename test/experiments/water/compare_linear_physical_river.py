"""Same physical quality: SOCP versus derived-bound QP, original Ill inputs (#760)."""

import argparse
import hashlib
import importlib.util
import json
import os
import platform
import tempfile
import time
import tracemalloc
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import outshine_ref.water._program as program
from compare_physical_river import audit, inputs
from native_profile_fit import inspect_solution
from outshine_ref.water import fit_physical_surface
from scipy import sparse


def options(name, primary):
    return (
        {}
        if name == "soc"
        else {
            "primary_rows": primary,
            "solver": "osqp" if name == "osqp" else "clarabel",
            "iterations": 50000,
        }
    )


def captured_solve(mesh, anchor, rows, box, primary, name):
    captured = {}
    constructor, osqp_solve = program.clarabel.DefaultSolver, program.solve_osqp

    def clarabel(p, q, a, b, cones, settings):
        if name == "linear":
            captured.update(p=p.copy(), q=q.copy(), a=a.copy(), b=b.copy())
        solver = constructor(p, q, a, b, cones, settings)

        def solve():
            result = solver.solve()
            if name == "linear":
                captured.update(
                    solution=np.asarray(result.x), dual=np.asarray(result.z)
                )
            return result

        return SimpleNamespace(solve=solve)

    def osqp_call(p, q, a, b, inequalities, seconds, iterations):
        captured.update(p=sparse.triu(p).tocsc(), q=q.copy(), a=a.copy(), b=b.copy())
        result = osqp_solve(p, q, a, b, inequalities, seconds, iterations)
        captured.update(solution=np.asarray(result.x), dual=np.asarray(result.z))
        return result

    tracemalloc.start()
    with (
        patch.object(program.clarabel, "DefaultSolver", side_effect=clarabel),
        patch.object(program, "solve_osqp", side_effect=osqp_call),
    ):
        fit = fit_physical_surface(
            mesh, anchor, rows, up_bounds_m=box, seconds=10, **options(name, primary)
        )
    peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    return fit, captured, peak


def run(path, output, repeats):
    import osqp

    began = time.perf_counter()
    mesh, anchor, rows, box, arrays, report = inputs(path)
    report["input_wall_seconds"] = time.perf_counter() - began
    report["host_load_average_begin"] = (
        os.getloadavg() if hasattr(os, "getloadavg") else None
    )
    primary = np.c_[
        np.arange(len(mesh.triangles)),
        np.arange(len(mesh.triangles)) + len(mesh.triangles),
    ]
    arrays["primary_rows"] = primary
    report["methods"], captures = {}, {}
    for name in ("soc", "linear", "osqp"):
        fit, captured, peak = captured_solve(mesh, anchor, rows, box, primary, name)
        if fit.mesh is None:
            raise RuntimeError(f"{name} absent: {fit.report}")
        sampled, sample_arrays = audit(
            fit.mesh, anchor, rows, box, arrays["row_tangent"]
        )
        report["methods"][name] = dict(
            fit.report, **sampled, python_traced_peak_bytes=peak
        )
        arrays[name + "_vertices"] = fit.mesh.vertices
        arrays[name + "_sample_grade_minmax"] = sample_arrays["sample_grade_minmax"]
        if captured:
            captures[name] = captured
    for key in ("p", "a"):
        if (captures["linear"][key] - captures["osqp"][key]).nnz:
            raise ValueError("actual solver CSC matrices differ")
    for key in ("q", "b"):
        np.testing.assert_array_equal(captures["linear"][key], captures["osqp"][key])
    report["linear_solver_inputs_bit_identical"] = True
    record = captures["linear"]
    sparse.save_npz(output / "linear-hessian-upper.npz", record["p"])
    sparse.save_npz(output / "linear-constraints.npz", record["a"])
    np.savez_compressed(
        output / "linear-vectors.npz",
        q=record["q"],
        upper=record["b"],
        lower=np.full(len(record["b"]), -np.inf),
        datum=np.median(mesh.vertices[:, 2]),
        solution=record["solution"],
        dual=record["dual"],
    )
    full = record["p"] + record["p"].T - sparse.diags(record["p"].diagonal())
    report["export_primal_dual_reader"] = inspect_solution(
        (
            record["a"],
            record["b"],
            record["q"],
            full,
            np.full(len(record["q"]), -np.inf),
            np.full(len(record["q"]), np.inf),
        ),
        record["solution"],
        np.zeros(len(record["q"])),
        -record["dual"],
    )
    report["cold_passes"] = []
    for repeat in range(repeats):
        order = (
            ("soc", "linear", "osqp") if repeat % 2 == 0 else ("osqp", "linear", "soc")
        )
        for name in order:
            fit = fit_physical_surface(
                mesh,
                anchor,
                rows,
                up_bounds_m=box,
                seconds=10,
                **options(name, primary),
            )
            if not fit.report["valid"]:
                raise RuntimeError(f"cold {name} rejected: {fit.report}")
            report["cold_passes"].append(
                {
                    "pass": repeat,
                    "method": name,
                    **{
                        key: fit.report[key]
                        for key in (
                            "solver_api_wall_ms",
                            "solver_api_cpu_ms",
                            "total_ms",
                            "iterations",
                            "finite_qp_optimal",
                        )
                    },
                }
            )
    for name in ("linear", "osqp"):
        difference = arrays[name + "_vertices"][:, 2] - arrays["soc_vertices"][:, 2]
        report["methods"][name]["maximum_height_difference_from_soc_m"] = float(
            abs(difference).max()
        )
    report["osqp_linear_height_difference_m"] = float(
        abs(arrays["osqp_vertices"][:, 2] - arrays["linear_vertices"][:, 2]).max()
    )
    report["retained_array_bytes"] = sum(a.nbytes for a in arrays.values())
    report["host_load_average_end"] = (
        os.getloadavg() if hasattr(os, "getloadavg") else None
    )
    report["osqp_version"] = osqp.__version__
    report["environment"] = {
        "system": platform.system(),
        "machine": platform.machine(),
        "cpu": next(
            (
                line.split(":", 1)[1].strip()
                for line in Path("/proc/cpuinfo").read_text().splitlines()
                if line.startswith("model name")
            ),
            "unknown",
        )
        if Path("/proc/cpuinfo").exists()
        else platform.processor(),
        "numpy": np.__version__,
        "clarabel": program.clarabel.__version__,
    }
    report["scope"] = (
        "same declared physical quality and objective; linear enclosure is more conservative, not an identical feasible set; cold host Python timings, native allocations/copies/factorization work unknown"
    )
    report["sources_sha256"] = {
        name: hashlib.sha256(
            Path(importlib.util.find_spec(name).origin).read_bytes()
        ).hexdigest()
        for name in (
            "outshine_ref.water",
            "outshine_ref.water.geodetic",
            "outshine_ref.water.frame",
            "outshine_ref.water._fit",
            "outshine_ref.water._program",
            "outshine_ref.water._mass",
        )
    }
    report["sources_sha256"].update(
        {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "compare_physical_river.py",
                "compare_linear_physical_river.py",
            )
        }
    )
    np.savez_compressed(output / "comparison.npz", **arrays)
    return arrays, report


def plot(arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    xyz = arrays["soc_vertices"]
    linear_error = arrays["linear_vertices"][:, 2] - xyz[:, 2]
    osqp_error = arrays["osqp_vertices"][:, 2] - arrays["linear_vertices"][:, 2]
    centre = xyz[np.argmax(abs(linear_error)), :2]
    fig, axes = plt.subplots(1, 3, figsize=(12.8, 5.5), layout="constrained")
    for ax, values, title, unit, cmap in zip(
        axes,
        (xyz[:, 2], linear_error * 1e6, osqp_error * 1e6),
        (
            "Physical SOCP reference",
            "Linear-bound QP − SOCP",
            "OSQP − same QP / Clarabel",
        ),
        ("ENU Up [m]", "Height difference [µm]", "Height difference [µm]"),
        ("viridis", "coolwarm", "coolwarm"),
        strict=True,
    ):
        image = ax.tripcolor(
            *xyz[:, :2].T,
            values,
            triangles=arrays["triangles"],
            cmap=cmap,
            shading="gouraud",
            vmin=float(values.min()),
            vmax=float(values.max()),
        )
        ax.set(
            xlim=(centre[0] - 150, centre[0] + 150),
            ylim=(centre[1] - 150, centre[1] + 150),
            aspect="equal",
            title=title,
            xlabel="ENU East [m]",
            ylabel="ENU North [m]",
        )
        fig.colorbar(image, ax=ax, label=unit, orientation="horizontal")
    fig.suptitle(
        "Original Ill: same physical grade bounds; crop at largest QP versus SOCP difference"
    )
    fig.savefig(output / "linear-comparison.png", dpi=120)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=2)
    args = parser.parse_args()
    out = args.output.resolve()
    if (
        not out.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or out.is_relative_to(Path(__file__).resolve().parents[3])
        or not 1 <= args.repeats <= 4
    ):
        parser.error(
            "system-temp output outside checkout and 1..4 cold passes required"
        )
    out.mkdir(parents=True, exist_ok=False)
    arrays, report = run(args.input, out, args.repeats)
    plot(arrays, out)
    report["artifacts_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in out.iterdir()
        if p.is_file()
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"cold_passes": report["cold_passes"], "methods": report["methods"]},
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

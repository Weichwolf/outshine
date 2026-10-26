"""Build native solver probes, run analytic cases and compare one reproducible real body."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

import numpy as np
from directional_river_fit import infer_flow, source_face_rows
from native_profile_checks import checks
from native_profile_fit import run_native
from outshine_ref.contracts import Mesh
from river_linear_model import linear_system
from river_profiles import RiverProfile
from river_surface import source_body, surface_mesh
from weighted_river_fit import constraints, directions

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def build(output):
    destination = output / "native-build"
    command = [
        "cmake",
        "-S",
        str(HERE),
        "-B",
        str(destination),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    if not os.environ.get("CMAKE_PREFIX_PATH") and shutil.which("brew"):
        command.append(
            "-DCMAKE_PREFIX_PATH="
            + subprocess.check_output(["brew", "--prefix"], text=True).strip()
        )
    with (output / "build.log").open("w") as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        subprocess.run(
            ["cmake", "--build", str(destination), "--parallel", "2"],
            check=True,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--input", type=Path, default=HERE / "fixtures/feldkirch-ill-profile.npz"
    )
    parser.add_argument("--spacing-m", type=float, default=20.0)
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--iterations", type=int, default=20000)
    args = parser.parse_args()
    output = args.output.resolve()
    if not np.isfinite(args.seconds) or args.seconds < 0 or args.iterations < 0:
        parser.error("finite nonnegative solver budgets required")
    if output.is_relative_to(ROOT):
        parser.error("private builds and model outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    native = build(output)
    binaries = {name: native / (name + "-profile-probe") for name in ("highs", "osqp")}
    receipt = {
        "scope": "native finite-problem comparison; not engine, Float32 or place admission",
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "directions": "procedural hypotheses, not supplied OSM flow",
        "seconds": args.seconds,
        "iterations": args.iterations,
        "results": [],
        "checks": {},
    }
    for name, binary in binaries.items():
        receipt["checks"][name] = checks(binary, output / "checks" / name)
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    with np.load(args.input, allow_pickle=False) as data:
        flat, topology = surface_mesh(source_body(data), args.spacing_m)
        target, _ = RiverProfile(data["segment_nodes"], data["segment_levels"]).sample(
            flat.vertices[:, :2], "blend-affine"
        )
        mesh = Mesh(np.c_[flat.vertices[:, :2], target], flat.triangles)
        flow, source, _ = infer_flow(
            mesh, data["segment_nodes"], data["segment_levels"]
        )
        rows, row_source, coverage = source_face_rows(
            mesh, data["segment_nodes"], data["segment_levels"]
        )
    receipt.update(topology=topology, source_path_coverage=coverage)
    normalized_flow, normalized_rows = directions(mesh, flow, rows)
    reference, rhs = constraints(mesh, normalized_flow, normalized_rows, 0.1, 0.005)
    native_rows, native_rhs, _, _ = linear_system(
        mesh, normalized_flow, rows, 0.1, 0.005
    )
    native_rows = native_rows[2 * len(mesh.vertices) :, : len(mesh.vertices)]
    assert (reference.tocsr() != native_rows).nnz == 0
    np.testing.assert_array_equal(rhs, native_rhs[2 * len(mesh.vertices) :])
    receipt["research_constraint_rows_exact"] = True
    for name, binary in binaries.items():
        objectives = (
            ("linf", "l2-uniform", "l2-lumped", "l2-area")
            if name == "highs"
            else ("l2-uniform", "l2-lumped", "l2-area")
        )
        for objective in objectives:
            destination = output / name / objective
            height, report = run_native(
                binary,
                destination,
                mesh,
                flow,
                rows,
                objective,
                seconds=args.seconds,
                iterations=args.iterations,
            )
            receipt["results"].append(report)
            (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            if height is not None:
                vertices = mesh.vertices.copy()
                vertices[:, 2] = height
                np.savez_compressed(
                    destination / "surface.npz",
                    vertices=vertices,
                    triangles=mesh.triangles,
                    prior=target,
                    flow=flow,
                    source_segment=source,
                    directed_rows=rows,
                    directed_source_segment=row_source,
                )
            print(json.dumps(report), flush=True)
    assert all(r["valid"] for r in receipt["results"]), (
        "failed native models retained in receipt"
    )
    print("analytic checks:", sum(map(len, receipt["checks"].values())), "passed")


if __name__ == "__main__":
    main()

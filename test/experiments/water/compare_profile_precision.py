"""Compare coordinate storage on the fixed real water body; private outputs only."""

import argparse
import hashlib
import json
import platform
import time
import tracemalloc
from pathlib import Path

import numpy as np
from directional_river_fit import infer_flow, source_face_rows
from outshine_ref.contracts import Mesh
from precision_reference import solve
from profile_precision import compare, encode, gradients
from river_profiles import RiverProfile
from river_surface import source_body, surface_mesh

HERE = Path(__file__).resolve().parent


def diagnostic(vertices, faces, decoded, flow, rows, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    from matplotlib.colors import Normalize
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    low, high = vertices.min(axis=0), vertices.max(axis=0)
    span = np.maximum(high - low, 1)
    norm = Normalize(low[2], high[2])
    figure = plt.figure(figsize=(12.8, 7.2), layout="constrained")
    for index, mode in enumerate(("float64", "local32"), 1):
        points = decoded[mode]
        axis = figure.add_subplot(2, 2, index, projection="3d")
        colours = plt.cm.viridis(norm(points[faces, 2].mean(axis=1)))
        axis.add_collection3d(Poly3DCollection(points[faces], facecolors=colours))
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2] - 1, high[2] + 1),
            title=mode + "; height, Z x60",
        )
        axis.set_box_aspect((span[0], span[1], span[2] * 60))
        axis.set_proj_type("ortho")
        axis.view_init(elev=27, azim=-60)
    for index, mode in enumerate(("absolute32", "local32"), 3):
        axis = figure.add_subplot(2, 2, index)
        gradient = gradients(decoded[mode], faces)
        along = np.einsum("fi,fi->f", gradient, flow)
        across = np.einsum("fi,fi->f", gradient, np.c_[-flow[:, 1], flow[:, 0]])
        violation = np.maximum.reduce((along, -along - 0.1, np.abs(across) - 0.005))
        source_face = rows[:, 0].astype(int)
        directed = np.einsum("fi,fi->f", gradient[source_face], rows[:, 1:3])
        np.maximum.at(
            violation,
            source_face,
            np.maximum(directed - rows[:, 4], rows[:, 3] - directed),
        )
        failures = violation > 1e-7
        axis.add_collection(
            PolyCollection(
                vertices[faces, :2],
                facecolors=np.where(
                    failures[:, None], [[0.85, 0.15, 0.05, 1]], [[0.65, 0.75, 0.8, 1]]
                ),
                edgecolors="none",
            )
        )
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            title=f"{mode}: {failures.sum()} faces violate constraints > 1e-7 m/m",
            xlabel="East [m]",
            ylabel="North [m]",
        )
        axis.set_aspect("equal")
    figure.suptitle(
        "Python storage diagnostic; identical full body, axes and height colours"
    )
    figure.savefig(output, dpi=100)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input", type=Path, default=HERE / "fixtures/feldkirch-ill-profile.npz"
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--spacing-m", type=float, default=20.0)
    parser.add_argument("--solver-eps", type=float, default=1e-9)
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(HERE.parents[2]):
        parser.error("private results belong outside the checkout")
    if output.exists() and any(output.iterdir()):
        parser.error(
            "use an empty output directory to avoid stale successful artifacts"
        )
    output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    tracemalloc.start()
    with np.load(args.input, allow_pickle=False) as data:
        flat, topology = surface_mesh(source_body(data), args.spacing_m)
        prior, _ = RiverProfile(data["segment_nodes"], data["segment_levels"]).sample(
            flat.vertices[:, :2], "blend-affine"
        )
        mesh = Mesh(np.c_[flat.vertices[:, :2], prior], flat.triangles)
        flow, source, _ = infer_flow(
            mesh, data["segment_nodes"], data["segment_levels"]
        )
        rows, row_source, coverage = source_face_rows(
            mesh, data["segment_nodes"], data["segment_levels"]
        )
    try:
        height, flow, rows, reference = solve(
            mesh, flow, rows, solver_eps=args.solver_eps
        )
    except (RuntimeError, ValueError) as error:
        (output / "failure.json").write_text(json.dumps({"failure": str(error)}) + "\n")
        raise
    vertices = np.c_[mesh.vertices[:, :2], height]
    origin, reports, decoded = compare(vertices, mesh.triangles, flow, rows)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if not reports["float64"]["passes_finite_precision_checks"]:
        raise RuntimeError("original complete-face reference did not pass")
    # Distinct region datums can decode a shared height differently, even locally.
    seam = np.array([[0.0, 0.0, 500.000123]])
    a, _ = encode(seam, "local32", np.array([0.0, 0.0, 500.0]))
    b, _ = encode(seam, "local32", np.array([0.0, 0.0, 400.0]))
    receipt = {
        "scope": "finite storage check, not native arithmetic, topology coverage or device admission",
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "spacing_m": args.spacing_m,
        "topology": topology,
        "source_path_coverage": coverage,
        "host": platform.node(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "reference": reference,
        "origin": origin.tolist(),
        "modes": reports,
        "independent_datum_seam_gap_m": float(abs(a[0, 2] - b[0, 2])),
        "traced_peak_bytes": peak,
        "preparation_solve_compare_ms": (time.perf_counter() - start) * 1000,
        "memory_scope": "tracemalloc process allocations while preparing, solving and comparing; not RSS/native solver peak",
    }
    np.savez_compressed(
        output / "surface.npz",
        vertices=vertices,
        triangles=mesh.triangles,
        prior=prior,
        flow=flow,
        directed_rows=rows,
        source_segment=source,
        directed_source_segment=row_source,
        origin=origin,
        **{mode: value for mode, value in decoded.items()},
    )
    receipt["surface_sha256"] = hashlib.sha256(
        (output / "surface.npz").read_bytes()
    ).hexdigest()
    (output / "receipt.json").write_text(
        json.dumps(receipt, indent=2, allow_nan=False) + "\n"
    )
    if args.plot:
        diagnostic(
            vertices, mesh.triangles, decoded, flow, rows, output / "precision.png"
        )
    for mode, report in reports.items():
        print(mode, json.dumps(report, allow_nan=False))


if __name__ == "__main__":
    main()

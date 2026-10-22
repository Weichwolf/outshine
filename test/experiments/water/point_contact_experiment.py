"""Original Ill: free soft-prior movement versus hard physical facet contacts (#760)."""

import argparse
import hashlib
import json
import platform
import tempfile
import time
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from compare_physical_river import inputs
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.water import _program, fit_physical_surface
from outshine_ref.water.frame import inverse_frame
from outshine_ref.water.geodetic import WGS84_A, query_facet
from scipy import sparse


def chosen_contacts(mesh):
    triangle = mesh.vertices[mesh.triangles]
    area = np.linalg.norm(
        np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]),
        axis=1,
    )
    eligible = np.flatnonzero(area >= np.median(area))
    centre = triangle.mean(axis=1)
    selected = eligible[
        [np.argmin(centre[eligible, 2]), np.argmax(centre[eligible, 2])]
    ]
    edges = np.sort(
        np.concatenate(
            [
                mesh.triangles[:, [0, 1]],
                mesh.triangles[:, [1, 2]],
                mesh.triangles[:, [2, 0]],
            ]
        ),
        axis=1,
    )
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    shared = edges[counts == 2]
    middle = mesh.vertices[shared].mean(axis=1)
    index = np.argmin(
        np.linalg.norm(middle[:, :2] - mesh.vertices[:, :2].mean(axis=0), axis=1)
    )
    return np.r_[centre[selected], middle[index, None]]


def independently_checked(mesh, points, anchor):
    selected = SurfaceSampler(mesh, xy_tolerance_m=0).sample(points[:, :2])
    if any(selected.reasons):
        raise ValueError("physical contact disappeared from unchanged XY topology")
    triangle = mesh.vertices[mesh.triangles[selected.face_indices]]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    up_error = np.sum((points - triangle[:, 0]) * normal, axis=1) / normal[:, 2]
    geographic, height, _up = inverse_frame(anchor, points)
    queried = [
        query_facet(anchor, t, [p], metric_height_m=0)["plane_height_m"][0]
        for t, p in zip(triangle, geographic, strict=True)
    ]
    return {
        "selected_faces": selected.face_indices.tolist(),
        "point_up_error_m": up_error.tolist(),
        "point_up_error_convention": "requested minus returned at fixed XY",
        "maximum_point_up_error_m": float(abs(up_error).max()),
        "geodetic_query_error_m": (np.asarray(queried) - height).tolist(),
    }


def run(args):
    mesh, anchor, rows, box, arrays, report = inputs(args.input)
    primary = np.c_[
        np.arange(len(mesh.triangles)),
        np.arange(len(mesh.triangles)) + len(mesh.triangles),
    ]
    base = fit_physical_surface(
        mesh, anchor, rows, up_bounds_m=box, primary_rows=primary
    )
    if base.mesh is None:
        raise ValueError("reference field absent")
    points = chosen_contacts(base.mesh)
    changed = mesh.vertices.copy()
    changed[:, 2] += 0.25
    shifted = Mesh(changed, mesh.triangles)
    free = fit_physical_surface(
        shifted, anchor, rows, up_bounds_m=box, primary_rows=primary
    )
    captured = {}
    constructor = _program.clarabel.DefaultSolver

    def record(p, q, a, b, cones, settings):
        captured.update(p=p.copy(), q=q.copy(), a=a.copy(), b=b.copy())
        solver = constructor(p, q, a, b, cones, settings)

        def solve():
            result = solver.solve()
            captured.update(x=np.asarray(result.x), dual=np.asarray(result.z))
            return result

        return SimpleNamespace(solve=solve)

    began = time.perf_counter()
    with patch.object(_program.clarabel, "DefaultSolver", side_effect=record):
        bound = fit_physical_surface(
            shifted,
            anchor,
            rows,
            up_bounds_m=box,
            primary_rows=primary,
            contact_points=points,
        )
    report["bound_call_wall_ms"] = (time.perf_counter() - began) * 1000
    if free.mesh is None or bound.mesh is None:
        raise ValueError(f"contact comparison absent: {free.report} {bound.report}")
    report["free"] = dict(
        free.report, **independently_checked(free.mesh, points, anchor)
    )
    report["bound"] = dict(
        bound.report, **independently_checked(bound.mesh, points, anchor)
    )
    if report["bound"]["maximum_point_up_error_m"] > 1e-9:
        raise ValueError("physical facet did not preserve the hard point")
    if max(
        abs(np.asarray(report["bound"]["geodetic_query_error_m"]))
    ) > 64 * np.spacing(WGS84_A):
        raise ValueError(
            "geodetic query disagrees beyond established ECEF roundoff allowance"
        )
    report["scope"] = (
        "real original mesh; three synthetic physical contacts from baseline facets, not measured levels; +0.25m soft-prior control; no native or region/terrain/bridge admission"
    )
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
        "clarabel": _program.clarabel.__version__,
    }
    arrays.update(
        reference_vertices=base.mesh.vertices,
        free_vertices=free.mesh.vertices,
        bound_vertices=bound.mesh.vertices,
        contact_points=points,
    )
    np.savez_compressed(args.output / "point-contacts.npz", **arrays)
    sparse.save_npz(args.output / "hessian-upper.npz", captured["p"])
    sparse.save_npz(args.output / "constraints.npz", captured["a"])
    np.savez_compressed(
        args.output / "vectors.npz",
        q=captured["q"],
        upper=captured["b"],
        lower=np.r_[
            np.full(len(captured["b"]) - len(points), -np.inf),
            captured["b"][-len(points) :],
        ],
        inequality_rows=len(captured["b"]) - len(points),
        solution=captured["x"],
        dual=captured["dual"],
        datum=np.median(changed[:, 2]),
    )
    return arrays, report


def plot(arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    points = arrays["contact_points"]
    fig, axes = plt.subplots(1, 2, figsize=(12.8, 6.0), layout="constrained")
    image = axes[0].tripcolor(
        *arrays["bound_vertices"][:, :2].T,
        arrays["bound_vertices"][:, 2] - arrays["free_vertices"][:, 2],
        triangles=arrays["triangles"],
        cmap="coolwarm",
        shading="gouraud",
    )
    axes[0].scatter(*points[:, :2].T, s=55, facecolors="none", edgecolors="black")
    axes[0].set(
        title="Original Ill: physical point contacts",
        xlabel="ENU East [m]",
        ylabel="ENU North [m]",
        aspect="equal",
    )
    fig.colorbar(image, ax=axes[0], label="Constrained − free ENU Up [m]")
    for name, label in [
        ("free", "Free shifted prior"),
        ("bound", "Explicit physical contacts"),
    ]:
        mesh = Mesh(arrays[name + "_vertices"], arrays["triangles"])
        sample = SurfaceSampler(mesh, xy_tolerance_m=0).sample(points[:, :2])
        axes[1].plot(
            np.arange(len(points)),
            sample.points_enu_m[:, 2] - points[:, 2],
            marker="o",
            label=label,
        )
    axes[1].set(
        title="Same physical points / unchanged topology",
        xlabel="Synthetic contact index",
        ylabel="Returned Up − requested Up [m]",
        xticks=range(len(points)),
    )
    axes[1].axhline(0, color="grey", linewidth=0.7)
    axes[1].legend()
    fig.savefig(output / "point-contacts.png", dpi=120)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    if not args.output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or args.output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp outside checkout")
    args.output.mkdir(parents=True, exist_ok=False)
    arrays, report = run(args)
    plot(arrays, args.output)
    report["artifacts_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in args.output.iterdir()
        if p.is_file()
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: report[k] for k in ("free", "bound", "bound_call_wall_ms")}, indent=2
        )
    )


if __name__ == "__main__":
    main()

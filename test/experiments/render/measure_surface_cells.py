"""Complete known native surface-cell costs; no renderer or material admission."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from check_building_capture_inputs import check, load
from full_view_reference import load as load_full_view
from native_capture_meshes import mesh
from surface_cell_oracle import clipped
from surface_cells import assemble, overlap


def controls(triangles, result):
    grid = (triangles.astype(float) - result["origin"]) / result["pitch"]
    count = len(result["original_faces"])
    cases = []
    if count:
        for index in np.linspace(0, count - 1, min(64, count)).astype(int):
            cell = np.searchsorted(result["offsets"], index, side="right") - 1
            cases.append(
                (
                    int(result["original_faces"][index]),
                    result["coordinates"][cell].astype(float) + 0.5,
                )
            )
    rng = np.random.default_rng(885)
    for face in np.linspace(0, len(grid) - 1, min(64, len(grid))).astype(int):
        low = np.floor(grid[face].min(0)) - 1
        high = np.ceil(grid[face].max(0)) + 1
        centre = np.floor(rng.uniform(low, high)) + 0.5
        cases.append((int(face), centre))
    extras = 0
    for face, centre in cases:
        actual = bool(overlap(grid[face], centre[None])[0][0])
        expected = clipped(grid[face], centre)
        if expected and not actual:
            raise ValueError("exact clipping found an omitted triangle/cell contact")
        extras += actual and not expected
    return {
        "exact_clipping_cases": len(cases),
        "omitted_contacts": 0,
        "guarded_extra_contacts": extras,
        "scope": "binary grid inputs as exact rationals; sample control, not all-pair certification",
    }


def picture(result, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, panels = plt.subplots(1, 3, figsize=(12.8, 7.2), dpi=100)
    coordinates = result["coordinates"].astype(np.int64)
    for panel, (a, b) in zip(panels, ((0, 1), (0, 2), (1, 2)), strict=True):
        high = coordinates.max(0, initial=0) + 1
        stride = max(1, int(np.ceil(high.max() / 512)))
        size = (high + stride - 1) // stride
        density = np.zeros((size[b], size[a]), np.uint32)
        np.add.at(
            density, (coordinates[:, b] // stride, coordinates[:, a] // stride), 1
        )
        panel.imshow(np.log1p(density), origin="lower", interpolation="nearest")
        panel.set_title(f"Grid {a}/{b}; density bins {stride} cells")
    figure.suptitle(
        "Complete known surface-cell occupancy; not visibility or a runtime image"
    )
    figure.tight_layout()
    figure.savefig(output / "occupancy.png")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(
            "test/outshine/data/render/building-capture-movement/native-cell.json.gz"
        ),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pitch", type=float, default=1.0)
    parser.add_argument("--representation", choices=("Full", "Fine"), default="Full")
    args = parser.parse_args()
    document = load(args.input)
    if "view_key" in document:
        if args.representation != "Full":
            raise ValueError("whole-view input contains its actual full detail only")
        source, document, _context, input_receipt = load_full_view(args.input)
    else:
        check(document)
        calls = [c for r in document["results"] for c in r["projection_calls"]]
        if len(calls) != 1:
            raise ValueError("one recorded complete known surface group required")
        source = mesh(calls[0], args.representation)
        input_receipt = {"scope": "existing known native capture-input Full/Fine group"}
    triangles = source["positions"][source["indices"]]
    tracemalloc.start()
    start, cpu = time.perf_counter(), time.process_time()
    result, work = assemble(triangles, pitch=args.pitch)
    cost = {
        "wall_s": time.perf_counter() - start,
        "cpu_s": time.process_time() - cpu,
        "traced_peak_bytes": tracemalloc.get_traced_memory()[1],
        "allocation_count": None,
    }
    tracemalloc.stop()
    validation = controls(triangles, result)
    args.output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(args.output / "cells.npz", **result)
    picture(result, args.output)
    vertex_bytes = sum(
        source[name].dtype.itemsize * np.prod(source[name].shape[1:])
        for name in ("positions", "uv", "normal", "colour", "metres")
    )
    fallback = result["fallback"]
    fallback_vertices = np.unique(source["indices"][fallback])
    comparison = {
        "original_declared_u32_geometry_bytes": int(
            len(source["positions"]) * vertex_bytes + source["indices"].size * 4
        ),
        "fallback_declared_u32_geometry_bytes": int(
            len(fallback_vertices) * vertex_bytes + len(fallback) * 12
        ),
        "optimistic_14B_cells_bytes": 14 * work["occupied_cells"],
        "complete_original_face_csr_bytes": result["offsets"].nbytes
        + result["original_faces"].nbytes,
        "scope": "declared direct Float32 fields/UInt32 indices versus 6B coordinate+4B normal+4B colour per cell; excludes opacity, PBR, hierarchy, mips, memberships in 14B model; no native GPU layout or global minimum",
    }
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "representation": args.representation,
        "input_receipt": input_receipt,
        "independent_controls": validation,
        "source_sha256": {
            name: hashlib.sha256(
                (Path(__file__).parent / name).read_bytes()
            ).hexdigest()
            for name in (
                "surface_cells.py",
                "measure_surface_cells.py",
                "surface_cell_oracle.py",
                "full_view_reference.py",
            )
        },
        "pitch_m": args.pitch,
        "grid_frame": "unchanged original parent-relative ECEF axes; not camera dependent, not optimised",
        "cell_diagonal_m": float(np.sqrt(3) * args.pitch),
        "work": work,
        "cost": cost,
        "storage": comparison,
        "raw_archive_bytes": (args.output / "cells.npz").stat().st_size,
        "peak_rss_bytes_linux": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "complete tested Float64 triangle/cell occupancy and explicit original fallback; allowance not interval-certified; no rendered surface/material/visibility/Tokyo/device admission",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()

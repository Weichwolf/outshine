"""Qualify the original pool contact across its complete pinned terrain window."""

import argparse
import hashlib
import json
import os
import resource
import sys
import time
from pathlib import Path

import numpy as np
from pool_body_experiment import FIXTURE, construct, original
from pool_contact_experiment import measured
from pool_native_neighborhood import load_neighborhood
from pool_region_contact import reconcile_window
from terrain_grid_lod import triangles


def witness(neighborhood, data):
    zoom, x0, _, y0, _ = neighborhood.manifest["window_zoom_x0_x1_y0_y1"]
    cell = data["terrain"]
    if cell["z"] != zoom:
        raise ValueError("original witness must belong to the supplied native window")
    row = (cell["y"] - y0) * 32 + int(cell["v"] * 32)
    col = (cell["x"] - x0) * 32 + int(cell["u"] * 32)
    side = neighborhood.heights.shape[0]
    indices = np.array(
        [
            row * side + col,
            row * side + col + 1,
            (row + 1) * side + col,
            (row + 1) * side + col + 1,
        ]
    )
    expected = np.array(cell["cellCornersM"], dtype="<f4").ravel()
    if neighborhood.heights.ravel()[indices].tobytes() != expected.tobytes():
        raise ValueError("original four witness height words changed")
    return indices, row, col


def timed(body, neighborhood, reuse):
    def progress(phase, index, total):
        print(f"{phase} {index}/{total}", flush=True)

    wall, cpu = time.perf_counter(), time.process_time()
    result = reconcile_window(
        body,
        neighborhood.vertices,
        len(neighborhood.heights),
        reuse_unchanged=reuse,
        progress=progress,
    )
    return result, {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare-rebuilds", action="store_true")
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    neighborhood = load_neighborhood()
    data, metadata, _, water, _ = original(FIXTURE)
    for key in ("asset_key", "stored_sha256", "native_sha256"):
        if data["source"][key] != neighborhood.manifest[key]:
            raise ValueError("original body and neighbor package identities differ")
    body = construct(data, metadata, water)
    witness_indices, row, column = witness(neighborhood, data)
    load = os.getloadavg()
    result, cost = timed(body, neighborhood, True)
    vertices, faces, source_indices, report = result
    if report["admission"] != "reference_window_closed":
        raise ValueError(
            "original finite window failed full body/outer-boundary qualification"
        )
    report.update(
        {
            "source": neighborhood.report,
            "cost": cost,
            "load_before": load,
            "body_input_sha256": hashlib.sha256(
                (FIXTURE / "input.json").read_bytes()
            ).hexdigest(),
            "output_numeric_bytes": vertices.nbytes
            + faces.nbytes
            + source_indices.nbytes,
        }
    )
    if args.compare_rebuilds:
        baseline, baseline_cost = timed(body, neighborhood, False)
        if not all(
            a.tobytes() == b.tobytes() for a, b in zip(result[:3], baseline[:3])
        ):
            raise ValueError("selective rebuild changed the reference geometry")
        report["unconditional_rebuild_comparison"] = {
            "same_output_words": True,
            "cost": baseline_cost,
            "work": baseline[3]["work"],
            "rebuilt_patches": baseline[3]["rebuilt_patches"],
        }
        del baseline
    side = len(neighborhood.heights)
    row, column = row // 4 * 4, column // 4 * 4
    probe = neighborhood.vertices.reshape(side, side, 3)[
        row : row + 5, column : column + 5
    ].reshape(-1, 3)
    report["representative_original_patch_cost"] = measured(
        body, probe, triangles(5, 1), 7
    )
    report["allocation_scope"] = (
        "Representative original 32-face patch only; whole-window allocation events and external GEOS heap are not measured. Work/returned array bytes and whole-process RSS are separate."
    )
    report["rusage_peak_before_plot_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    if sys.platform == "linux":
        report["linux_vm_hwm_before_plot_bytes"] = next(
            int(s.split()[1]) * 1024
            for s in Path("/proc/self/status").read_text().splitlines()
            if s.startswith("VmHWM:")
        )
    packed = vertices.astype(np.float32)
    cross = np.cross(
        packed[faces[:, 1]] - packed[faces[:, 0]],
        packed[faces[:, 2]] - packed[faces[:, 0]],
    )[:, 2]
    report["f32_enu_cast_diagnostic"] = {
        "coincident_vertices": len(vertices) - len(np.unique(packed, axis=0)),
        "nonpositive_faces": int((cross <= 0).sum()),
        "scope": "Direct ENU cast, not the native page/frame transform; no native packing admission.",
    }
    np.savez(
        args.output / "candidate.npz",
        vertices=vertices,
        faces=faces,
        original_vertex_indices=source_indices,
        original_vertices=neighborhood.vertices,
        original_faces=neighborhood.faces,
        witness_indices=witness_indices,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.render:
        from pool_neighborhood_view import display

        display(body, neighborhood, vertices, faces, witness_indices, args.output)


if __name__ == "__main__":
    main()

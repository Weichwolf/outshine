"""Exact native Sliver/QP counterexample and explicit one-edge intervention (#935)."""

import argparse
import hashlib
import json
import os
import platform
import time
import tracemalloc
from pathlib import Path

import clarabel
import numpy as np
import shapely
from compare_native_water_frame import footprints, original_case
from compare_physical_river import audit
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import normal_caps, tangent_directions
from river_membership import face_membership
from water_sliver_edge import contract_edge
from water_sliver_replay import (
    geometry_report,
    load_fixture,
    original_mesh,
    physical_report,
    replay,
)


def source_check(arrays, contraction, manifest):
    data, metadata, legacy = original_case(manifest)
    _, polygons = footprints(
        data,
        metadata["original_selection"]["selected_surface_indices"],
        np.asarray(legacy["frame"]["origin_lon_lat"]),
    )
    old = face_membership(arrays["target_enu_m"], arrays["triangles"], polygons)
    new = face_membership(
        contraction.mesh.vertices, contraction.mesh.triangles, polygons
    )
    assert not old.status.any() and not new.status.any()
    changed = []
    for j, i in enumerate(contraction.faces):
        if not np.array_equal(
            old.members[old.offsets[i] : old.offsets[i + 1]],
            new.members[new.offsets[j] : new.offsets[j + 1]],
        ):
            changed.append(int(i))
    return {
        "reconstructed_old_members": len(old.members),
        "reconstructed_new_members": len(new.members),
        "changed_surviving_face_memberships": changed,
        "original_source_holes": sum(len(p.interiors) for p in polygons),
        "sources_retained": np.unique(new.members).tolist(),
        "original_source_indices": arrays["source_original_indices"].tolist(),
        "old_work": old.work,
        "new_work": new.work,
        "scope": "Same-host reconstructed source rings; cross-host low-bit overlaps differ from captured CSR. No replacement native source oracle.",
    }


def run(fixture, output, manifest):
    arrays, case = load_fixture(fixture)
    mesh = original_mesh(arrays)
    reports = {}
    for name, scaled in (("original", False), ("row-scaled", True)):
        solution, reports[name] = replay(fixture, scaled=scaled)
        if scaled:
            physical = physical_report(arrays, solution)
            np.savez_compressed(
                output / "row-scaled.npz",
                vertices=physical.pop("vertices"),
                solution=solution,
            )
            reports[name].update(physical)
    began = time.perf_counter()
    contraction = contract_edge(
        mesh, arrays["rows"], arrays["primary_rows"], [713, 714], maximum_shift_m=1e-12
    )
    preparation = time.perf_counter() - began
    tracemalloc.start()
    traced = contract_edge(
        mesh, arrays["rows"], arrays["primary_rows"], [713, 714], maximum_shift_m=1e-12
    )
    peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    np.testing.assert_array_equal(contraction.mesh.vertices, traced.mesh.vertices)
    fit = fit_physical_surface(
        contraction.mesh,
        arrays["anchor_lon_lat"],
        contraction.rows,
        up_bounds_m=arrays["up_bounds_m"],
        primary_rows=contraction.primary,
        seconds=10,
        iterations=200,
    )
    assert fit.mesh is not None and fit.report["finite_qp_optimal"]
    caps = normal_caps(
        arrays["anchor_lon_lat"],
        contraction.mesh.vertices[:, :2],
        contraction.mesh.triangles,
        arrays["up_bounds_m"],
    )
    sampled, _ = audit(
        fit.mesh,
        arrays["anchor_lon_lat"],
        contraction.rows,
        arrays["up_bounds_m"],
        tangent_directions(
            caps["up"][contraction.rows[:, 0].astype(int)], contraction.rows[:, 1:4]
        ),
    )
    assert sampled["sampled_physical_grade_violation"] <= 1e-9
    geometry, offsets, members = geometry_report(mesh, contraction, arrays)
    sources = source_check(arrays, contraction, manifest)
    assert (
        geometry["all_original_source_ids_retained"]
        and geometry["largest_island_ring_identical"]
    )
    assert not sources["changed_surviving_face_memberships"]
    np.savez_compressed(
        output / "contracted.npz",
        vertices=contraction.mesh.vertices,
        triangles=contraction.mesh.triangles,
        rows=contraction.rows,
        primary_rows=contraction.primary,
        original_vertex_indices=contraction.vertices,
        original_face_indices=contraction.faces,
        original_constraint_indices=contraction.row_indices,
        source_offsets=offsets,
        source_members=members,
        source_original_indices=arrays["source_original_indices"],
        fitted_vertices=fit.mesh.vertices,
    )
    report = {
        "scope": "Explicit 714-to-713 causal intervention on #933; no automatic weld or native admission.",
        "fixture_source_commit": case["source_commit"],
        "fixture_files": case["files"],
        "replays": reports,
        "contraction": contraction.report,
        "preparation_wall_seconds": preparation,
        "preparation_python_traced_peak_bytes": peak,
        "allocation_scope": "Python/NumPy tracing; excludes solver/GEOS native allocations and complete process peak.",
        "fit": dict(fit.report),
        "independent_sampled_grades": sampled,
        "geometry": geometry,
        "source_check": sources,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "clarabel": clarabel.__version__,
        "geos": shapely.geos_version_string,
        "load_average": os.getloadavg(),
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("water_sliver_edge.py"),
                Path(__file__).with_name("water_sliver_replay.py"),
            )
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).parent / "fixtures/native-geos-3141",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    result = run(args.fixture, args.output, args.manifest)
    print(
        json.dumps(
            {
                "original_status": result["replays"]["original"]["status"],
                "scaled_violation": result["replays"]["row-scaled"][
                    "whole_face_violation"
                ],
                "contracted_valid": result["fit"]["valid"],
                "topology_changes": [
                    result["geometry"]["holes_before"],
                    result["geometry"]["holes_after"],
                ],
            }
        )
    )

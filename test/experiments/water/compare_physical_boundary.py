"""Bounded cross-frame water cuts, with original river and adverse controls (#941)."""

import argparse
import json
import platform
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_native_water_frame import footprints, original_case
from compare_physical_river import audit, inputs
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import (
    model_height_targets,
    normal_caps,
    tangent_directions,
)
from outshine_ref.water.geodetic import local_points
from physical_boundary import (
    boundary_error,
    canonical_cut,
    child_rows,
    cut_values,
    from_world,
    local_piece,
    rotation,
    signed_twice_area,
    to_world,
)
from physical_boundary_audit import coverage, inherited_envelope, store_piece


def measured(function):
    tracemalloc.start()
    start = time.perf_counter()
    try:
        result = function()
    finally:
        seconds = time.perf_counter() - start
        peak = tracemalloc.get_traced_memory()[1]
        live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
        tracemalloc.stop()
    return result, {
        "wall_seconds": seconds,
        "python_traced_peak_bytes": peak,
        "traced_retained_allocation_blocks": live,
    }


def zero_height_xy_control(name, geographic, heights, anchors):
    positions = []
    for anchor in anchors:
        zero = local_points(anchor, geographic, 0)
        up = model_height_targets(anchor, zero[:, :2], heights)
        positions.append(to_world(anchor, np.c_[zero[:, :2], up]))
    return {
        "name": name,
        "points": len(geographic),
        "anchors_lon_lat": np.asarray(anchors).tolist(),
        "zero_height_xy_pair_gap_max_m": float(
            np.linalg.norm(positions[0] - positions[1], axis=1).max()
        ),
    }


def synthetic(anchor, model_height):
    xy = np.array([[x, y] for y in (-30, 0, 30) for x in (-80, 0, 80.0)])
    triangles = []
    for y in range(2):
        for x in range(2):
            a = 3 * y + x
            triangles.extend([[a, a + 1, a + 4], [a, a + 4, a + 3]])
    mesh = Mesh(
        np.c_[xy, model_height_targets(anchor, xy, model_height - 0.002 * xy[:, 0])],
        np.array(triangles, dtype=np.uint32),
    )
    faces = np.arange(len(triangles))
    rows = np.r_[
        np.c_[faces, np.tile([1, 0, 0, -0.01, 0], (len(faces), 1))],
        np.c_[faces, np.tile([0, 1, 0, -0.005, 0.005], (len(faces), 1))],
    ]
    return (
        mesh,
        np.asarray(anchor),
        rows,
        [mesh.vertices[:, 2].min() - 20, mesh.vertices[:, 2].max() + 20],
    )


def fit(mesh, anchor, rows, box, primary, fixed=None):
    result, cost = measured(
        lambda: fit_physical_surface(
            mesh,
            anchor,
            rows,
            up_bounds_m=box,
            primary_rows=primary,
            fixed=fixed,
            seconds=20,
            iterations=200,
        )
    )
    report = dict(result.report, cost=cost)
    if result.mesh is not None:
        if not report.get("finite_qp_optimal", False):
            raise ValueError("finite optimality certificate required")
        caps = normal_caps(
            anchor, result.mesh.vertices[:, :2], result.mesh.triangles, box
        )
        tangent = tangent_directions(caps["up"][rows[:, 0].astype(int)], rows[:, 1:4])
        sampled, _ = audit(result.mesh, anchor, rows, box, tangent)
        report["independent_physical_audit"] = sampled
    return result.mesh, report


def crease(meshes, anchors, edges, parents):
    normals = []
    for mesh, anchor, seam, parent in zip(meshes, anchors, edges, parents, strict=True):
        points = mesh.vertices[mesh.triangles]
        normal = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        normal = normal @ rotation(anchor, [0, 0])
        lookup = {}
        for face, triangle in enumerate(mesh.triangles):
            for a, b in ((0, 1), (1, 2), (2, 0)):
                lookup[tuple(sorted((int(triangle[a]), int(triangle[b]))))] = face
        selected = np.array([lookup[tuple(sorted(e))] for e in seam])
        normals.append(normal[selected])
        if len(normals) == 1:
            canonical_parents = parent[selected]
        elif not np.array_equal(canonical_parents, parent[selected]):
            raise ValueError("same original facet across each new cut required")
    # atan2 resolves near-parallel normals without acos's floor at ~1e-6 deg.
    angles = np.rad2deg(
        np.arctan2(
            np.linalg.norm(np.cross(*normals), axis=1), (normals[0] * normals[1]).sum(1)
        )
    )
    return {
        "maximum_new_dihedral_degrees": float(angles.max()),
        "per_segment_degrees": angles.tolist(),
    }


def compare_case(
    name,
    mesh,
    anchor,
    rows,
    box,
    anchors,
    output,
    *,
    plane_x=None,
    polygons=None,
    source_ids=None,
):
    output.mkdir(parents=True, exist_ok=True)
    nf = len(mesh.triangles)
    primary = np.c_[np.arange(nf), nf + np.arange(nf)]
    solved, base_report = fit(mesh, anchor, rows, box, primary)
    report = {
        "name": name,
        "body_anchor_lon_lat": anchor.tolist(),
        "regional_anchors_lon_lat": np.asarray(anchors).tolist(),
        "source_vertices": len(mesh.vertices),
        "source_faces": nf,
        "canonical_fit": base_report,
        "methods": {},
    }
    if solved is None:
        return report
    if plane_x is None:
        plane_x = float(np.median(mesh.vertices[:, 0]))
    cut, cut_cost = measured(lambda: canonical_cut(solved, plane_x))
    raw = cut_values(cut, mesh.vertices)
    report["cut"] = dict(
        cut_cost,
        segments=len(cut.seam),
        boundary_vertices=len(np.unique(cut.seam)),
        added_vertices=len(cut.crossing_edges),
        output_faces=[len(f) for f in cut.faces],
        plane_x_m=cut.plane_x,
        input_triangles_visited=nf,
        halfspace_edge_tests=6 * nf,
        retained_cut_array_bytes=sum(
            a.nbytes
            for a in (
                cut.vertices,
                *cut.faces,
                *cut.parents,
                cut.seam,
                cut.crossing_edges,
                cut.crossing_t,
            )
        ),
    )
    accumulated = np.zeros(nf)
    for faces, parents in zip(cut.faces, cut.parents, strict=True):
        np.add.at(accumulated, parents, signed_twice_area(cut.vertices, faces) / 2)
    original_area = signed_twice_area(mesh.vertices, mesh.triangles) / 2
    report["cut"]["maximum_parent_area_difference_m2"] = float(
        abs(accumulated - original_area).max()
    )
    report["cut"]["total_signed_area_difference_m2"] = float(
        (accumulated - original_area).sum()
    )
    stored = {
        "canonical_vertices": cut.vertices,
        "original_vertices": mesh.vertices,
        "original_triangles": mesh.triangles,
        "canonical_seam": cut.seam,
    }
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, box)
    tangents = tangent_directions(caps["up"][rows[:, 0].astype(int)], rows[:, 1:4])
    for method in ("shared_body_cut", "separate_fits"):
        method_report, arrays = regional_method(
            method,
            cut,
            raw,
            anchor,
            anchors,
            rows,
            tangents,
            caps,
            output,
            name,
            polygons,
            source_ids,
            float(original_area.sum()),
        )
        report["methods"][method] = method_report
        stored.update(arrays)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / f"{name}.npz", **stored)
    (output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def regional_method(
    method,
    cut,
    raw,
    anchor,
    anchors,
    rows,
    tangents,
    parent_caps,
    output,
    name,
    polygons,
    source_ids,
    original_area,
):
    meshes, edges, global_ids, stored, report = [], [], [], {}, {"regions": []}
    nf = int(max(p.max() for p in cut.parents)) + 1
    for side, regional_anchor in enumerate(anchors):
        local, ids, seam = local_piece(
            cut,
            side,
            anchor,
            regional_anchor,
            vertices=raw if method == "separate_fits" else None,
        )
        local_rows, primary, gathered = child_rows(
            rows, cut.parents[side], anchor, regional_anchor, nf
        )
        original_tangent = tangents[gathered] @ rotation(anchor, regional_anchor)
        if method == "separate_fits":
            boundary = np.unique(seam)
            canonical_local = from_world(
                regional_anchor, to_world(anchor, cut.vertices[ids])
            )
            vertices = local.vertices.copy()
            vertices[:, 2] += (-0.25, 0.25)[side]
            vertices[boundary, :2] = canonical_local[boundary, :2]
            if np.any(signed_twice_area(vertices, local.triangles) <= 0):
                report["regions"].append({"valid": False, "reason": "boundary_xy_fold"})
                break
            local = Mesh(vertices, local.triangles)
            fixed = {int(i): float(canonical_local[i, 2]) for i in boundary}
            up_box = [
                float(min(vertices[:, 2].min(), min(fixed.values())) - 20),
                float(max(vertices[:, 2].max(), max(fixed.values())) + 20),
            ]
            local, fit_report = fit(
                local, regional_anchor, local_rows, up_box, primary, fixed
            )
            report["regions"].append(fit_report)
            if local is None:
                break
        else:
            returned = from_world(anchor, to_world(regional_anchor, local.vertices))
            up_box = [
                float(local.vertices[:, 2].min() - 20),
                float(local.vertices[:, 2].max() + 20),
            ]
            report["regions"].append(
                {
                    "valid": True,
                    "roundtrip_vertex_max_m": float(
                        np.linalg.norm(returned - cut.vertices[ids], axis=1).max()
                    ),
                }
            )
        independent, _ = audit(
            local, regional_anchor, local_rows, up_box, original_tangent
        )
        report["regions"][-1]["independent_original_direction_audit"] = independent
        actual_body = from_world(anchor, to_world(regional_anchor, local.vertices))
        report["regions"][-1]["inherited_whole_face"] = inherited_envelope(
            actual_body,
            local.triangles,
            cut.parents[side],
            parent_caps,
            local_rows,
            tangents[gathered],
        )
        meshes.append(local)
        edges.append(seam)
        global_ids.append(ids)
        for key, value in {
            "vertices": local.vertices,
            "triangles": local.triangles,
            "global_ids": ids,
            "parent_faces": cut.parents[side],
            "seam": seam,
        }.items():
            stored[f"{method}_{side}_{key}"] = value
    report["valid"] = len(meshes) == 2
    if not report["valid"]:
        return report, stored
    report["boundary"] = boundary_error(
        meshes[0], anchors[0], edges[0], meshes[1], anchors[1], edges[1]
    )
    rounded = [
        Mesh(m.vertices.astype(np.float32).astype(float), m.triangles) for m in meshes
    ]
    report["float32_local_position_control"] = boundary_error(
        rounded[0], anchors[0], edges[0], rounded[1], anchors[1], edges[1]
    )
    report["crease"] = crease(meshes, anchors, edges, cut.parents)
    memberships = [None, None]
    if polygons is not None:
        (checked, memberships), cost = measured(
            lambda: coverage(meshes, anchors, anchor, polygons, original_area)
        )
        report["coverage"] = dict(checked, cost=cost)
    boundary_ids = np.unique(cut.seam)
    boundary_edges = np.searchsorted(boundary_ids, cut.seam).astype(np.uint32)
    canonical = to_world(anchor, cut.vertices[boundary_ids])
    report["stored_products"] = []
    for side in range(2):
        product = output / f"{name}-{method}-{side}.npz"
        saved = store_piece(
            product,
            meshes[side],
            anchors[side],
            global_ids[side],
            edges[side],
            cut.parents[side],
            canonical,
            boundary_ids,
            boundary_edges,
            memberships[side],
            source_ids,
        )
        report["stored_products"].append(dict(saved, filename=product.name))
    report["minimum_final_Mesh_owned_copy_bytes"] = int(
        sum(m.vertices.nbytes + m.triangles.nbytes for m in meshes)
    )
    return report, stored


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--original", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = []
    controls = [
        zero_height_xy_control(
            "antimeridian",
            np.array([[179.999, 0], [179.9995, 0.0001]]),
            np.array([500.0, 500.0]),
            [[179.99, 0], [-179.99, 0]],
        ),
        zero_height_xy_control(
            "high_latitude",
            np.array([[20.0, 89.5], [20.01, 89.5001]]),
            np.array([1000.0, 1000.0]),
            [[10, 89.5], [30, 89.5]],
        ),
        zero_height_xy_control(
            "negative_height",
            np.array([[9.6, 47.23], [9.61, 47.231]]),
            np.array([-50.0, -50.0]),
            [[9.588371, 47.232575], [9.608371, 47.232575]],
        ),
    ]
    for name, anchor, height, anchors in (
        ("negative_height", [9.6, 47.23], -50, [[9.59, 47.23], [9.61, 47.23]]),
        ("antimeridian", [179.9995, 10], 500, [[179.99, 10], [-179.99, 10]]),
        ("high_latitude", [20, 89.5], 1000, [[10, 89.5], [30, 89.5]]),
    ):
        cases.append(
            compare_case(
                name, *synthetic(anchor, height), anchors, args.output, plane_x=10
            )
        )
        print(
            name, {k: v["valid"] for k, v in cases[-1]["methods"].items()}, flush=True
        )
    if args.original:
        manifest_path = (
            Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json"
        )
        mesh, anchor, rows, box, _arrays, provenance = inputs(manifest_path)
        controls.append(
            zero_height_xy_control(
                "original_ill_course_model_priors",
                _arrays["course_original_lon_lat"].reshape(-1, 2),
                _arrays["course_original_levels_m"].reshape(-1).astype(float),
                [anchor + [-0.01, 0], anchor + [0.01, 0]],
            )
        )
        data, manifest, _legacy = original_case(manifest_path)
        source_ids = manifest["original_selection"]["selected_surface_indices"]
        _original, polygons = footprints(data, source_ids, anchor)
        original = compare_case(
            "original_ill",
            mesh,
            anchor,
            rows,
            box,
            [anchor + [-0.01, 0], anchor + [0.01, 0]],
            args.output,
            polygons=polygons,
            source_ids=np.asarray(source_ids),
        )
        original["original_input"] = provenance
        cases.append(original)
        print(
            "original_ill",
            {k: v["valid"] for k, v in original["methods"].items()},
            flush=True,
        )
    (args.output / "report.json").write_text(
        json.dumps(
            {
                "scope": "finite Float64 Python physical-edge comparison, not GPU/whole-world admission",
                "datum": "declared WGS84 ellipsoidal model; DEM priors are not observed levels",
                "environment": {
                    "platform": platform.platform(),
                    "numpy": np.__version__,
                },
                "cases": cases,
                "zero_height_xy_controls": controls,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()

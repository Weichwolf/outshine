"""Replay and independent geometry/provenance checks for the stored #933 case."""

import hashlib
import json
from types import SimpleNamespace

import clarabel
import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.water._fit import grade_envelopes
from outshine_ref.water._program import conic_certificate
from outshine_ref.water.frame import normal_caps
from scipy import sparse
from water_sliver_edge import exact_area


def load_fixture(path):
    case = json.loads((path / "case.json").read_text())
    for name, record in case["files"].items():
        data = (path / name).read_bytes()
        assert len(data) == record["bytes"]
        assert hashlib.sha256(data).hexdigest() == record["sha256"]
    with np.load(path / "physical-inputs.npz", allow_pickle=False) as data:
        arrays = {key: data[key] for key in data.files}
    return arrays, case


def replay(path, *, scaled=False):
    p = sparse.load_npz(path / "physical-hessian.npz")
    a = sparse.load_npz(path / "physical-constraints.npz")
    with np.load(path / "physical-vectors.npz", allow_pickle=False) as v:
        q, b = v["linear"], v["bound"]
    scale = np.ones(len(b))
    if scaled:
        scale /= np.maximum(1, abs(a).max(axis=1).toarray().ravel())
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.max_threads = 1
    settings.max_iter = 200
    settings.time_limit = 10
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-11
    result = clarabel.DefaultSolver(
        p,
        q,
        (sparse.diags(scale) @ a).tocsc(),
        b * scale,
        [clarabel.NonnegativeConeT(len(b))],
        settings,
    ).solve()
    report = {
        "status": str(result.status),
        "iterations": result.iterations,
        "maximum_abs_A": float(abs(a.data).max()),
        "solver_primal_residual": result.r_prim,
        "solver_dual_residual": result.r_dual,
        "scaled_rows": int(np.count_nonzero(scale != 1)),
        "original_primal_violation": float(np.maximum(a @ result.x - b, 0).max()),
    }
    if str(result.status) in ("Solved", "AlmostSolved"):
        full = p + p.T - sparse.diags(p.diagonal())
        report["original_certificate"] = conic_certificate(
            SimpleNamespace(x=result.x, z=np.asarray(result.z) * scale),
            full,
            q,
            a,
            b,
            len(b),
            0,
        )
    return np.asarray(result.x), report


def physical_report(arrays, solution):
    vertices = arrays["target_enu_m"].copy()
    vertices[:, 2] = solution + np.median(vertices[:, 2])
    triangle = vertices[arrays["triangles"]]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    normal /= normal[:, 2, None]
    caps = normal_caps(
        arrays["anchor_lon_lat"],
        vertices[:, :2],
        arrays["triangles"],
        arrays["up_bounds_m"],
    )
    rows = arrays["rows"]
    envelope = grade_envelopes(
        normal, caps, rows[:, 0].astype(int), arrays["row_tangent"]
    )
    return {
        "whole_face_violation": max(
            float((rows[:, 4] - envelope[:, 0]).max()),
            float((envelope[:, 1] - rows[:, 5]).max()),
            0.0,
        ),
        "vertices": vertices,
    }


def geometry_report(original, contraction, arrays):
    old = shapely.union_all(shapely.polygons(original.vertices[original.triangles, :2]))
    new = shapely.union_all(
        shapely.polygons(contraction.mesh.vertices[contraction.mesh.triangles, :2])
    )
    assert old.geom_type == new.geom_type == "Polygon" and old.is_valid and new.is_valid
    old_holes = sorted(old.interiors, key=lambda r: abs(exact_area(r.coords)))
    new_holes = sorted(new.interiors, key=lambda r: abs(exact_area(r.coords)))
    members, offsets = [], [0]
    removed_members = []
    for i in contraction.removed_faces:
        removed_members.append(
            arrays["source_members"][
                slice(*arrays["source_offsets"][i : i + 2])
            ].tolist()
        )
    for i in contraction.faces:
        members.extend(
            arrays["source_members"][slice(*arrays["source_offsets"][i : i + 2])]
        )
        offsets.append(len(members))
    report = {
        "components_before": 1,
        "components_after": 1,
        "holes_before": len(old_holes),
        "holes_after": len(new_holes),
        "hole_areas_before_exact_m2": [
            float(abs(exact_area(r.coords))) for r in old_holes
        ],
        "hole_areas_after_exact_m2": [
            float(abs(exact_area(r.coords))) for r in new_holes
        ],
        "largest_island_ring_identical": old_holes[-1].equals_exact(new_holes[-1], 0),
        "outer_ring_hausdorff_m": old.exterior.hausdorff_distance(new.exterior),
        "geos_symmetric_difference_m2": old.symmetric_difference(new).area,
        "removed_face_source_members": removed_members,
        "captured_source_members_before": len(arrays["source_members"]),
        "captured_source_members_after": len(members),
        "all_original_source_ids_retained": np.array_equal(
            np.unique(members), np.unique(arrays["source_members"])
        ),
        "source_scope": "Inherited captured face provenance; not a new positive-overlap proof at source boundaries.",
    }
    return (
        report,
        np.asarray(offsets, dtype=np.uint32),
        np.asarray(members, dtype=np.uint32),
    )


def original_mesh(arrays):
    return Mesh(arrays["target_enu_m"], arrays["triangles"])

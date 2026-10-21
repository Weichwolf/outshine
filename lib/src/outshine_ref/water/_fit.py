"""Conservative physical whole-facet water reference, not a native cooker (#760)."""

import time

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightFit
from outshine_ref.water._contacts import point_contact_errors, prepare_contacts
from outshine_ref.water._program import solve_program
from outshine_ref.water.frame import normal_caps, normal_norm_bounds, tangent_directions


def grade_envelopes(normals, caps, faces, tangent):
    """Enclose physical geodetic grades in projected-constant directions.

    Horizontal metres are on the parallel surface at the query's own height.
    Caller supplies unit reference tangents orthogonal to each cap's centre Up.
    """
    normal = normals[faces]
    size = np.linalg.norm(normal, axis=1)
    delta = caps["delta"][faces]
    vertical = (normal * caps["up"][faces]).sum(axis=1)
    lower_denominator = (vertical - size * delta) * np.sqrt(1 - delta**2)
    upper_denominator = vertical + size * delta
    if np.any(lower_denominator <= 0):
        raise ValueError("physical facets require a uniformly positive vertical ray")
    centre = -(normal * tangent).sum(axis=1)
    low, high = centre - size * delta, centre + size * delta
    return np.c_[
        np.minimum(low / lower_denominator, low / upper_denominator),
        np.maximum(high / lower_denominator, high / upper_denominator),
    ]


def fit_physical_surface(
    mesh,
    anchor,
    rows,
    *,
    up_bounds_m,
    fixed=None,
    contact_points=None,
    seconds=5.0,
    iterations=200,
    normal_model="whole-prism",
    primary_rows=None,
    solver="clarabel",
    vertex_up_bounds_m=None,
):
    """Rows are (face, ENU dx,dy,dz,lower<=0,upper>=0), not old planar rows.

    Fixed contacts are explicit ENU-Up values at existing shared vertices. A
    rejected solve returns no geometry, never an unvalidated/relaxed water field.
    contact_points add hard physical ENU positions inside/on the same facets.
    Two explicit primary row indices per face select the derived-bound linear QP;
    otherwise the original norm-SOCP reference remains the default.
    Optional Nx2 vertex_up_bounds_m restricts each vertex inside the global
    normal-envelope prism; these are ENU-Up coordinates, not geodetic heights.
    """
    start = time.perf_counter()
    report = {"valid": False, "scope": "finite conservative physical SOCP reference"}

    def absent(reason):
        return HeightFit(
            None,
            dict(report, reason=reason, total_ms=1000 * (time.perf_counter() - start)),
        )

    if not isinstance(mesh, Mesh):
        raise TypeError("owned shared Mesh required")
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    rows = np.asarray(rows, float)
    if not np.isfinite(seconds) or seconds < 0 or type(iterations) is not int:
        raise ValueError(
            "finite nonnegative time and integer iteration budget required"
        )
    if nv > 8192 or nf > 16384 or len(rows) > 65536:
        return absent("input_budget_exhausted")
    if seconds == 0 or iterations <= 0:
        return absent("solver_budget_exhausted")
    triangle = mesh.vertices[mesh.triangles, :2]
    edge = triangle[:, 1:] - triangle[:, :1]
    determinant = edge[:, 0, 0] * edge[:, 1, 1] - edge[:, 0, 1] * edge[:, 1, 0]
    if np.any(determinant == 0) or not np.isfinite(determinant).all():
        return absent("degenerate_surface")
    if (
        not nf
        or rows.ndim != 2
        or rows.shape[1] != 6
        or not np.isfinite(rows).all()
        or np.any(rows[:, 0] != np.floor(rows[:, 0]))
        or np.any(rows[:, 0] < 0)
        or np.any(rows[:, 0] >= nf)
        or np.any(rows[:, 4] > 0)
        or np.any(rows[:, 5] < 0)
    ):
        raise ValueError(
            "existing faces, finite 3D directions and intervals containing zero required"
        )
    pins = dict(fixed or {})
    if any(
        type(i) is not int or not 0 <= i < nv or not np.isfinite(h)
        for i, h in pins.items()
    ):
        raise ValueError("finite ENU-Up contacts at existing vertices required")
    contacts, contact_report = prepare_contacts(mesh, contact_points)
    report.update(contact_report)
    if contacts is None:
        return absent(report["reason"])
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, up_bounds_m)
    vertex_bounds = None
    if vertex_up_bounds_m is not None:
        if not isinstance(
            vertex_up_bounds_m, np.ndarray
        ) or vertex_up_bounds_m.shape != (nv, 2):
            raise ValueError("bounded NumPy Nx2 vertex Up bounds required")
        vertex_bounds = np.array(vertex_up_bounds_m, dtype=float, copy=True)
        if (
            vertex_bounds.shape != (nv, 2)
            or not np.isfinite(vertex_bounds).all()
            or np.any(vertex_bounds[:, 0] > vertex_bounds[:, 1])
            or np.any(vertex_bounds[:, 0] < caps["up_bounds_m"][0])
            or np.any(vertex_bounds[:, 1] > caps["up_bounds_m"][1])
        ):
            raise ValueError(
                "finite ordered vertex Up bounds inside the global prism required"
            )
        if any(
            not vertex_bounds[i, 0] <= h <= vertex_bounds[i, 1] for i, h in pins.items()
        ):
            return absent("fixed_contact_outside_vertex_bounds")
    if np.any(
        (contacts.points[:, 2] < caps["up_bounds_m"][0])
        | (contacts.points[:, 2] > caps["up_bounds_m"][1])
    ):
        return absent("point_contact_height_outside_bounds")
    if normal_model not in ("whole-prism", "centre", "enu-control"):
        raise ValueError("unknown explicitly declared normal model")
    report["normal_model"] = normal_model
    report["scope"] = (
        "finite physical whole-prism envelope"
        if normal_model == "whole-prism"
        else "declared approximation control; no physical whole-face admission"
    )
    if normal_model != "whole-prism":
        caps["delta"] = np.zeros(nf)
    if normal_model == "enu-control":
        caps["up"] = np.tile([0.0, 0.0, 1.0], (nf, 1))
    faces = rows[:, 0].astype(int)
    tangent = tangent_directions(caps["up"][faces], rows[:, 1:4])
    bound = None
    if primary_rows is not None:
        if normal_model != "whole-prism":
            raise ValueError(
                "derived linear bounds require the physical whole-prism model"
            )
        bound = normal_norm_bounds(caps, faces, tangent, rows[:, 4:6], primary_rows)
    height, mass, solver_report = solve_program(
        mesh,
        caps,
        faces,
        tangent,
        rows,
        pins,
        seconds,
        iterations,
        start,
        normal_bound=bound,
        solver_name=solver,
        point_contacts=contacts,
        vertex_up_bounds_m=vertex_bounds,
    )
    report.update(solver_report)
    if height is None:
        return absent(report["reason"])
    if not np.isfinite(height).all():
        return absent("nonfinite_solution")
    vertices = np.c_[mesh.vertices[:, :2], height]
    edges = vertices[mesh.triangles][:, 1:] - vertices[mesh.triangles][:, :1]
    normal = np.cross(edges[:, 0], edges[:, 1])
    normal /= normal[:, 2, None]
    if bound is not None:
        report["derived_normal_bound_excess"] = float(
            (np.linalg.norm(normal, axis=1) - bound).max()
        )
    envelope = grade_envelopes(normal, caps, faces, tangent)
    violation = max(
        float((rows[:, 4] - envelope[:, 0]).max(initial=0)),
        float((envelope[:, 1] - rows[:, 5]).max(initial=0)),
        0.0,
    )
    pin_error = max((abs(height[i] - h) for i, h in pins.items()), default=0.0)
    box_error = max(
        float(up_bounds_m[0] - height.min()), float(height.max() - up_bounds_m[1]), 0.0
    )
    report.update(
        whole_face_envelope_violation=violation,
        fixed_up_error_m=pin_error,
        height_box_violation_m=box_error,
    )
    result_mesh = Mesh(vertices, mesh.triangles)
    report.update(point_contact_errors(result_mesh, contacts))
    vertex_error = 0.0
    if vertex_bounds is not None:
        vertex_error = max(
            float((vertex_bounds[:, 0] - height).max(initial=0)),
            float((height - vertex_bounds[:, 1]).max(initial=0)),
            0.0,
        )
        report.update(
            vertex_height_box_violation_m=vertex_error,
            vertex_bound_array_bytes=vertex_bounds.nbytes,
        )
    if (
        max(
            violation,
            pin_error,
            box_error,
            report.get("derived_normal_bound_excess", 0),
            report["fixed_point_up_error_m"],
            vertex_error,
        )
        > 1e-9
    ):
        return absent("physical_envelope_check_failed")
    residual = height - mesh.vertices[:, 2]
    report.update(
        valid=True,
        physical_whole_face_admitted=normal_model == "whole-prism",
        reason="",
        integrated_enu_up_rms_m=float(np.sqrt(residual @ mass @ residual)),
        maximum_enu_up_target_error_m=float(abs(residual).max()),
        total_ms=1000 * (time.perf_counter() - start),
    )
    return HeightFit(result_mesh, report)

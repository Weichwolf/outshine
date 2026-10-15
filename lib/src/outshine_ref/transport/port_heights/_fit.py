"""Fit original DEM observations and exact sliced soil constraints; audit roads separately."""

import time

import numpy as np
from scipy import sparse

from ._budget import PortLimit
from ._system import discrete_matrix, fit_planes, vertex_matrices


def fit_sliced(
    model, captured, mesh, certificates, *, seconds=8, method="cone", budget=None
):
    began = time.monotonic()
    h, equal, _, _parts = vertex_matrices(
        model, mesh["xy"], mesh["faces"], mesh["parts"], budget=budget
    )
    certificate, original_note = certificates
    certificate = sparse.hstack(
        [
            certificate,
            sparse.csc_matrix(
                (certificate.shape[0], h.shape[1] - certificate.shape[1])
            ),
        ]
    )
    soil = mesh["parts"] < 0
    cones = certificate.shape[0] // 2 + int(soil.sum())
    if budget and (
        cones > budget.max_cones
        or 12 * cones + 24 * len(mesh["xy"]) > budget.max_sparse_nonzeros
    ):
        raise PortLimit("port_constraint_or_sparse_budget")
    gradient = sparse.vstack(
        [
            certificate,
            discrete_matrix(mesh["xy"], mesh["faces"][soil], mesh["frames"][soil], h),
        ]
    ).tocsc()
    if (
        budget
        and (gradient.nnz + h.nnz + equal.nnz) * (16 if method == "lp" else 1)
        > budget.max_sparse_nonzeros
    ):
        raise PortLimit("port_sparse_work_budget")
    # New cut points interpolate a prior; only the original captured DEM observations
    # belong in the fitting objective. Finite road gradients remain a mandatory
    # independent acceptance check; parameter refinement fixes their approximation.
    objective = h[mesh["original_vertex_map"]]
    system = {
        **original_note,
        "unknowns": h.shape[1],
        "objective_observations": len(captured["ground_targets"]),
        "soil_face_cones": int(soil.sum()),
        "total_cones": gradient.shape[0] // 2,
        "point_equalities": equal.shape[0],
        "assembly_ms": (time.monotonic() - began) * 1000,
        "finite_road_constraints": "All faces independently checked after fitting; no unchecked acceptance.",
    }
    try:
        coefficients, report = fit_planes(
            objective,
            gradient,
            equal,
            captured["ground_targets"],
            seconds=seconds,
            method=method,
            max_iterations=budget.max_iterations if budget else 100,
        )
    except MemoryError:
        return (
            None,
            system,
            {
                "valid": False,
                "reason": "address_space_budget_exhausted",
                "method": method,
                "total_ms": (time.monotonic() - began) * 1000,
            },
        )
    if coefficients is None:
        return None, system, report
    heights = h @ coefficients + report["datum_m"]
    updated_seed = {
        "plane_coefficients": coefficients[: 3 * len(model.origins)],
        "vertices": np.c_[mesh["xy"], heights],
        "datum_m": report["datum_m"],
    }
    mesh["heights"] = heights
    return (updated_seed, coefficients), system, report

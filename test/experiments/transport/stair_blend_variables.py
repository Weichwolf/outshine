"""Equivalent displacement coordinates with fixed height variables eliminated."""

import clarabel
import numpy as np


def reduced(layout, model):
    mode = str(np.asarray(layout.get("variable_mode", "heights")).item())
    if mode == "heights":
        return model["P"], model["q"], model["A"], model["b"], model["cones"], None, {}
    if mode != "fixed_displacement":
        raise ValueError("known bounded terrain variable formulation required")
    count, total = len(layout["xy"]), len(model["q"])
    free = np.ones(total, bool)
    free[model["pins"]] = False
    ids = np.flatnonzero(free)
    fixed_delta = np.zeros(total)
    fixed_delta[model["pins"]] = model["pin_values"] - layout["targets"][model["pins"]]
    shift = np.r_[layout["targets"] - model["datum"], np.zeros(total - count)]
    shift[model["pins"]] = model["pin_values"] - model["datum"]
    linear = model["q"].copy()
    linear[:count] = 0
    linear += model["P"] @ fixed_delta
    keep = np.ones(len(model["b"]), bool)
    cones, first = [], 0
    for cone in model["cones"]:
        if isinstance(cone, clarabel.ZeroConeT):
            keep[first : first + cone.dim] = False
        else:
            cones.append(cone)
        first += cone.dim
    if first != len(keep):
        raise ValueError("terrain conic rows changed during variable elimination")
    matrix = model["A"][keep][:, ids].tocsc()
    bounds = (model["b"] - model["A"] @ shift)[keep]
    quadratic = model["P"][ids][:, ids].tocsc()
    note = {
        "mode": mode,
        "fixed_height_variables_eliminated": int((~free).sum()),
        "solver_variables": len(ids),
        "solver_rows": len(bounds),
        "solver_A_nonzeros": matrix.nnz,
        "solver_P_nonzeros": quadratic.nnz,
        "additional_retained_sparse_bytes": sum(
            m.data.nbytes + m.indices.nbytes + m.indptr.nbytes
            for m in (matrix, quadratic)
        ),
        "explicit_shift_delta_linear_bytes": shift.nbytes
        + fixed_delta.nbytes
        + linear.nbytes,
        "scope": "Same quadratic/cones; fixed Dirichlet heights substituted, target displacement zero-centred, no class/contact/budget relaxation. Reconstructed physical heights independently admitted.",
    }
    return quadratic, linear[ids], matrix, bounds, cones, (ids, fixed_delta), note


def restore(layout, model, answer, mapping):
    if mapping is None:
        return np.asarray(answer[: len(layout["xy"])]) + model["datum"]
    ids, fixed_delta = mapping
    values = fixed_delta.copy()
    values[ids] = answer
    return layout["targets"] + values[: len(layout["xy"])]

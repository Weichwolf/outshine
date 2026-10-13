"""Independent vein graph, tie, budget and scalar material reference checks."""

import numpy as np
from leaf_vein_field import vein_field
from leaf_venation import grow_veins, nearest
from scipy.optimize import minimize_scalar


def check_graph(blade, dense, indexed):
    assert np.array_equal(dense.parents, indexed.parents)
    assert np.allclose(dense.positions, indexed.positions, rtol=0, atol=1e-15)
    assert np.array_equal(dense.radii, indexed.radii)
    assert dense.stop_reason == indexed.stop_reason
    assert dense.remaining_sources == indexed.remaining_sources
    p = indexed.positions
    assert indexed.parents[0] == -1
    assert np.all(
        (indexed.parents[1:] >= 0) & (indexed.parents[1:] < np.arange(1, len(p)))
    )
    widths, _ = blade.width_profile(p[:, 1] / blade.length)
    assert np.all(np.abs(p[:, 0]) <= widths + 1e-15)
    assert np.isfinite(p).all() and np.all(indexed.radii > 0)
    assert np.all(indexed.radii[indexed.parents[1:]] >= indexed.radii[1:])
    return {
        "nodes": len(p),
        "connected_parent_first": True,
        "remaining_sources": indexed.remaining_sources,
        "stop_reason": indexed.stop_reason,
    }


def normalized_distance(u, point, a, b, r0, r1):
    p = a + u * (b - a)
    r = r0 + u * (r1 - r0)
    return sum((point[j] - p[j]) ** 2 for j in range(2)) / r**2


def scalar_field(plan, point):
    value = 0.0
    for i in range(1, len(plan.positions)):
        a, b = plan.positions[plan.parents[i]], plan.positions[i]
        r0, r1 = plan.radii[plan.parents[i]], plan.radii[i]

        args = (point, a, b, r0, r1)
        result = minimize_scalar(
            normalized_distance,
            args=args,
            bounds=(0, 1),
            method="bounded",
            options={"xatol": 1e-14},
        )
        assert result.success
        normalized = min(
            result.fun, normalized_distance(0, *args), normalized_distance(1, *args)
        )
        value = max(value, max(0.0, 1 - normalized / 9) ** 3)
    return value


def check_field(plan):
    rng = np.random.default_rng(123)
    points = np.vstack(
        (
            plan.positions[:: max(1, len(plan.positions) // 32)],
            rng.uniform((-0.04, -0.01), (0.04, 0.13), (64, 2)),
        )
    )
    dense, dense_work = vein_field(plan, points, "dense")
    indexed, indexed_work = vein_field(plan, points, "indexed")
    reference = np.array([scalar_field(plan, p) for p in points])
    error = float(np.max(np.abs(reference - indexed)))
    assert error < 1e-10 and np.allclose(dense, indexed, rtol=0, atol=1e-13)
    return {
        "independent_points": len(points),
        "max_scalar_field_error": error,
        "dense_evaluations": dense_work,
        "indexed_evaluations": indexed_work,
    }


def check_cases(blade):
    nodes = np.array([[-1.0, 0], [1.0, 0], [1.0, 0]])
    points = np.array([[0.0, 0], [1.0, 0]])
    for method in ("dense", "tree"):
        owners, _ = nearest(nodes, points, method)
        assert np.array_equal(owners, [0, 1])
        empty = grow_veins(blade, np.empty((0, 2)), method)
        assert empty.stop_reason == "complete" and len(empty.positions) == 1
        partial = grow_veins(blade, np.array([[0.01, 0.06]]), method, steps=0)
        assert partial.stop_reason == "step_budget" and partial.remaining_sources == 1
    invalid = (np.array([[np.nan, 0]]), np.array([[1.0, 0.06]]), np.zeros((4097, 2)))
    for sources in invalid:
        try:
            grow_veins(blade, sources)
        except ValueError:
            continue
        raise AssertionError("invalid vein source accepted")
    return {
        "exact_ties": True,
        "empty_graph": True,
        "explicit_partial_result": True,
        "rejected_inputs": len(invalid),
    }

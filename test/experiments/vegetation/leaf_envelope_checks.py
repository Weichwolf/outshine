"""Counterexamples for node-density artifacts and exact tapered vein subdivision."""

import numpy as np
from leaf_material_checks import scalar_field
from leaf_vein_field import vein_field
from leaf_venation import VeinPlan


def straight_plan(parts, first_radius, last_radius):
    t = np.linspace(0, 1, parts + 1)
    return VeinPlan(
        np.column_stack((t * 0.04, t * 0.03)),
        np.arange(-1, parts, dtype=np.int32),
        first_radius + t * (last_radius - first_radius),
        0,
        "complete",
    )


def check_envelopes():
    rng = np.random.default_rng(891)
    centre = np.linspace(0, 1, 97)[:, None] * np.array([0.04, 0.03])
    points = np.vstack((centre, rng.uniform((-0.01, -0.01), (0.05, 0.04), (2048, 2))))
    cases = []
    for r0, r1 in ((0.002, 0.002), (0.002, 0.0003), (0.0003, 0.002)):
        whole = straight_plan(1, r0, r1)
        reference, _ = vein_field(whole, points, "dense")
        assert np.max(np.abs(reference[: len(centre)] - 1)) < 1e-13
        errors = []
        for parts in (2, 7, 31):
            divided = straight_plan(parts, r0, r1)
            for method in ("dense", "indexed"):
                result, _ = vein_field(divided, points, method)
                errors.append(float(np.max(np.abs(result - reference))))
        assert max(errors) < 2e-13
        numerical = np.array([scalar_field(whole, p) for p in points[::23]])
        scalar_error = float(np.max(np.abs(numerical - reference[::23])))
        assert scalar_error < 1e-10
        cases.append(
            {
                "radii_m": [r0, r1],
                "points": len(points),
                "maximum_subdivision_error": max(errors),
                "maximum_numerical_error": scalar_error,
            }
        )
    constant = straight_plan(1, 0.002, 0.002)
    normal = np.array([-0.6, 0.8])
    distances = np.array([0, 0.003, 0.006, 0.0061])
    cross_section = 0.5 * constant.positions[1] + distances[:, None] * normal
    actual, _ = vein_field(constant, cross_section)
    expected = np.maximum(1 - (distances / 0.006) ** 2, 0) ** 3
    assert np.allclose(actual, expected, rtol=0, atol=1e-14)
    # Old summed kernels produce an extra cap contribution at every inserted node.
    old_interior, old_joint = -np.expm1(-1), -np.expm1(-2)
    return {
        "subdivision": cases,
        "analytic_cross_section_error": float(np.max(np.abs(actual - expected))),
        "old_joint_height_excess": float(old_joint - old_interior),
        "new_axis_height_range": [1, 1],
        "global_C1_continuity_claimed": False,
    }


if __name__ == "__main__":
    import json

    print(json.dumps(check_envelopes(), indent=2))

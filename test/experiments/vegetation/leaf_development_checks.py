"""Independent deformation, source spacing and bounded-growth checks."""

import numpy as np
from blade_surface import Blade
from leaf_development import develop_veins, insert_sources
from leaf_growth_map import LeafGrowthMap
from leaf_venation import leaf_sources


def growth_checks():
    rng = np.random.default_rng(319)
    points = rng.uniform((-0.03, 0), (0.03, 0.12), (128, 2))
    worst = 0.0
    for scale in (0.03, 0.25, 1.0):
        growth = LeafGrowthMap(scale, 2 * (1 - scale), 0.12, 0.06)
        physical = growth.forward(points)
        assert np.max(abs(growth.inverse(physical) - points)) < 1e-15
        for a, b in zip(physical[::2], physical[1::2]):
            t = np.linspace(0, 1, 65)[:, None]
            curve = growth.inverse(a + t * (b - a))
            chord = growth.inverse(a) + t * (growth.inverse(b) - growth.inverse(a))
            error = np.linalg.norm(curve - chord, axis=1)
            bound = growth.chord_error(b - a)
            worst = max(worst, abs(error.max() - bound))
            assert np.max(error) <= bound + 1e-15
            assert abs(error[32] - bound) < 1e-15
        # The shear is triangular: det(J) = scale², so it never folds the blade.
        epsilon = 1e-7
        dx = (
            growth.forward(points + [epsilon, 0])
            - growth.forward(points - [epsilon, 0])
        ) / (2 * epsilon)
        dy = (
            growth.forward(points + [0, epsilon])
            - growth.forward(points - [0, epsilon])
        ) / (2 * epsilon)
        determinant = dx[:, 0] * dy[:, 1] - dx[:, 1] * dy[:, 0]
        assert np.max(abs(determinant - scale**2)) < 1e-9
    nodes, sources = np.array([[0.0, 0.0]]), np.array([[1.0, 0.0]])
    candidates = np.array([[0.1, 0], [0.5, 0], [0.6, 0], [0.5, 0.5], [1, 0.1]])
    for method in ("dense", "tree"):
        ids = insert_sources(nodes, sources, candidates, 0.2, method)
        assert ids.tolist() == [1, 3]
    blade = Blade(lobes=0)
    candidates = leaf_sources(blade, 17, 48).reshape(2, 24, 2)
    plan, report = develop_veins(blade, candidates, settle=0)
    assert plan.stop_reason == "step_budget"
    plan, report = develop_veins(blade, candidates, max_nodes=2)
    assert len(plan.positions) <= 2 and plan.stop_reason == "node_budget"
    assert report["born_sources"] == report["removed_sources"] + plan.remaining_sources
    try:
        develop_veins(blade, candidates * 100)
    except ValueError:
        pass
    else:
        raise AssertionError("outside source candidates accepted")
    return {
        "deformation_probes": 128,
        "chord_bound_max_error_m": worst,
        "spacing_and_budgets": True,
    }

"""Independent box-world sampling validates only the stated Poisson model."""

import unittest

import numpy as np
from test_urban_parent_phase import homogeneous_parent
from urban_parent_conditional import render_conditional
from urban_parent_poisson import PoissonParent, calibrated_rates, view_footprints


def monte_carlo_shadow(model, origins, points, light, *, worlds=12000):
    """Sample actual marked boxes; no polygon operations or visibility formula."""
    rng = np.random.default_rng(1113)
    weights = (model.rates * model.pitch**2).ravel()
    count = rng.poisson(weights.sum(), worlds)
    owner = np.repeat(np.arange(worlds), count)
    choice = rng.choice(len(weights), size=len(owner), p=weights / weights.sum())
    cell, level = np.divmod(choice, len(model.parent.heights))
    width = model.parent.centre_fraction().shape[1]
    xy = np.c_[cell % width, cell // width] * model.pitch
    xy += rng.random(xy.shape) * model.pitch
    low = np.c_[xy - model.side / 2, np.zeros(len(owner))]
    high = np.c_[xy + model.side / 2, model.parent.heights[level]]
    result = []
    for origin, point, at in zip(origins, points, light, strict=True):
        hit = []
        for start, direction, limit in [
            (origin, point - origin, 1.0),
            (point, at, np.inf),
        ]:
            parallel = direction == 0
            time = np.divide(
                np.stack((low, high), axis=1) - start,
                direction,
                out=np.zeros((len(owner), 2, 3)),
                where=~parallel,
            )
            near = np.where(parallel, -np.inf, time.min(axis=1)).max(axis=1)
            far = np.where(parallel, np.inf, time.max(axis=1)).min(axis=1)
            outside = np.any(parallel & ((start < low) | (start > high)), axis=1)
            intersects = ~outside & (np.maximum(near, 0) < np.minimum(far, limit))
            blocked = np.zeros(worlds, bool)
            np.logical_or.at(blocked, owner, intersects)
            hit.append(blocked)
        visible = ~hit[0]
        joint = visible & ~hit[1]
        probability = joint.sum() / visible.sum()
        sigma = np.sqrt(max(probability * (1 - probability), 1e-10) / visible.sum())
        result.append((probability, sigma, int(visible.sum())))
    return result


class ParentPoissonTest(unittest.TestCase):
    def test_calibration_matches_every_vertical_height_cdf(self):
        parent = homogeneous_parent()
        coverage = np.array([0, 0.1, 0.85 * 0.75**2])
        rates = calibrated_rates(coverage, parent.probabilities, 12)
        tail = np.cumsum(parent.probabilities[::-1])[::-1]
        void = np.exp(-np.cumsum(rates[..., ::-1], axis=-1)[..., ::-1] * 12**2)
        np.testing.assert_allclose(void, 1 - coverage[:, None] * tail, atol=2e-15)

    def test_joint_void_preserves_coincident_view_light_hotspot(self):
        parent = homogeneous_parent()
        model = PoissonParent(parent)
        origins = np.array([[256.0, 256, 100]])
        points, light = np.array([[256.0, 256, 0]]), np.array([[0.0, 0, 1]])
        independent = model.conditional_visibility(origins, points, light, joint=False)
        joint = model.conditional_visibility(origins, points, light)
        np.testing.assert_allclose(independent, 1 - 0.85 * 0.75**2, atol=2e-15)
        np.testing.assert_array_equal(joint, [1])

    def test_joint_shadow_matches_independent_nonuniform_box_worlds(self):
        parent = homogeneous_parent()
        parent.frame[3] = 64
        parent.centre_fractions[:8] *= 0.2
        model = PoissonParent(parent)
        points = np.array([[32.0, 32, 0], [32, 24, 0], [32, 40, 0], [32, 32, 10]])
        origins = points + [
            [-60, -30, 100],
            [60, 30, 100],
            [-60, -30, 100],
            [-60, 0, -8],
        ]
        light = np.array(
            [[0.6, 0.3, 1], [-0.6, -0.3, 1], [0.6, -0.3, 1], [0.6, 0.3, 1]]
        )
        actual = model.conditional_visibility(origins, points, light)
        sampled = monte_carlo_shadow(model, origins, points, light)
        for exact, (estimate, sigma, visible) in zip(actual, sampled, strict=True):
            self.assertGreater(visible, 500)
            self.assertLess(abs(exact - estimate), 6 * sigma + 0.002)
        independent = model.conditional_visibility(origins, points, light, joint=False)
        self.assertTrue(np.all(actual >= independent))

    def test_outside_receiver_can_have_shadow_from_inside_coarse_support(self):
        parent = homogeneous_parent()
        parent.frame[3] = 64
        model = PoissonParent(parent)
        p = np.array([[-20.0, 32, 0]])
        result = model.conditional_visibility(
            p + [0, 0, 100], p, np.array([[1.0, 0, 1]])
        )
        self.assertGreater(result[0], 0)
        self.assertLess(result[0], 1)

    def test_parallel_lattice_gap_is_an_explicit_model_counterexample(self):
        parent = homogeneous_parent()
        origins = np.array([[15.68, -16.0, 2]])
        direction = np.array([[0.0, 1, 0]])
        profile = {"kind": "sun", "direction": [0, 0, 1], "intensity": 1, "ambient": 0}
        _, coverage, _ = render_conditional(parent, origins, direction, profile)
        np.testing.assert_array_equal(coverage, [0])
        model = PoissonParent(parent)
        shapes = view_footprints(origins, origins + [0, 544, 0], 48, model.side / 2)
        depth = sum(model.integral(shapes, k) for k in range(4))
        self.assertLess(float(np.exp(-depth)[0]), 0.01)


if __name__ == "__main__":
    unittest.main()

"""Independent bounded minimization checks for global cubic axis projection."""

from itertools import pairwise

import numpy as np
from scipy.optimize import minimize_scalar
from spatial_examples import spatial_axis
from spine_closest import closest_spine


def check_closest():
    rng = np.random.default_rng(173)
    maximum = 0.0
    cases = 0
    for name in ("straight", "inflection", "space"):
        axis = spatial_axis(name)
        u = np.linspace(0, 1, 31)
        centres = axis.spine.evaluate(u)
        points = centres + rng.normal(size=centres.shape) * 0.07
        for method in ("bounded", "batch", "scalar"):
            parameter, distance = closest_spine(axis.spine, points, method)
            projected = axis.spine.evaluate(parameter)
            np.testing.assert_allclose(
                np.linalg.norm(projected - points, axis=1), distance, atol=2e-12
            )
            for p, t, d in zip(points, parameter, distance):
                candidates = []
                boundaries = np.linspace(0, 1, len(axis.spine.controls) * 4 + 1)
                for a, b in pairwise(boundaries):
                    objective = lambda q, curve=axis.spine, target=p: float(
                        np.sum((curve.evaluate(q) - target) ** 2)
                    )
                    result = minimize_scalar(
                        objective,
                        bounds=(a, b),
                        method="bounded",
                        options={"xatol": 1e-14},
                    )
                    candidates.extend(
                        (
                            (result.fun, float(result.x)),
                            (objective(a), a),
                            (objective(b), b),
                        )
                    )
                value, reference = min(candidates)
                error = abs(d - np.sqrt(value))
                maximum = max(maximum, error)
                assert error < 2e-9, (name, method, error, t, reference)
                cases += 1
    return {"independent_points": cases, "maximum_distance_error_m": maximum}

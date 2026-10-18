"""Small analytic native cases, including expected absence and a retained counterexample."""

import numpy as np
from native_profile_fit import run_native
from outshine_ref.contracts import Mesh


def rectangle(heights):
    return Mesh(
        np.c_[np.array([[0, 0], [1, 0], [0, 1], [1, 1]], float), heights],
        np.array([[0, 1, 2], [1, 3, 2]], np.uint32),
    )


def checks(binary, output):
    flow, empty = np.tile([1.0, 0], (2, 1)), np.empty((0, 5))
    executed = []
    for objective in ("linf", "l2-uniform", "l2-area"):
        for name, mesh, fixed, seconds, iterations in (
            ("downhill", rectangle([1, 0.99, 1, 0.99]), {0: 1.0, 3: 0.99}, 2.0, 20000),
            ("uphill", rectangle([0, 1, 0, 1]), {}, 2.0, 20000),
            (
                "incompatible_pins",
                rectangle([0, 1, 0, 1]),
                {0: 0.0, 1: 1.0},
                2.0,
                20000,
            ),
            ("zero_time", rectangle([0, 1, 0, 1]), {}, 0.0, 20000),
            ("zero_iterations", rectangle([0, 1, 0, 1]), {}, 2.0, 0),
        ):
            height, report = run_native(
                binary,
                output / objective / name,
                mesh,
                flow,
                empty,
                objective,
                fixed=fixed,
                seconds=seconds,
                iterations=iterations,
            )
            if name in ("incompatible_pins", "zero_time", "zero_iterations"):
                assert height is None and not report["valid"], (name, report)
            else:
                assert report["valid"], (name, report)
                expected = (
                    mesh.vertices[:, 2] if name == "downhill" else np.full(4, 0.5)
                )
                np.testing.assert_allclose(height, expected, rtol=0, atol=1e-7)
                if objective == "linf" and name == "uphill":
                    assert abs(report["objective_error_m"] - 0.5) < 1e-7
            executed.append(
                {"case": name, "objective": objective, "status": report["status"]}
            )
        mesh = Mesh(
            np.array([[0, 0, 0], [1, 0, 1], [0, 1, 1]], float),
            np.array([[0, 1, 2]], np.uint32),
        )
        rows = np.array([[0, 1, 0, -0.1, 0], [0, 0, 1, -0.1, 0]], float)
        height, report = run_native(
            binary,
            output / objective / "multiple_directions",
            mesh,
            np.array([[1.0, 0]]),
            rows,
            objective,
        )
        assert report["valid"], report
        np.testing.assert_allclose(
            height, 0.5 if objective == "linf" else 2 / 3, rtol=0, atol=1e-7
        )
        executed.append(
            {
                "case": "multiple_directions",
                "objective": objective,
                "status": report["status"],
            }
        )
    height, report = run_native(
        binary,
        output / "lumped_bias",
        rectangle([0, 1, 0, 1]),
        flow,
        empty,
        "l2-lumped",
    )
    assert report["valid"] and np.ptp(height) > 0.004, report
    executed.append(
        {
            "case": "retained_lumped_bias",
            "objective": "l2-lumped",
            "status": report["status"],
        }
    )
    return executed

"""Independent polynomial, native-reference and profiled-surface checks."""

import json
import subprocess
import sys

import numpy as np
from axis_surface import profiled_axis_mesh
from curve_frame import arc_frame
from profile_curve import BezierCurve, Profile


def independent_raw(curve, parameter):
    t = np.clip(np.asarray(parameter), 0, 1)
    segment = np.minimum((t * len(curve.controls)).astype(int), len(curve.controls) - 1)
    local = t * len(curve.controls) - segment
    weights = np.stack(
        (
            (1 - local) ** 3,
            3 * local * (1 - local) ** 2,
            3 * local**2 * (1 - local),
            local**3,
        ),
        axis=-1,
    )
    return np.einsum("...i,...ij->...j", weights, np.asarray(curve.controls)[segment])


def check_curves(curves, oracle=None):
    rng = np.random.default_rng(2350)
    queries = np.r_[[-2.5, -1, 0, 1, 1.5, 2], rng.uniform(-2, 3, 40)]
    cases = []
    for curve in curves:
        np.testing.assert_allclose(
            curve.raw(queries), independent_raw(curve, queries), atol=2e-15
        )
        profile = Profile.from_curve(curve)
        for clamp in (False, True):
            for extra in (None, np.linspace(0.15, 0.4, 1000)):
                cases.append(
                    {
                        "curves": curve.controls,
                        "table": profile.table.tolist(),
                        "raw_t": queries.tolist(),
                        "x": queries.tolist(),
                        "clamp": clamp,
                        **(
                            {"extra_table": extra.tolist()} if extra is not None else {}
                        ),
                    }
                )
    for kind, preset, table in (
        ("maximum", 1, np.ones(1000)),
        ("minimum", 2, np.zeros(1000)),
        ("linear", 4, np.linspace(0, 1, 1000)),
        ("inverse_linear", 5, np.linspace(1, 0, 1000)),
    ):
        for clamp in (False, True):
            cases.append(
                {
                    "curves": curves[0].controls,
                    "table": table.tolist(),
                    "raw_t": queries.tolist(),
                    "x": queries.tolist(),
                    "clamp": clamp,
                    "preset": preset,
                    "kind": kind,
                    "extra_table": np.linspace(0.15, 0.4, 1000).tolist(),
                }
            )
    result = {
        "independent_bernstein_matches": True,
        "curves": len(curves),
        "native_cases": 0,
    }
    if oracle is None:
        return result
    process = subprocess.run(
        [sys.executable, str(oracle)],
        input=json.dumps({"cases": cases}),
        text=True,
        capture_output=True,
        timeout=30,
        check=True,
    )
    reference = json.loads(process.stdout)
    worst = 0.0
    for case, native in zip(cases, reference["cases"], strict=True):
        curve = BezierCurve(case["curves"])
        profile = Profile(case["table"], case.get("kind", "custom"))
        extra = Profile(case["extra_table"]) if "extra_table" in case else None
        expected = {
            "raw_points": curve.raw(queries),
            "values": profile.evaluate(queries, extra=extra),
            "profile": profile.evaluate(queries, extra=extra, clamp=case["clamp"]),
            "tiled": profile.evaluate(
                queries, tiled=True, extra=extra, clamp=case["clamp"]
            ),
            "parent": np.clip(queries, 0, 1) * 2.5 if case["clamp"] else queries * 2.5,
        }
        for name, value in expected.items():
            error = float(np.max(np.abs(value - np.asarray(native[name]))))
            worst = max(worst, error)
            np.testing.assert_allclose(
                value, native[name], atol=1.5e-6, rtol=1e-6, err_msg=name
            )
    result.update(
        native_cases=len(cases),
        native_maximum_absolute_error=worst,
        native_reference=reference,
    )
    return result


def rejection_checks():
    invalid = [
        lambda: BezierCurve(()),
        lambda: BezierCurve((((0, 0), (0.3, 0), (0.7, float("nan")), (1, 1)),)),
        lambda: BezierCurve(
            (((0, 0), (0.3, 0), (0.7, 1), (1, 1)), ((0.9, 1), (0.9, 1), (1, 1), (1, 1)))
        ),
        lambda: BezierCurve((((0, 0), (1.5, 1), (-0.5, 1), (1, 1)),)).value(0.5),
        lambda: Profile(np.zeros(3)),
        lambda: Profile(np.full(1000, float("inf"))),
        lambda: Profile(np.linspace(0, 1, 1000), "unknown"),
        lambda: Profile(np.ones(1000), "linear"),
        lambda: Profile(np.ones(1000)).evaluate(float("nan")),
        lambda: profiled_axis_mesh(1, 0.5, 0.1, Profile(np.r_[np.ones(999), 0])),
        lambda: profiled_axis_mesh(
            1, 0.5, 0.1, Profile(np.r_[np.ones(699), -0.1, np.ones(300)])
        ),
        lambda: profiled_axis_mesh(1, 2, 0.6, Profile(np.ones(1000))),
    ]
    for call in invalid:
        try:
            call()
        except ValueError:
            continue
        raise AssertionError("invalid profile or geometry accepted")
    crossed = BezierCurve((((0, 0), (0.8, 0.2), (0.2, 0.8), (1, 1)),))
    np.testing.assert_allclose(
        crossed.value(np.array([0, 0.5, 1])), [0, 0.5, 1], atol=1e-13
    )
    return len(invalid)


def surface_checks(profile):
    length, bend, base = 1.8, 0.45, 0.13
    mesh = profiled_axis_mesh(length, bend, base, profile, segments=128, sides=32)
    points = mesh["vertices"][mesh["faces"]]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    assert np.all(np.linalg.norm(cross, axis=1) > 0)
    corners = mesh["normals"].reshape(-1, 3, 3)
    assert np.all(np.sum(cross * corners.mean(axis=1), axis=1) > 0)
    np.testing.assert_allclose(np.linalg.norm(corners, axis=2), 1, atol=1e-14)
    edges = np.concatenate(
        [mesh["faces"][:, [0, 1]], mesh["faces"][:, [1, 2]], mesh["faces"][:, [2, 0]]]
    )
    _, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
    assert np.all(counts == 2)
    eps = 1e-7
    t = np.linspace(0.05, 0.95, 16) + 0.000127
    t = t[
        np.min(
            np.abs(t[:, None] - np.linspace(0, 1, len(profile.table))[None, :]), axis=1
        )
        > eps * 2
    ]
    angle = 0.8

    def position(time, theta):
        centre, _, lateral, normal = arc_frame(length, bend, 0, time)
        radial = np.cos(theta) * lateral + np.sin(theta) * normal
        return centre + base * profile.evaluate(time)[..., None] * radial

    dt = (position(t + eps, angle) - position(t - eps, angle)) / (2 * eps)
    ds = (position(t, angle + eps) - position(t, angle - eps)) / (2 * eps)
    measured = np.cross(dt, ds)
    measured /= np.linalg.norm(measured, axis=1)[:, None]
    _, tangent, lateral, normal = arc_frame(length, bend, 0, t)
    radial = np.cos(angle) * lateral + np.sin(angle) * normal
    target = (length - bend * base * profile.evaluate(t) * np.sin(angle))[
        :, None
    ] * radial - base * profile.derivative(t)[:, None] * tangent
    target /= np.linalg.norm(target, axis=1)[:, None]
    np.testing.assert_allclose(measured, target, atol=2e-8)
    return {
        "closed_mesh": True,
        "independent_normal_difference_matches": True,
        "triangles": len(mesh["faces"]),
    }

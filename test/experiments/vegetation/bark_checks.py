"""Independent analytic, closest-centre and rigid-transform bark checks."""

from dataclasses import replace

import numpy as np
from bark_coordinates import axis_chart, material_coordinates
from plant_experiment import sapling
from scipy.optimize import minimize_scalar
from scipy.spatial.transform import Rotation


def check_charts():
    root, level = sapling().axis(()), sapling().levels[0]
    maximum = 0.0
    for bend in (0.0, 0.4, -0.4, np.pi, -np.pi):
        state = replace(root, bend=bend, origin=np.zeros(3), frame=np.eye(3))
        parameters = np.repeat(np.linspace(0, 1, 17), 9)
        angles = np.tile(np.linspace(0, 2 * np.pi, 9), 17)
        arc = bend * parameters
        if bend:
            centres = np.column_stack(
                (
                    arc * 0,
                    state.length / bend * np.sin(arc),
                    state.length / bend * (1 - np.cos(arc)),
                )
            )
        else:
            centres = np.column_stack((arc * 0, state.length * parameters, arc * 0))
        radius = level.base_radius + parameters * (level.tip_radius - level.base_radius)
        x, y = radius * np.cos(angles), radius * np.sin(angles)
        points = centres + np.column_stack((x, -y * np.sin(arc), y * np.cos(arc)))
        expected = np.column_stack((x, y, parameters * state.length))
        chart, actual_radius, distance = axis_chart(state, level, points)
        np.testing.assert_allclose(chart, expected, atol=2e-14)
        np.testing.assert_allclose(actual_radius, radius, atol=2e-14)
        np.testing.assert_allclose(distance, radius, atol=2e-14)
        np.testing.assert_allclose(chart[::9], chart[8::9], atol=2e-14)
        rotation = Rotation.from_rotvec((0.4, -0.7, 1.2)).as_matrix()
        transformed = replace(state, frame=rotation, origin=np.array((51, -23, 4)))
        moved = points @ rotation.T + transformed.origin
        np.testing.assert_allclose(
            axis_chart(transformed, level, moved)[0], chart, atol=2e-13
        )
        scaled = replace(state, length=state.length * 7, origin=state.origin * 7)
        scaled_level = replace(
            level,
            length=level.length * 7,
            base_radius=level.base_radius * 7,
            tip_radius=level.tip_radius * 7,
        )
        np.testing.assert_allclose(
            axis_chart(scaled, scaled_level, points * 7)[0], chart * 7, atol=2e-13
        )
        for point in points[9:-9:13]:

            def objective(t, bend=bend, state=state, point=point):
                angle = bend * t
                centre = (
                    np.array((0, state.length * t, 0))
                    if not bend
                    else np.array(
                        (
                            0,
                            state.length / bend * np.sin(angle),
                            state.length / bend * (1 - np.cos(angle)),
                        )
                    )
                )
                return np.sum((point - centre) ** 2)

            result = minimize_scalar(
                objective, bounds=(0, 1), method="bounded", options={"xatol": 1e-13}
            )
            direct = axis_chart(state, level, point[None])[0][0, 2] / state.length
            maximum = max(maximum, abs(result.x - direct))
    assert maximum < 3e-8, maximum
    return {
        "bends": 5,
        "analytic_points": 765,
        "closest_centre_parameter_error": maximum,
    }


def check_selection(vertices):
    plant = sapling()
    results = {
        method: material_coordinates(plant, vertices, method)[0]
        for method in ("dense", "indexed")
    }
    for key in results["dense"]:
        np.testing.assert_array_equal(results["dense"][key], results["indexed"][key])
    assert not plant._leaves
    fresh = sapling()
    material_coordinates(fresh, np.zeros((0, 3)))
    assert not fresh._axes and not fresh._leaves
    for points, method in (
        (np.zeros((1_000_001, 3)), "indexed"),
        (np.array([[np.nan, 0, 0]]), "indexed"),
        (np.array([[1j, 0, 0]]), "indexed"),
        (np.zeros((1, 3)), "unknown"),
    ):
        try:
            material_coordinates(fresh, points, method)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid request reached material expansion")
    assert not fresh._axes and not fresh._leaves
    return results["indexed"]

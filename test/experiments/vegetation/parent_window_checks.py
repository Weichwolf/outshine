"""Independent physical ranges, cached-axis geometry and preflight controls."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from conifer_windows import bind_windows, pattern_key
from generator_specs import TubeShape
from outshine_ref.vegetation import (
    AxisLevel,
    AxisState,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
    Profile,
)
from parent_window_product import build, window_plan, wood_mesh


def control():
    plant = NeedledPlant(
        PlantPlan(
            17, (AxisLevel(1, 0.3, 0.02, 0.01), AxisLevel(2, 0.2, 0.001, 0.0001)), ()
        ),
        (
            None,
            NeedlePattern(first=0, last=1, spacing=0.025, group_size=1, short_length=0),
        ),
        (Needle(length=0.02),),
    )
    axes = {
        "origins": np.array(((0, 0, 0), (1, 0, 0), (2, 0, 0)), dtype=float),
        "rotations": np.tile(np.eye(3), (3, 1, 1)),
        "lengths": np.array((0.3, 0.1, 0.2)),
        "bends": np.zeros(3),
        "path_offsets": np.array((0, 2, 5, 8), dtype=np.uint64),
        "path_pairs": np.array(
            ((0, 0), (1, 0), (0, 0), (1, 0), (2, 0), (0, 0), (1, 0), (2, 1))
        ),
        "needle_counts": np.array((0, 5, 9), dtype=np.uint32),
        "first_needle_ids": np.array((0, 0, 5), dtype=np.uint64),
    }
    return plant, axes


class ParentWindowTests(unittest.TestCase):
    def test_actual_axis_counts_and_centres_match_independent_requested_needles(self):
        plant, axes = control()
        with patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("full organs")
        ):
            windows = window_plan(plant, axes, (0, 1, 1), 2)
        np.testing.assert_array_equal(windows["first_needle_ids"], (0, 5))
        np.testing.assert_array_equal(windows["needle_counts"], (5, 9))
        np.testing.assert_allclose(
            windows["origins"], ((1, 0.05, 0), (2, 0.1, 0)), atol=1e-15
        )
        for index in (1, 2):
            axis = AxisState(
                (index - 1,),
                index,
                float(axes["lengths"][index]),
                0,
                axes["origins"][index],
                np.eye(3),
                0,
                0,
            )
            detail = plant.patterns[1].detail(axis, (0.001, 0.0001), 17, 100)
            self.assertEqual(len(detail["ids"]), (5, 9)[index - 1])
            self.assertAlmostEqual(
                float(windows["parent_parameters"][index - 1]),
                float(detail["parent_parameters"].mean()),
            )

    def test_catalog_capacity_matches_physical_tails(self):
        plant, axes = control()
        windows = window_plan(plant, axes, (0, 1, 1), 2)
        key = pattern_key(plant.patterns[1])
        catalog = {key: {"needles": 12, "phase": 0, "family": 0}}
        bound = bind_windows(plant, windows, catalog)
        np.testing.assert_allclose(bound["weights"], (5 / 12, 9 / 12))
        catalog[key]["needles"] = 11
        with self.assertRaises(ValueError):
            bind_windows(plant, windows, catalog)

    def test_cached_uniform_scale_and_rotation_retain_physical_wood(self):
        shape = TubeShape(length=0.2, bend=0, radius=Profile(np.full(4, 0.01)))
        graph = SimpleNamespace(
            by_id={i: SimpleNamespace(shape=shape) for i in range(1, 5)}
        )
        axes = {
            "origins": np.array(((10, 20, 30), (100, 0, 0), (200, 0, 0), (300, 0, 0))),
            "rotations": np.tile(
                np.array(((0, -1, 0), (1, 0, 0), (0, 0, 1))), (4, 1, 1)
            ),
            "uniform_scales": np.array((2, 1, 1, 1)),
        }
        mesh = wood_mesh(graph, axes, np.arange(4))
        stem = mesh["vertices"][mesh["vertices"][:, 0] < 20]
        np.testing.assert_allclose(stem.min(axis=0), (9.6, 19.98, 29.98), atol=4e-15)
        np.testing.assert_allclose(stem.max(axis=0), (10, 20.02, 30.02), atol=4e-15)
        np.testing.assert_allclose(
            np.linalg.norm(mesh["normals"], axis=1), 1, atol=2e-15
        )
        self.assertEqual(len(mesh["normals"]), len(mesh["faces"]) * 3)

    def test_over_budget_execution_refuses_before_catalog_io(self):
        with (
            patch(
                "parent_window_product.read_execution",
                return_value=({"windows": 100001}, None, None, None),
            ),
            patch("parent_window_product.load_catalog") as decode,
        ):
            with self.assertRaises(ValueError):
                build(None, None, None, None)
            decode.assert_not_called()
        plant, axes = control()
        with patch("parent_window_product._axis_window_values") as allocate:
            with self.assertRaises(ValueError):
                window_plan(plant, axes, (0, 1, 1), 1)
            allocate.assert_not_called()


if __name__ == "__main__":
    unittest.main()

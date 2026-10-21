import copy
import json
import unittest

import numpy as np
from mvt_directional_scalar_control import directional_facets as scalar
from mvt_native_body_layout import prepare_native_body_plan
from mvt_t_connection_inputs import FIXTURE, inputs, source_rows
from mvt_t_connections import plan_connections
from mvt_t_directional_facets import directional_facets
from stair_junction_solve import Conditions
from test_mvt_native_body import transform


class DirectionalAssembly(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.sources = inputs()
        cls.native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )

    def plan(self, angle=0, translation=(0, 0)):
        case, native = transform(self.case, self.native, angle, translation)
        sources = source_rows(case["ways"])
        return prepare_native_body_plan(
            case, sources, plan_connections(sources), native
        )[0]

    def assert_same(self, plan, precision=True, crossfall=0.2):
        angle = np.arange(16) * 2 * np.pi / 16
        directions = np.c_[np.cos(angle), np.sin(angle)]
        systems, notes = [], []
        for method in (scalar, directional_facets):
            system = Conditions(len(plan["xy"]) + 3 * len(plan["nodes"]) + 1)
            notes.append(
                method(
                    plan,
                    system,
                    directions,
                    np.cos(np.pi / 16),
                    True,
                    precision,
                    height_bound_m=40,
                    crossfall_limit=crossfall,
                )
            )
            systems.append(system)
        self.assertEqual(notes[0], notes[1])
        # Preserve row/term order before CSC conversion too, including zeros and
        # repeated constraints where two original segments influence one face.
        for name in ("rows", "columns", "values", "bounds"):
            np.testing.assert_array_equal(
                getattr(systems[0], name), getattr(systems[1], name)
            )
        for name in ("indptr", "indices", "data"):
            np.testing.assert_array_equal(
                getattr(systems[0].matrix(), name), getattr(systems[1].matrix(), name)
            )

    def test_original_and_rigidly_transformed_programs_are_identical(self):
        for angle, translation in (
            (0, (0, 0)),
            (0.37, (91, -17)),
            (-1.2, (-130, 71)),
        ):
            with self.subTest(angle=angle):
                self.assert_same(self.plan(angle, translation))

    def test_precision_disabled_and_tighter_crossfall_keep_original_rows(self):
        plan = self.plan()
        self.assert_same(plan, precision=False)
        self.assert_same(plan, crossfall=0.05)

    def test_empty_owner_and_uncovered_geometry_are_not_silently_dropped(self):
        plan = self.plan()
        plan["parts"][1] = np.empty((0, 3), int)
        self.assert_same(plan)
        plan = self.plan()
        plan["rows"][1]["xy"] = np.array([[1e5, 1e5], [1e5 + 1, 1e5]])
        for method in (scalar, directional_facets):
            with self.assertRaisesRegex(ValueError, "without an axis segment"):
                method(
                    plan,
                    Conditions(1000),
                    np.eye(2),
                    1,
                    True,
                    True,
                    height_bound_m=40,
                    crossfall_limit=0.2,
                )

    def test_bad_precision_and_crossfall_remain_rejected(self):
        plan = self.plan()
        for method in (scalar, directional_facets):
            with self.assertRaisesRegex(ValueError, "crossfall"):
                method(
                    plan,
                    Conditions(1000),
                    np.eye(2),
                    1,
                    True,
                    True,
                    height_bound_m=40,
                    crossfall_limit=0.001,
                )
            with self.assertRaisesRegex(ValueError, "better conditioned precision"):
                method(
                    plan,
                    Conditions(1000),
                    np.eye(2),
                    1,
                    True,
                    True,
                    height_bound_m=1e9,
                    crossfall_limit=0.2,
                )

    def test_sparse_row_blocks_preserve_existing_rows_and_own_values(self):
        a, b = Conditions(9), Conditions(9)
        for system in (a, b):
            system.add([1, 4], [0.25, -0.0], 3)
        columns = np.array([[2, 2, 7], [0, 8, 1]])
        values = np.array([[1e-12, -7, -0.0], [8, 3, -2.5]])
        bounds = np.array([1.5, -6])
        for col, val, bound in zip(columns, values, bounds, strict=True):
            a.add(col, val, bound)
        b.add_rows(columns, values, bounds)
        b.add_rows(np.empty((0, 3), int), np.empty((0, 3)), np.empty(0))
        columns[:] = 0
        values[:] = 0
        bounds[:] = 0
        self.assertEqual(a.bounds, b.bounds)
        for field in ("indptr", "indices", "data"):
            np.testing.assert_array_equal(
                getattr(a.matrix(), field), getattr(b.matrix(), field)
            )
        original = copy.deepcopy(b.__dict__)
        with self.assertRaises(ValueError):
            b.add_rows([[1, 2]], [[1]], [4])
        self.assertEqual(b.__dict__, original)


if __name__ == "__main__":
    unittest.main()

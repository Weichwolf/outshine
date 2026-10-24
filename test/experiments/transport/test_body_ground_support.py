"""Actual underside witnesses, dyadic triangulation controls and bounded failures."""

import json
import unittest

import numpy as np
from body_ground_support import prepare, sample
from body_ground_support_experiment import control_body as plane
from body_ground_support_experiment import extend, native_soft_exterior
from world_stair_reference import FIXTURE, construct

STRATEGIES = ("scan", "index", "boundary")


class BodyGroundSupport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = construct(json.loads(FIXTURE.read_text()))
        cls.body = cls.case["data"]

    def test_same_dyadic_surface_has_analytic_support_for_both_diagonals(self):
        points = np.array(
            [
                [4.5, 0.25],
                [4.5, 2],
                [2, 4.5],
                [0.25, 4.5],
                [-0.5, 3.5],
                [1, 1],
                [2, 2],
                [0, 0],
                [4, 4],
            ]
        )
        foot = np.clip(points, 0, 4)
        for diagonal in (0, 1):
            for strategy in STRATEGIES:
                with self.subTest(diagonal=diagonal, strategy=strategy):
                    result = sample(
                        prepare(plane(diagonal), 1, strategy=strategy), points
                    )
                    self.assertTrue(result["valid"].all())
                    np.testing.assert_array_equal(result["foot_xy_m"], foot)
                    np.testing.assert_array_equal(
                        result["height_m"], 2 + 0.25 * foot.sum(axis=1)
                    )
                    np.testing.assert_array_equal(
                        result["distance_m"], np.linalg.norm(points - foot, axis=1)
                    )

    def test_world_cap_centres_keep_the_actual_underside_height_and_face_binding(self):
        points = self.body["body_vertices"][self.body["bottom_faces"]].mean(axis=1)
        for strategy in STRATEGIES:
            model = prepare(self.body, 1, strategy=strategy)
            result = sample(model, points[:, :2])
            self.assertTrue(result["valid"].all())
            np.testing.assert_allclose(
                result["height_m"], points[:, 2], atol=1e-7, rtol=0
            )
            np.testing.assert_array_equal(result["distance_m"], 0)
            np.testing.assert_array_equal(result["body_cap_face_ids"], np.arange(58))
            np.testing.assert_array_equal(
                result["source_masks"], self.body["face_sources"]
            )

    def test_old_exterior_control_changes_with_diagonal_and_nearest_recipe_does_not(
        self,
    ):
        point = np.array([[4.5, 0.25]])
        old, new = [], []
        for diagonal in (0, 1):
            model = prepare(plane(diagonal), 1)
            old.append(native_soft_exterior(model, point, 0).item())
            new.append(extend(sample(model, point), 0).item())
        np.testing.assert_allclose(
            old, [3.182405905288869, 3.1038840996118093], atol=1e-14, rtol=0
        )
        self.assertAlmostEqual(old[0] - old[1], 0.07852180567705958, places=14)
        self.assertEqual(new[0], new[1])
        model = prepare(plane(), 1)
        hard = sample(model, np.array([[1.0, 1.0], [2.0, 2.0]]))
        np.testing.assert_array_equal(extend(hard, 0), hard["height_m"])
        with self.assertRaisesRegex(ValueError, "height budget"):
            extend(hard, 0, most=1)

    def test_world_full_grid_has_equal_support_from_all_three_methods(self):
        x, y = np.meshgrid(np.arange(17.0, 30, 0.5), np.arange(265.0, 280, 0.5))
        points = np.column_stack([x.ravel(), y.ravel()])
        results = [
            sample(prepare(self.body, 1, strategy=s), points) for s in STRATEGIES
        ]
        for result in results:
            self.assertTrue(result["valid"].all())
            np.testing.assert_allclose(
                result["height_m"], results[0]["height_m"], atol=1e-7, rtol=0
            )
            np.testing.assert_allclose(
                result["foot_xy_m"], results[0]["foot_xy_m"], atol=1e-8, rtol=0
            )
            np.testing.assert_allclose(
                result["distance_m"], results[0]["distance_m"], atol=1e-8, rtol=0
            )
            np.testing.assert_array_equal(
                result["body_cap_face_ids"], results[0]["body_cap_face_ids"]
            )
            np.testing.assert_array_equal(
                result["source_masks"], results[0]["source_masks"]
            )

    def test_compaction_keeps_actual_index_identity_and_snapshot_owns_its_buffers(self):
        body = {k: a.copy() for k, a in self.body.items()}
        model = prepare(body, 1)
        ids = model.source_vertex_ids
        self.assertEqual(len(ids), 56)
        np.testing.assert_array_equal(
            model.sampler.mesh.vertices, body["body_vertices"][ids]
        )
        np.testing.assert_array_equal(
            ids[model.sampler.mesh.triangles], body["bottom_faces"]
        )
        result = sample(model, np.array([[23.0, 271.0]]))
        body["body_vertices"][:] = 0
        body["face_sources"][:] = 0
        after = sample(model, np.array([[23.0, 271.0]]))
        np.testing.assert_array_equal(result["height_m"], after["height_m"])
        np.testing.assert_array_equal(result["source_masks"], after["source_masks"])
        self.assertFalse(model.sampler.mesh.vertices.flags.writeable)
        self.assertFalse(model.source_vertex_ids.flags.writeable)

    def test_true_boundary_has_no_internal_edges_and_keeps_cap_witnesses(self):
        model = prepare(plane(), 1, strategy="boundary")
        self.assertEqual(model.preparation["boundary_edges"], 4)
        self.assertFalse(model.boundary_geometries.flags.writeable)
        with self.assertRaises(TypeError):
            model.preparation["boundary_edges"] = 9
        result = sample(model, np.array([[4.5, 0.25]]))
        self.assertEqual(result["body_cap_face_ids"].item(), 0)
        self.assertEqual(result["source_masks"].item(), 1)

    def test_actual_shared_vertex_ties_fit_a_single_input_query_budget(self):
        result = sample(prepare(plane(), 1), np.array([[0.0, 0.0]]), max_queries=1)
        self.assertTrue(result["valid"].item())
        self.assertEqual(result["height_m"].item(), 2)
        self.assertEqual(result["body_cap_face_ids"].item(), 0)

    def test_coincident_and_touching_unshared_sheets_are_not_joined(self):
        for height in (0, 2):
            vertices = np.array(
                [
                    [0.0, 0.0, 0.0],
                    [1.0, 0.0, 0.0],
                    [0.0, 1.0, 0.0],
                    [1.0, 0.0, height],
                    [1.0, 1.0, height],
                    [0.0, 1.0, height],
                ]
            )
            body = {
                "body_vertices": vertices,
                "bottom_faces": np.array([[0, 1, 2], [3, 4, 5]]),
                "face_sources": np.array([1, 2]),
            }
            with self.assertRaisesRegex(ValueError, "indexed continuity"):
                prepare(body, 1)
        body = plane()
        body["body_vertices"] = np.vstack(
            [body["body_vertices"], body["body_vertices"] + [0, 0, 2]]
        )
        body["bottom_faces"] = np.vstack(
            [body["bottom_faces"], body["bottom_faces"] + 4]
        )
        body["face_sources"] = np.array([1, 1, 2, 2])
        with self.assertRaises(ValueError):
            prepare(body, 1)

    def test_equal_distance_different_heights_are_unresolved_locally(self):
        body = {
            "body_vertices": np.array(
                [
                    [-2.0, -1.0, 0.0],
                    [-1.0, -1.0, 0.0],
                    [-1.0, 1.0, 0.0],
                    [1.0, -1.0, 2.0],
                    [2.0, -1.0, 2.0],
                    [1.0, 1.0, 2.0],
                ]
            ),
            "bottom_faces": np.array([[0, 1, 2], [3, 4, 5]]),
            "face_sources": np.array([1, 2]),
        }
        for strategy in STRATEGIES:
            result = sample(
                prepare(body, 1, strategy=strategy),
                np.array([[0.0, 0.0], [-1.5, -0.5]]),
            )
            np.testing.assert_array_equal(result["valid"], [False, True])
            self.assertTrue(np.isnan(result["height_m"][0]))
            self.assertEqual(result["body_cap_face_ids"][0], -1)
            self.assertEqual(result["source_masks"][0], 0)
            self.assertEqual(result["reasons"][0], "ambiguous_nearest_height")

    def test_unsigned_source_masks_and_contact_keys_remain_exact(self):
        body = plane()
        body["face_sources"] = np.array([2**63, 2**63], dtype=np.uint64)
        result = sample(prepare(body, 2**64 - 1), np.array([[1.0, 1.0]]))
        self.assertEqual(result["contact_key"], 2**64 - 1)
        self.assertEqual(int(result["source_masks"][0]), 2**63)

    def test_budget_cancel_and_invalid_inputs_leave_source_untouched(self):
        body = plane()
        before = {k: a.tobytes() for k, a in body.items()}
        for kwargs in (
            {"max_faces": 1},
            {"max_pairs": 1},
            {"max_faces": True},
            {"cancelled": lambda: True},
        ):
            with self.assertRaises(ValueError):
                prepare(body, 1, **kwargs)
        for key in (0, -1, True, 2**64):
            with self.assertRaises(ValueError):
                prepare(body, key)
        model = prepare(body, 1)
        points = np.array([[1.0, 1.0], [2.0, 2.0]])
        for kwargs in (
            {"max_queries": 1},
            {"max_pairs": 3},
            {"cancelled": lambda: True},
        ):
            with self.assertRaises(ValueError):
                sample(model, points, **kwargs)
        with self.assertRaises(ValueError):
            sample(model, np.array([[np.nan, 0.0]]))
        empty = sample(model, np.empty((0, 2)), max_queries=0, max_pairs=0)
        self.assertEqual(empty["height_m"].shape, (0,))
        for k, a in body.items():
            self.assertEqual(a.tobytes(), before[k])
        body["body_vertices"] = body["body_vertices"].astype(np.float32)
        with self.assertRaisesRegex(ValueError, "binary64"):
            prepare(body, 1)


if __name__ == "__main__":
    unittest.main()

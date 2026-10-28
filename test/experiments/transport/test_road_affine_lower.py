"""Whole original affine bodies, bounded admission and independent box controls."""

import copy
import itertools
import unittest
from unittest.mock import patch

import numpy as np
from replay_road_closed_surface import ARRAYS, original_cases
from road_affine_lower import box_distance, plan_lower
from road_closed_checks import observed_body_error
from road_closed_surface import prepare_closed
from road_surface_model import Curve
from test_road_closed_surface import straight
from test_road_surface_lod import recipe


class AffineBounds(unittest.TestCase):
    def test_correlated_box_encloses_interior_points_for_both_slope_signs(self):
        for p, q, along in itertools.product(
            ((-4, 3), (-4, -1), (1, 4)),
            ((-0.4, 0.7), (-0.7, -0.1), (0.1, 0.7)),
            ((-0.2, 0.3), (-0.3, -0.1), (0.1, 0.3)),
        ):
            limit = box_distance(p, q, along, 0.18, 0.3)
            samples = np.array(
                list(itertools.product(*(np.linspace(*a, 11) for a in (p, q, along))))
            )
            x, y, target = samples.T
            normal = np.c_[-x, y, np.ones(len(x))]
            normal /= np.linalg.norm(normal, axis=1)[:, None]
            delta = (
                0.3 * normal - np.c_[target, np.zeros(len(x)), np.full(len(x), 0.18)]
            )
            self.assertLessEqual(
                float(np.linalg.norm(delta, axis=1).max()), limit + 1e-15
            )

    def test_nonconstant_cap_normals_and_curved_frames_are_refused(self):
        source = recipe(curvature=0.01)
        with self.assertRaisesRegex(ValueError, "straight"):
            plan_lower(Curve(source))
        with self.assertRaisesRegex(ValueError, "cap normals"):
            plan_lower(Curve(recipe(bank_rate=0.01)))
        source = straight()
        source["intervals"][0]["middle"][4] = 1e-8
        with self.assertRaisesRegex(ValueError, "straight"):
            plan_lower(Curve(source))
        for boxes in (True, 0, 128):
            with self.assertRaisesRegex(ValueError, "boxes"):
                plan_lower(Curve(straight()), boxes=boxes)

    def test_old_admission_does_not_call_the_affine_planner_or_exact_pairs(self):
        with (
            patch("road_affine_lower.plan_lower", side_effect=AssertionError("affine")),
            patch(
                "road_closed_exact.qualify_intersections",
                side_effect=AssertionError("exact"),
            ),
        ):
            old = prepare_closed(straight(), 0.25, max_payload_bytes=1024)
            new = prepare_closed(
                straight(), 0.25, max_payload_bytes=1024, affine_lower=True
            )
        for key in ARRAYS:
            np.testing.assert_array_equal(new[key], old[key])
        for key in old.keys() - set(ARRAYS):
            self.assertEqual(new[key], old[key])
        with self.assertRaisesRegex(ValueError, "choice"):
            prepare_closed(straight(), 0.25, max_payload_bytes=1024, affine_lower=1)


class OriginalAffineBody(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = next(
            c["recipe"] for c in original_cases()[0] if c["ordinal"] == 54991
        )

    def test_entire_last_body_both_methods_and_width_hull_alternatives(self):
        original = copy.deepcopy(self.source)
        for method, counts in (("adaptive", (56, 108)), ("uniform", (72, 140))):
            previous = None
            for boxes in (16, 32, 64):
                body = prepare_closed(
                    self.source,
                    0.25,
                    max_payload_bytes=8 * 1024 * 1024,
                    method=method,
                    refine_normal_bound=True,
                    affine_lower=True,
                    affine_width_boxes=boxes,
                )
                self.assertEqual(
                    body["recipe_version"], "outshine/direct-affine-road-display/1"
                )
                self.assertEqual(
                    (len(body["positions_enh_m"]), len(body["indices"])), counts
                )
                self.assertLessEqual(body["maximum_stored_bound_m"], 0.25)
                self.assertLessEqual(
                    observed_body_error(self.source, body),
                    body["maximum_stored_bound_m"],
                )
                self.assertEqual(
                    body["quality"]["exact_intersections"]["self_intersections"], 0
                )
                self.assertGreater(body["quality"]["signed_volume_m3"], 0)
                self.assertLessEqual(
                    body["maximum_actual_lower_rounding_m"],
                    body["lower_rounding_allowance_m"],
                )
                self.assertEqual(
                    body["payload_bytes"], sum(body[key].nbytes for key in ARRAYS)
                )
                if previous is not None:
                    for key in ARRAYS:
                        np.testing.assert_array_equal(body[key], previous[key])
                previous = body
        self.assertEqual(self.source, original)

    def test_original_is_retained_when_whole_error_payload_or_pair_work_fails(self):
        options = {"refine_normal_bound": True, "affine_lower": True}
        with self.assertRaisesRegex(ValueError, "Normal-offset"):
            prepare_closed(
                self.source, 0.2, max_payload_bytes=8 * 1024 * 1024, **options
            )
        with self.assertRaisesRegex(RuntimeError, "capacity"):
            prepare_closed(self.source, 0.25, max_payload_bytes=604, **options)
        with self.assertRaisesRegex(ValueError, "intersection_work_budget"):
            prepare_closed(
                self.source,
                0.25,
                max_payload_bytes=8 * 1024 * 1024,
                affine_width_boxes=8,
                **options,
            )


if __name__ == "__main__":
    unittest.main()

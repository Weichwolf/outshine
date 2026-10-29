"""Hysteresis and failures preserve a single source-bound representation."""

import unittest
from dataclasses import replace

import numpy as np

from field_view_admission import ViewDomain, select_field
from field_view_sampling import FieldGrid, ImageView


class AdmissionTests(unittest.TestCase):
    def setUp(self):
        self.view = ImageView((0, 0, 64000), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 900)
        self.domain = ViewDomain(
            FieldGrid((0, 0, 0), (0, 0, 1), 200),
            self.view,
            "original-ids-and-geometry",
            "view-and-material-light-shader",
            (60000, 68000),
            0.5,
            (0.04491, 0.02052),
        )
        self.inputs = {
            "was_field": False,
            "source_binding": self.domain.source_binding,
            "radiance_binding": self.domain.radiance_binding,
            "complete": True,
            "depth_separated": True,
            "composition_qualified": True,
        }

    def test_enter_stay_exit_never_has_two_owners(self):
        states, was_field = [], False
        for z in (60500, 62000, 60500, 59000, 60500, 62000):
            mode, _ = select_field(
                self.domain,
                replace(self.view, eye=(0, 0, z)),
                **(self.inputs | {"was_field": was_field}),
            )
            states.append(mode)
            was_field = mode == "field"
        self.assertEqual(
            states, ["parent", "field", "field", "parent", "parent", "field"]
        )

    def test_unknown_originals_or_depth_are_never_zero_cost_success(self):
        for key in ("complete", "depth_separated", "composition_qualified"):
            self.assertEqual(
                select_field(self.domain, self.view, **(self.inputs | {key: False}))[0],
                "parent",
            )

    def test_depth_separation_does_not_remove_coverage_background_covariance(self):
        coverage = np.array([1.0, 1, 0, 0])
        background = np.array([1.0, 1, 0, 0])
        true_filtered = np.mean(coverage + (1 - coverage) * background)
        separately_filtered = (
            coverage.mean() + (1 - coverage.mean()) * background.mean()
        )
        self.assertEqual(true_filtered, 0.5)
        self.assertEqual(separately_filtered - true_filtered, 0.25)
        self.assertEqual(
            select_field(
                self.domain,
                self.view,
                **(self.inputs | {"composition_qualified": False}),
            ),
            ("parent", "subpixel_composition_unqualified"),
        )

    def test_changed_source_lighting_camera_and_failed_calibration_keep_parent(self):
        for key in ("source_binding", "radiance_binding"):
            self.assertEqual(
                select_field(
                    self.domain, self.view, **(self.inputs | {key: "changed"})
                )[0],
                "parent",
            )
        self.assertEqual(
            select_field(self.domain, replace(self.view, focal_px=901), **self.inputs)[
                0
            ],
            "parent",
        )
        failed = replace(self.domain, measured_errors=(0.0554, 0.0242))
        self.assertEqual(select_field(failed, self.view, **self.inputs)[0], "parent")

    def test_binding_is_owned_and_calibration_cannot_be_unbounded(self):
        distances = [60000, 68000]
        domain = replace(self.domain, distance_m=distances)
        distances[0] = 1
        self.assertEqual(domain.distance_m, (60000, 68000))
        for options in (
            {"measured_errors": (float("nan"), 0)},
            {"distance_m": (1, float("inf"))},
            {"source_binding": ""},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                replace(self.domain, **options)


if __name__ == "__main__":
    unittest.main()

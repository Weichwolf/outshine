"""Exact joined boundaries, physical apertures, metric fields and early work caps."""

import unittest
from dataclasses import replace
from itertools import pairwise
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.facade_frames import FacadeFrame, frame_surface, region
from test_opening_panels import hits


def closed(test, mesh):
    triangles = mesh["vertices"][mesh["faces"]]
    directed = {}
    for t in triangles:
        for a, b in zip(t, np.roll(t, -1, axis=0), strict=True):
            key = tuple(a), tuple(b)
            directed[key] = directed.get(key, 0) + 1
    for (a, b), count in directed.items():
        test.assertEqual(count, 1)
        test.assertEqual(directed.get((b, a)), 1)
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    np.testing.assert_allclose(
        normal / np.linalg.norm(normal, axis=1, keepdims=True),
        mesh["normals"][mesh["faces"][:, 0]],
    )
    uv = mesh["uv"][mesh["faces"]]
    np.testing.assert_allclose(
        np.linalg.norm(triangles[:, 1:] - triangles[:, :1], axis=2),
        np.linalg.norm(uv[:, 1:] - uv[:, :1], axis=2),
        atol=1e-12,
    )
    return float(
        np.einsum(
            "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
        ).sum()
        / 6
    )


class FacadeFrames(unittest.TestCase):
    def test_exact_closed_oriented_edges_volume_and_metric_uv(self):
        for width, height in ((8.0, 6.0), (19.137, 22.683), (4.0, 2.5)):
            plan = FacadeFrame(
                width,
                height,
                (0.12, width / 2, width - 0.12),
                (0.11, height / 3, height * 2 / 3, height - 0.11),
            )
            mesh = frame_surface(plan, material=19)
            void_area = sum(
                (b - a - plan.pier_m) * (d - c - plan.beam_m)
                for a, b in pairwise(plan.vertical)
                for c, d in pairwise(plan.horizontal)
            )
            expected = (width * height - void_area) * plan.depth_m
            self.assertAlmostEqual(closed(self, mesh), expected, places=10)
            self.assertAlmostEqual(
                region(plan).area * plan.depth_m, expected, places=10
            )
            self.assertEqual(len(mesh["faces"]), plan.triangle_count())
            np.testing.assert_array_equal(mesh["materials"], 19)

    def test_apertures_are_real_through_openings_and_crossings_are_solid(self):
        plan = FacadeFrame(8, 6, (0.12, 4, 7.88), (0.11, 3, 5.89))
        mesh = frame_surface(plan)
        for a, b in pairwise(plan.vertical):
            for c, d in pairwise(plan.horizontal):
                self.assertEqual(len(hits(mesh, ((a + b) / 2, (c + d) / 2))), 0)
        for x in plan.vertical:
            for y in plan.horizontal:
                np.testing.assert_allclose(hits(mesh, (x, y)), [0, plan.depth_m])

    def test_covered_back_omits_only_contact_faces_before_materialization(self):
        plan = FacadeFrame(8, 6, (0.12, 4, 7.88), (0.11, 3, 5.89))
        full = frame_surface(plan)
        outer = frame_surface(plan, back_covered=True)
        selected = full["roles"] != 1
        np.testing.assert_array_equal(
            full["vertices"][full["faces"][selected]], outer["vertices"][outer["faces"]]
        )
        np.testing.assert_array_equal(
            full["normals"][full["faces"][selected]], outer["normals"][outer["faces"]]
        )
        np.testing.assert_array_equal(
            full["uv"][full["faces"][selected]], outer["uv"][outer["faces"]]
        )
        self.assertEqual(len(outer["faces"]), plan.triangle_count(True))
        self.assertNotIn(1, outer["roles"])

    def test_plan_rejects_collapsed_axes_and_unbounded_or_mutable_inputs(self):
        plan = FacadeFrame(8, 6, (0.12, 4, 7.88), (0.11, 3, 5.89))
        for changes in (
            {"depth_m": 0},
            {"width_m": float("nan")},
            {"vertical": [0.12, 7.88]},
            {"vertical": (0.12, 0.24, 7.88)},
            {"horizontal": (0.2, 3, 5.89)},
        ):
            with self.assertRaises(ValueError):
                replace(plan, **changes)
        with self.assertRaisesRegex(ValueError, "256"):
            FacadeFrame(
                100, 100, (0.12, *range(1, 32), 99.88), (0.11, *range(1, 32), 99.89)
            )

    def test_capacity_and_binding_fail_before_front_triangulation(self):
        plan = FacadeFrame(8, 6, (0.12, 4, 7.88), (0.11, 3, 5.89))
        with patch(
            "outshine_ref.buildings.facade_frames.compile_faces",
            side_effect=AssertionError("expanded"),
        ):
            for options in (
                {"max_triangles": 1},
                {"max_triangles": True},
                {"material": 65536},
            ):
                with self.assertRaises(ValueError):
                    frame_surface(plan, **options)
            with self.assertRaises(TypeError):
                frame_surface(plan, back_covered=1)


if __name__ == "__main__":
    unittest.main()

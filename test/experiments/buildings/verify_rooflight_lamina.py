"""Actual material surfaces, geometric projection bounds and retained-body demand."""

import unittest
from unittest.mock import patch

import numpy as np
import trimesh
from demand import View, displacement_error
from outshine_ref.buildings.rooflight_panels import closed, corners, sheets
from recipe_hall_operators import inputs_for
from recipe_rooflight_operators import prepare
from rooflight_lamina import DISTANCE_BOUND_M, assemble, panels, selection, template
from structural_shell_geometry import native
from trimesh.proximity import closest_point_naive
from verify_industrial_halls import sources, view_for


def far_view(row, *, factor=8, movement=0):
    original = view_for(row)
    return View(
        original.target + (original.eye - original.target) * factor,
        original.target,
        width=1920,
        height=1080,
        movement_m=movement,
    )


def material_samples(mesh, material):
    triangles = mesh["vertices"][mesh["faces"][mesh["materials"] == material]]
    return np.concatenate(
        [
            triangles.mean(axis=1),
            (triangles + np.roll(triangles, -1, axis=1)).reshape(-1, 3) / 2,
        ]
    )


def closest(mesh, material, samples):
    faces = mesh["faces"][mesh["materials"] == material]
    if not len(faces):
        raise ValueError("Missing actual material surface")
    value = trimesh.Trimesh(vertices=mesh["vertices"], faces=faces, process=False)
    return closest_point_naive(value, samples)[:2]


def distance_check(full, lamina):
    for first, last in ((full, lamina), (lamina, full)):
        for material, bound in ((10, DISTANCE_BOUND_M), (3, 0.003)):
            samples = material_samples(first, material)
            _, distances = closest(last, material, samples)
            if distances.max() > bound + 1e-8:
                raise ValueError("Actual material surface exceeds its geometric bound")


def project(points, view):
    q = (points - view.eye) @ view.basis.T
    return q[:, :2] * view.focal_px / q[:, 2:3]


class RooflightLamina(unittest.TestCase):
    def test_batched_projection_matches_scalar_reference_for_actual_views(self):
        self.assertEqual(selection((), View(np.array([0.0, -1, 1]), np.zeros(3))), ())
        for row, neighbors in sources()[:3]:
            fields = (
                prepare()
                .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
                .outputs["rooflights"]
                .value
            )
            for factor, movement in ((1, 0), (4, 5), (1, 10000)):
                view = far_view(row, factor=factor, movement=movement)
                actual = selection(fields, view)
                for field, result in zip(fields, actual, strict=True):
                    top = corners(field)
                    expected = displacement_error(
                        np.r_[top, top - [0, 0, 0.12]], view, DISTANCE_BOUND_M
                    )
                    if np.isfinite(expected):
                        self.assertAlmostEqual(
                            result["geometry_bound_px"], expected, places=12
                        )
                    else:
                        self.assertIsNone(result["geometry_bound_px"])
                    self.assertEqual(result["lamina"], expected <= view.detail_error_px)

    def test_all_real_fields_have_bounded_actual_material_surfaces_both_directions(
        self,
    ):
        count = 0
        for row, neighbors in sources()[:3]:
            fields = (
                prepare()
                .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
                .outputs["rooflights"]
                .value
            )
            compact, chosen = panels(fields, far_view(row))
            public = sheets(fields)
            self.assertTrue(all(p["lamina"] for p in chosen))
            for full, reduced, emitted in zip(
                closed(fields), compact, public, strict=True
            ):
                distance_check(full, reduced)
                for key in reduced:
                    self.assertEqual(reduced[key].dtype, emitted[key].dtype)
                    np.testing.assert_array_equal(reduced[key], emitted[key])
                self.assertEqual(len(reduced["faces"]), 20)
                self.assertEqual(np.count_nonzero(reduced["materials"] == 3), 4)
                count += 1
        self.assertEqual(count, 117)

    def test_shifted_missing_or_misbound_glass_does_not_pass_surface_check(self):
        row, neighbors = sources()[2]
        fields = (
            prepare()
            .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
            .outputs["rooflights"]
            .value
        )
        full = closed(fields[:1])[0]
        compact = panels(fields[:1], far_view(row))[0][0]
        variants = (
            dict(compact, vertices=compact["vertices"] + [0, 0, 0.3]),
            dict(
                compact,
                faces=compact["faces"][:-4],
                materials=compact["materials"][:-4],
            ),
            dict(compact, materials=np.full(20, 10, dtype=np.uint16)),
        )
        for changed in variants:
            with self.assertRaises(ValueError):
                distance_check(full, changed)

    def test_projected_bound_holds_for_actual_matches_and_camera_movement(self):
        row, neighbors = sources()[2]
        fields = (
            prepare()
            .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
            .outputs["rooflights"]
            .value
        )
        view = far_view(row, factor=4, movement=5)
        reduced, chosen = panels(fields, view)
        for item, full, compact, choice in zip(
            fields, closed(fields), reduced, chosen, strict=True
        ):
            self.assertTrue(choice["lamina"])
            samples = material_samples(full, 10)
            matches, _ = closest(compact, 10, samples)
            for offset in ([0, 0, 0], [5, 0, 0], [0, -5, 0], [0, 0, 5]):
                moved = View(view.eye + offset, view.target, width=1920, height=1080)
                # Match the original orientation: movement is translation, not a free rotation.
                shifted = View(
                    view.eye + offset, view.target + offset, width=1920, height=1080
                )
                actual = np.linalg.norm(
                    project(samples, shifted) - project(matches, shifted), axis=1
                )
                self.assertLessEqual(actual.max(), choice["geometry_bound_px"] + 1e-8)
                self.assertGreater(
                    np.min((corners(item) - moved.eye) @ moved.basis.T[:, 2]), 0
                )

    def test_near_plane_or_motion_restores_full_part_before_expansion(self):
        row, neighbors = sources()[2]
        fields = (
            prepare()
            .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
            .outputs["rooflights"]
            .value
        )
        centre = corners(fields[0]).mean(axis=0)
        view = View(centre + [0, 0, 0.01], centre, width=1920, height=1080)
        decision = selection(fields[:1], view)[0]
        self.assertFalse(decision["lamina"])
        self.assertIsNone(decision["geometry_bound_px"])
        with patch(
            "rooflight_lamina.template",
            side_effect=AssertionError("unrequested lamina"),
        ):
            part, _ = panels(fields[:1], view)
        self.assertEqual(len(part[0]["faces"]), 44)
        far = far_view(row)
        unsafe = far_view(row, movement=float(np.linalg.norm(far.eye - centre)))
        self.assertFalse(selection(fields[:1], unsafe)[0]["lamina"])

    def test_far_never_builds_full_panels_and_capacity_rejects_before_expansion(self):
        row, neighbors = sources()[1]
        fields = (
            prepare()
            .evaluate(inputs_for(row, neighbors), outputs=("rooflights",))
            .outputs["rooflights"]
            .value
        )
        with patch(
            "rooflight_lamina.closed_template",
            side_effect=AssertionError("unrequested full panel"),
        ):
            value, chosen = panels(fields, far_view(row))
        self.assertTrue(all(item["lamina"] for item in chosen))
        self.assertEqual(sum(len(p["faces"]) for p in value), 20 * len(fields))
        with self.assertRaisesRegex(ValueError, "capacity"):
            sheets([fields[0]] * 65)
        with patch(
            "rooflight_lamina.template",
            side_effect=AssertionError("unrequested expansion"),
        ):
            for invalid in ([fields[0]] * 65, [fields[0]] * 2):
                with self.assertRaisesRegex(ValueError, "bounded unique"):
                    panels(invalid, far_view(row))

    def test_sheets_are_not_colliders_and_templates_keep_typed_immutable_storage(self):
        part = template(1.0, 4.0, -0.3)
        with self.assertRaises(ValueError):
            native(part)
        with self.assertRaises(TypeError):
            part["materials"] = np.zeros(20, dtype=np.uint16)
        self.assertTrue(all(not a.flags.writeable for a in part.values()))
        for bad in ((True, 4, -0.3), (0.01, 4, -0.3), (1, 4, float("nan"))):
            with self.assertRaises(ValueError):
                template(*bad)
        self.assertLessEqual(template.cache_info().currsize, 32)

    def test_retained_physical_body_unchanged_and_never_rebuilt_for_render_lod(self):
        row, neighbors = sources()[2]
        run = prepare().evaluate(
            inputs_for(row, neighbors), outputs=("programme", "rooflights", "shell")
        )
        body = run.outputs["shell"].value.body
        before = {k: v.copy() for k, v in body.items()}
        with (
            patch(
                "recipe_rooflight_operators.structure",
                side_effect=AssertionError("repeated CSG"),
            ),
            patch(
                "recipe_rooflight_operators.plan",
                side_effect=AssertionError("repeated plan"),
            ),
        ):
            for factor in (1, 8):
                mesh, _, _ = assemble(run, far_view(row, factor=factor), opened=0.5)
                self.assertTrue(np.isfinite(mesh["vertices"]).all())
        for k, v in body.items():
            np.testing.assert_array_equal(v, before[k])


if __name__ == "__main__":
    unittest.main()

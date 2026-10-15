"""Actual source shells keep far output, source bounds and existing facade axes."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from provider_contract import attributes
from tile_facade_recess import attach, recess_error
from tile_facades import project
from tile_roof_envelopes import choose
from verify_roof_envelopes import case
from verify_urban_roofs import sources


def source_facades(row):
    p = attributes(row["properties"])
    choice = choose(row, p)
    _, envelope, _, _ = case(row, p, choice)
    mesh = {
        k: envelope.mesh[k] for k in ("vertices", "faces", "normals", "uv", "materials")
    }
    mesh, selected, facades = project(
        mesh, row, p | {"eave_m": choice.eave_m}, (0.48, 0.53), 3.2, 2.9
    )
    mesh["materials"][selected] = 32768
    return mesh, facades, p


def camera(frame, distance):
    a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
    normal = np.array([(b - a)[1], -(b - a)[0], 0.0]) / frame["width_m"]
    target = np.r_[(a + b) / 2, frame["height_m"] / 2]
    return View(target + normal * distance, target, width=1920, height=1080, fov_deg=48)


class RecessConsumers(unittest.TestCase):
    def test_directional_error_bounds_dense_independent_projections_and_movement(self):
        corners = np.array(
            [[0.0, 0.0, 0.0], [12.0, 0.0, 0.0], [12.0, 0.0, 9.0], [0.0, 0.0, 9.0]]
        )
        normal = np.array([0.0, -1.0, 0.0])
        x, z = np.meshgrid(np.linspace(0, 12, 21), np.linspace(0, 9, 21))
        points = np.c_[x.ravel(), np.zeros(x.size), z.ravel()]
        rng = np.random.default_rng(318)
        moves = rng.normal(size=(128, 3))
        moves *= 0.4 / np.linalg.norm(moves, axis=1, keepdims=True)
        for eye in ([6.0, -25.0, 4.5], [25.0, -9.0, 6.0], [4.0, -30.0, 20.0]):
            view = View(
                np.array(eye),
                np.array([6.0, 0.0, 4.5]),
                width=1920,
                height=1080,
                fov_deg=48,
                movement_m=0.4,
            )
            bound = recess_error(corners, normal, view)
            for move in moves:
                original = (points - view.eye - move) @ view.basis.T
                recessed = (points - 0.18 * normal - view.eye - move) @ view.basis.T
                before = original[:, :2] / original[:, 2:3] * view.focal_px
                after = recessed[:, :2] / recessed[:, 2:3] * view.focal_px
                self.assertLessEqual(
                    float(np.linalg.norm(after - before, axis=1).max()), bound + 1e-10
                )

    def test_front_on_projection_avoids_unnecessary_geometry_without_changing_budget(
        self,
    ):
        corners = np.array(
            [[0.0, 0.0, 0.0], [4.0, 0.0, 0.0], [4.0, 0.0, 4.0], [0.0, 0.0, 4.0]]
        )
        view = View(
            np.array([2.0, -100.0, 2.0]),
            np.array([2.0, 0.0, 2.0]),
            width=1920,
            height=1080,
            fov_deg=48,
        )
        bound = recess_error(corners, np.array([0.0, -1.0, 0.0]), view)
        old = (
            view.focal_px
            * 0.18
            * np.sqrt(
                1
                + (view.width / (2 * view.focal_px)) ** 2
                + (view.height / (2 * view.focal_px)) ** 2
            )
            / (100 - 0.18)
        )
        self.assertLess(bound, view.detail_error_px)
        self.assertGreater(old, view.detail_error_px)
        self.assertLess(bound, old / 10)
        crossing = View(
            np.array([2.0, -0.1, 2.0]), np.array([2.0, 0.0, 2.0]), movement_m=0.2
        )
        self.assertTrue(
            np.isinf(recess_error(corners, np.array([0.0, -1.0, 0.0]), crossing))
        )

    def test_actual_sources_near_recesses_stay_inside_footprint_and_source_heights(
        self,
    ):
        for row in sources():
            original = copy.deepcopy(row)
            mesh, facades, p = source_facades(row)
            frame = max(facades, key=lambda f: f["width_m"])
            output, records, _ = attach(
                mesh,
                row,
                p,
                facades,
                camera(frame, 25),
                lambda: 32769,
                32768,
                plain_wall_material=1,
            )
            self.assertTrue(records)
            from blender_city_mesh import cell_sizes

            dimensions = cell_sizes(output, (32768, 32769))
            self.assertTrue(np.isfinite(dimensions).all())
            np.testing.assert_allclose(
                output["vertices"][:, 2].min(), p["minHeight"], atol=1e-12
            )
            np.testing.assert_allclose(
                output["vertices"][:, 2].max(), p["buildingHeight"], atol=1e-12
            )
            self.assertTrue(
                shapely.covers(
                    row["polygon"].buffer(1e-8),
                    shapely.points(output["vertices"][:, :2]),
                ).all()
            )
            np.testing.assert_allclose(
                np.linalg.norm(output["normals"], axis=1), 1, atol=1e-12
            )
            self.assertEqual(row, original)
            by_id = {f["id"]: f for f in facades}
            for record in records:
                self.assertEqual(record["cells"], by_id[record["id"]]["opening_count"])

    def test_far_keeps_exact_input_without_cell_or_material_expansion(self):
        row = sources()[0]
        mesh, facades, p = source_facades(row)
        view = camera(max(facades, key=lambda f: f["width_m"]), 100000)
        with patch(
            "tile_facade_recess.recess_surface",
            side_effect=AssertionError("far expansion"),
        ):
            output, records, _ = attach(
                mesh,
                row,
                p,
                facades,
                view,
                lambda: self.fail("far material allocation"),
                32768,
                plain_wall_material=1,
            )
        self.assertIs(output, mesh)
        self.assertEqual(records, [])

    def test_far_demand_reads_no_geometry_buffers(self):
        class UnreadableGeometry(dict):
            def __getitem__(self, key):
                raise AssertionError("Far demand read a geometry buffer: " + key)

        row = sources()[0]
        _, facades, p = source_facades(row)
        guarded = UnreadableGeometry()
        view = camera(max(facades, key=lambda f: f["width_m"]), 100000)
        output, records, _ = attach(
            guarded,
            row,
            p,
            facades,
            view,
            lambda: self.fail("far material"),
            32768,
            plain_wall_material=1,
        )
        self.assertIs(output, guarded)
        self.assertFalse(records)

    def test_capacity_fallback_retains_complete_painted_wall(self):
        row = sources()[1]
        mesh, facades, p = source_facades(row)
        output, records, rejected = attach(
            mesh,
            row,
            p,
            facades,
            camera(facades[0], 25),
            lambda: self.fail("capacity allocated"),
            32768,
            plain_wall_material=1,
            max_cells=0,
        )
        self.assertIs(output, mesh)
        self.assertFalse(records)
        self.assertTrue(rejected)


if __name__ == "__main__":
    unittest.main()

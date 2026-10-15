"""Actual source shells keep far output, source bounds and existing facade axes."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from demand import View
from provider_contract import attributes
from tile_facade_recess import attach
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

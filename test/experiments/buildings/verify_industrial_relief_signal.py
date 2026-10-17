"""Real distant material surfaces, independent distances and preserved source/physics."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
import shapely
from demand import View, displacement_error
from industrial_relief_mesh import cap_prefix
from industrial_relief_products import ArticulatedDetails, demanded
from industrial_relief_signal import SignalDetails, direct_caps
from industrial_relief_signal_probe import OmittedRelief
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from structural_shell_geometry import native
from tile_buildings import construct
from verify_recipe_halls import mesh_equal
from verify_rooflight_lamina import closest, material_samples, project


class IndustrialReliefSignal(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rows, cls.receipt, _context, cls.view = source_tile()
        cls.assets = prepare_halls(rows)
        cls.owner = SignalDetails(cls.assets, cls.view)

    def test_all_actual_caps_equal_direct_emission_and_share_the_physical_buffers(self):
        count = 0
        for parts in self.owner.reliefs.values():
            for part in parts:
                cap = cap_prefix(part)
                mesh_equal(cap, direct_caps(part.plan))
                self.assertEqual(native(part.full).status(), manifold.Error.NoError)
                with self.assertRaisesRegex(ValueError, "NotManifold"):
                    native(cap)
                for key in cap:
                    self.assertTrue(np.shares_memory(cap[key], part.full[key]))
                    self.assertFalse(cap[key].flags.writeable)
                self.assertEqual(set(cap["materials"]), {10})
                count += 1
        self.assertEqual(count, 39)

    def test_actual_material_surfaces_remain_within_the_physical_depth_both_ways(self):
        for parts in self.owner.reliefs.values():
            for part in parts:
                cap = cap_prefix(part)
                for first, last in ((part.outer, cap), (cap, part.outer)):
                    _points, distances = closest(last, 10, material_samples(first, 10))
                    self.assertLessEqual(distances.max(), part.plan.depth_m + 1e-8)
                wrong = dict(
                    cap, materials=np.full(len(cap["faces"]), 1, dtype=np.uint16)
                )
                with self.assertRaisesRegex(ValueError, "Missing actual material"):
                    closest(wrong, 10, material_samples(part.outer, 10))

    def test_side_to_front_correspondence_has_bounded_projection_under_movement(self):
        for parts in self.owner.reliefs.values():
            for part in parts:
                p = part.plan
                tangent = np.r_[p.tangent, 0]
                normal = np.array([p.tangent[1], -p.tangent[0], 0])
                samples = material_samples(part.full, 10)
                depth = (samples - [*p.origin, 0]) @ normal
                matched = samples + (p.depth_m - depth)[:, None] * normal
                local = np.c_[
                    (matched[:, :2] - p.origin) @ np.asarray(p.tangent),
                    matched[:, 2],
                ]
                distances = shapely.distance(p.region, shapely.points(local))
                roundoff = 128 * np.finfo(float).eps * max(1, abs(samples).max())
                self.assertLessEqual(distances.max(), roundoff)
                centre = part.wall_bounds.mean(axis=0)
                for direction in (
                    normal * 1000 + [0, 0, 100],
                    tangent * 1000 + normal * 80 + [0, 0, 100],
                ):
                    view = View(
                        centre + direction,
                        centre,
                        width=1920,
                        height=1080,
                        movement_m=2,
                    )
                    bound = displacement_error(part.wall_bounds, view, p.depth_m)
                    for delta in (np.zeros(3), tangent * 1.5, normal * -1.5):
                        actual = replace(
                            view, eye=view.eye + delta, target=view.target + delta
                        )
                        error = np.linalg.norm(
                            project(samples, actual) - project(matched, actual), axis=1
                        )
                        self.assertLessEqual(error.max(), bound + 1e-8)

    def test_camera_and_angle_changes_select_existing_faces_without_mesh_work(self):
        identifier = next(iter(self.owner.reliefs))
        part = self.owner.reliefs[identifier][0]
        centre = part.wall_bounds.mean(axis=0)
        physical = tuple(p.full for p in self.owner.reliefs[identifier])
        with (
            patch(
                "industrial_relief_signal.direct_caps",
                side_effect=AssertionError("warm cap generation"),
            ),
            patch(
                "industrial_relief_products.products",
                side_effect=AssertionError("warm physical generation"),
            ),
        ):
            for eye in (
                centre + [5, -8, 2],
                centre + [10000, -10000, 10000],
                centre + [-10000, 10000, 10000],
            ):
                self.owner.view = View(eye, centre, width=1920, height=1080)
                chosen = self.owner.draw_parts(identifier)
                for actual, p, far in zip(
                    chosen,
                    self.owner.reliefs[identifier],
                    self.owner.far_parts[identifier],
                    strict=True,
                ):
                    self.assertIs(
                        actual, p.outer if demanded(p, self.owner.view) else far
                    )
                self.assertEqual(
                    tuple(id(p.full) for p in self.owner.reliefs[identifier]),
                    tuple(map(id, physical)),
                )

    def test_the_complete_tile_keeps_every_material_region_and_source(self):
        self.owner.view = self.view
        baseline = OmittedRelief(self.assets, self.view)
        previous, old = construct(
            "Grohnde",
            prepared=(self.assets.rows, self.receipt),
            ordinary_builder=baseline,
        )
        mesh, source = construct(
            "Grohnde",
            prepared=(self.assets.rows, self.receipt),
            ordinary_builder=self.owner,
        )
        self.assertEqual(source["generated"], 37)
        self.assertEqual(source["omissions"], [])
        for key in (
            "counts",
            "material_definitions",
            "generated",
            "delivered_building_polygons",
        ):
            self.assertEqual(source[key], old[key])
        for before, after in zip(old["members"], source["members"], strict=True):
            for key in before.keys() - {"prototype"}:
                self.assertEqual(before[key], after[key])
        self.assertGreater(len(mesh["faces"]), len(previous["faces"]))
        self.assertLess(len(mesh["faces"]), 88649)
        preferred = ArticulatedDetails(self.assets, self.view)
        preferred_mesh, _ = construct(
            "Grohnde",
            prepared=(self.assets.rows, self.receipt),
            ordinary_builder=preferred,
        )
        mesh_equal(mesh, preferred_mesh)
        self.assertEqual(
            sum(p["rendered_facades"] for p in self.owner.relief_draws[-8:]), 39
        )

    def test_projected_alternative_and_invalid_offsets_keep_exact_surface_semantics(
        self,
    ):
        part = next(iter(self.owner.reliefs.values()))[0]
        cap, projected = cap_prefix(part), direct_caps(part.plan, offset_m=0.004)
        for key in cap.keys() - {"vertices"}:
            np.testing.assert_array_equal(projected[key], cap[key])
        self.assertAlmostEqual(
            float(
                np.linalg.norm(projected["vertices"] - cap["vertices"], axis=1).max()
            ),
            part.plan.depth_m - 0.004,
            places=10,
        )
        for invalid in (True, float("nan"), -1, part.plan.depth_m + 1):
            with self.assertRaises(ValueError):
                direct_caps(part.plan, offset_m=invalid)
        with patch(
            "industrial_relief_products.products",
            side_effect=AssertionError("invalid heavy work"),
        ):
            for invalid in (True, "unknown"):
                with self.assertRaises(ValueError):
                    SignalDetails(self.assets, self.view, method=invalid)


if __name__ == "__main__":
    unittest.main()

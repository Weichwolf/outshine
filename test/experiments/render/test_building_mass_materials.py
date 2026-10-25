"""Analytic surface measures, local failures and independently mixed GGX controls."""

import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from build_building_mass import cook as geometry_cook
from build_mass_appearance import cook
from building_mass_appearance import build_appearance, source_measure
from building_mass_lighting import shade
from building_mass_material_image import samples
from building_mass_materials import material, policy
from building_material_reference import face_materials
from parent_surface_inputs import ParentInputs


def surface(colour=None):
    return {
        "origin": [0, 0, 0],
        "east": [1, 0, 0],
        "north": [0, 1, 0],
        "up": [0, 0, 1],
        "minimum_height_m": 0,
        "wall_colour": colour,
        "shapes": [
            {
                "ring": [[0, 0], [2, 0], [2, 3], [0, 3]],
                "holes": [],
                "party_wall_edges": [0] * 4,
                "front_edge": 0,
                "roof_kind": 0,
                "ident": 0,
                "opening_style": 0,
                "wall_variant": 0,
                "seat_m": 0,
                "foot_m": 0,
                "floor_m": 3,
                "bay_m": 3,
                "sole_m": 0,
                "eaves_m": 5,
                "rise_m": 0,
            }
        ],
    }


def geometry():
    return {
        "basis": np.eye(3),
        "anchor": np.zeros(3),
        "selected_sources": np.array([1, 2]),
        "group_sources": np.array([1, 2]),
        "group_offsets": np.array([0, 2]),
    }


class BuildingMassMaterials(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.at = policy()

    def test_constant_box_surface_measures_and_rigid_frame_invariance(self):
        value, _, _ = source_measure(1, surface([0.2, 0.3, 0.4]), geometry(), self.at)
        np.testing.assert_allclose(value[:, -1], [50, 6], rtol=0, atol=1e-12)
        np.testing.assert_allclose(
            value[0, :3], np.array([0.2, 0.3, 0.4]) * 50, atol=1e-12, rtol=0
        )
        np.testing.assert_allclose(
            value[:, 3:-1].sum(axis=1), [50, 6], atol=1e-12, rtol=0
        )
        moved = geometry() | {
            "basis": np.array([[0, 1, 0], [-1, 0, 0], [0, 0, 1]]),
            "anchor": np.array([10, 20, 30]),
        }
        other, _, _ = source_measure(1, surface([0.2, 0.3, 0.4]), moved, self.at)
        np.testing.assert_allclose(value, other, rtol=0, atol=1e-12)

    def test_window_mean_conserves_paint_frame_pane_area_without_roughness_average(
        self,
    ):
        colour, weights = material(self.at, 1, 0, np.ones(3), storeys_per_m=1 / 3)
        fx, fy = self.at["fractions"][1]
        pane = (fx - 0.04 - 0.012) * (fy - 0.04)
        expected = (
            self.at["base"][0] * (1 - fx * fy - 0.35 * 0.16 / 3)
            + np.array([0.48, 0.46, 0.41]) * (fx * fy - pane)
            + np.array([0.025, 0.035, 0.045]) * pane
        )
        np.testing.assert_allclose(colour, expected, atol=2e-9, rtol=0)
        self.assertAlmostEqual(weights.sum(), 1)
        self.assertEqual(np.count_nonzero(weights), 3)
        self.assertAlmostEqual(
            weights[np.flatnonzero(self.at["palette"] == np.float32(0.12))[0]],
            pane,
            places=8,
        )
        _, closed = material(self.at, 1, 0, np.ones(3), openings=False)
        self.assertEqual(np.count_nonzero(closed), 1)

    def test_group_uses_surface_area_and_missing_source_rejects_entire_group(self):
        a, b = surface([0.2, 0.3, 0.4]), surface([0.8, 0.6, 0.4])
        b["shapes"][0]["eaves_m"] = 10
        parent = SimpleNamespace(
            iter_forms=lambda _: [(33, [1, 2], [a, b])], form_reads=1, form_bytes=0
        )
        arrays, _ = build_appearance(parent, {33}, geometry(), self.at)
        np.testing.assert_allclose(
            arrays["albedo"][0, 0],
            (np.array([0.2, 0.3, 0.4]) + 2 * np.array([0.8, 0.6, 0.4])) / 3,
            atol=3e-8,
            rtol=0,
        )
        parent.iter_forms = lambda _: [(33, [1, 2], [a, None])]
        arrays, report = build_appearance(parent, {33}, geometry(), self.at)
        self.assertFalse(arrays["supported"][0])
        self.assertFalse(arrays["albedo"].any())
        self.assertEqual([n["source"] for n in report["source_needs"]], [2])

    def test_diffuse_mean_plus_roughness_measure_matches_independent_lit_mixture(self):
        tables = {
            "shape": (2, 2),
            "kGgxAlbedo": np.ones(4),
            "kGgxAlbedoAverage": np.ones(2),
        }
        normal = np.array([[0, 0, 1.0]])
        a, b = np.array([[0.1, 0.2, 0.3]]), np.array([[0.8, 0.6, 0.4]])

        def lighting(colour, weights, palette):
            return shade(
                colour, weights, np.array(palette), normal, normal, [0, 0, 1], tables
            )[0]

        expected = 0.2 * lighting(a, [[1]], [0.12]) + 0.8 * lighting(b, [[1]], [0.88])
        mean = 0.2 * a + 0.8 * b
        np.testing.assert_allclose(
            lighting(mean, [[0.2, 0.8]], [0.12, 0.88]), expected, atol=1e-13, rtol=1e-14
        )
        naive = lighting(mean, [[1]], [0.2 * 0.12 + 0.8 * 0.88])
        self.assertGreater(float(abs(expected - naive).max()), 0.1)
        with self.assertRaises(ValueError):
            shade(a, [[1]], [0.12], normal, normal, [0, 0, 0], tables)

    def test_original_affine_uv_gradient_and_unknown_paint_are_explicit(self):
        source = {
            "positions": np.array([[0.0, 0, 0], [1, 0, 0], [0, 1, 0]]),
            "indices": np.array([[0, 1, 2]]),
            "uv": np.array([[0.0, 1], [0, 3], [0, 4]]),
            "colour": np.ones((3, 4)),
            "binding": np.zeros((1, 5), int),
        }
        fields, needs = face_materials(source, self.at)
        self.assertEqual(needs, [])
        np.testing.assert_array_equal(fields["uv_gradient"], [[2, 3, 0]])
        source["colour"][1, 0] = 0.5
        fields, needs = face_materials(source, self.at)
        self.assertFalse(fields["supported"][0])
        self.assertIn("nonconstant original paint", needs[0]["reason"])

    def test_actual_generator_reads_only_index_and_selected_source_block(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        product, _ = geometry_cook(path, {33})
        original, reads, parents = ParentInputs.member, [], []

        def member(parent, name):
            self.assertIn(name, {parent.metadata["index"], parent.forms[33]})
            reads.append(name)
            if not parents:
                parents.append(parent)
            return original(parent, name)

        with (
            patch.object(ParentInputs, "member", member),
            patch(
                "building_mass_appearance.source_measure", wraps=source_measure
            ) as measured,
        ):
            arrays, report = cook(path, {33}, product, self.at)
        self.assertEqual(len(reads), 2)
        self.assertNotIn("member", vars(parents[0]))
        self.assertEqual(report["supported_groups"], len(product["group_offsets"]) - 1)
        self.assertEqual(report["form_reads"], 1)
        self.assertTrue(arrays["supported"].all())
        self.assertEqual(report["sources_with_measures"], len(product["group_sources"]))
        self.assertEqual(
            report["retained_sources_without_new_material_work"],
            len(product["needs_sources"]),
        )
        called = [call.args[0] for call in measured.call_args_list]
        self.assertEqual(len(called), len(product["group_sources"]))
        self.assertEqual(set(called), set(product["group_sources"]))
        self.assertTrue(set(called).isdisjoint(product["needs_sources"]))
        np.testing.assert_allclose(
            arrays["roughness_weights"].sum(axis=2), 1, rtol=0, atol=2e-7
        )

    def test_unavailable_remainder_source_receives_no_material_work(self):
        product = geometry() | {
            "selected_sources": np.array([1, 2, 3]),
            "group_sources": np.array([1, 3]),
        }
        parent = SimpleNamespace(
            iter_forms=lambda _: [(33, [1, 2, 3], [surface(), None, surface()])],
            form_reads=1,
            form_bytes=0,
        )
        with patch(
            "building_mass_appearance.source_measure", wraps=source_measure
        ) as measured:
            arrays, report = build_appearance(parent, {33}, product, self.at)
        self.assertEqual([call.args[0] for call in measured.call_args_list], [1, 3])
        self.assertEqual(report["sources_with_measures"], 2)
        self.assertEqual(report["retained_sources_without_new_material_work"], 1)
        self.assertEqual(report["source_needs"], [])
        self.assertTrue(arrays["supported"].all())

    def test_perspective_sample_points_stay_on_the_original_triangle_plane(self):
        mesh = {
            "positions": np.array([[0.0, 0, -2], [1, 0, -2], [0, 1, -2]]),
            "indices": np.array([[0, 1, 2]]),
        }
        view = (np.zeros(3), np.diag([1, 1, -1]), 4, 4, 4)
        data = samples(mesh, view, np.array([[0, 0], [4, 4]]), 2)
        self.assertGreater(len(data["points"]), 0)
        np.testing.assert_allclose(data["points"][:, 2], -2, rtol=0, atol=1e-14)
        self.assertTrue(np.all(data["points"][:, :2] >= 0))
        self.assertTrue(np.all(data["points"][:, :2].sum(axis=1) <= 1 + 1e-14))


if __name__ == "__main__":
    unittest.main()

"""Flower differentials, attachments, cached organ replay and short-scale fields."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import (
    Blade,
    GroundCoverPlan,
    SpeciesHeightMixture,
)
from species_competition import CompetingMarks
from species_spatial import FourierMarks
from understory_mixture import cover_keys, prepare, selection, shuffled_control
from understory_plan import UPRIGHT, LilyStudy, lily_mesh


class UnderstoryChecks(unittest.TestCase):
    def test_signed_keys_and_saved_partial_marking_keep_original_sites(self):
        ids = np.array(((-2, -1, 4), (0, 1, 9), (2, -3, 1)), np.int64)
        np.testing.assert_array_equal(
            cover_keys(ids), ((3, 1, 4), (0, 2, 9), (4, 5, 1))
        )
        with self.assertRaises(ValueError):
            cover_keys(np.array(((np.iinfo(np.int64).min, 0, 0),)))
        sites = GroundCoverPlan(113, 20).sites((-2, -2), (2, 2))
        mixture, fields = prepare()
        full = selection(sites, mixture, fields)
        m = SpeciesHeightMixture.from_recipe(json.loads(json.dumps(mixture.recipe())))
        f = CompetingMarks.from_recipe(json.loads(json.dumps(fields.recipe())))
        subset = {key: value[::-3] for key, value in sites.items()}
        with patch.object(
            CompetingMarks, "prepare", side_effect=AssertionError("field rebuild")
        ):
            part = selection(subset, m, f)
        for key, value in full.items():
            np.testing.assert_array_equal(value[::-3], part[key])
        np.testing.assert_array_equal(full["ids"], sites["ids"])

    def test_shuffled_control_keeps_exact_model_budget_and_zero_strength_skips_fields(
        self,
    ):
        sites = GroundCoverPlan(113, 10).sites((-2, -2), (2, 2))
        mixture, fields = prepare()
        full = selection(sites, mixture, fields)
        shuffled = shuffled_control(full)
        for key in ("ids", "origins"):
            np.testing.assert_array_equal(full[key], shuffled[key])
        np.testing.assert_array_equal(
            np.bincount(full["classes"] * 8 + full["variants"], minlength=24),
            np.bincount(shuffled["classes"] * 8 + shuffled["variants"], minlength=24),
        )
        with patch.object(
            FourierMarks, "gaussian", side_effect=AssertionError("zero-strength field")
        ):
            selection(sites, mixture, fields, strength=0)

    def test_organ_subset_from_cached_parameters_matches_full_without_rng(self):
        plan = LilyStudy(29)
        full, ranges, recipe = plan.mesh()
        saved = json.loads(json.dumps(recipe))
        with patch.object(
            np.random, "Generator", side_effect=AssertionError("cached recipe RNG")
        ):
            for row in reversed(ranges[::3]):
                part, one, _ = lily_mesh(saved, selected=[tuple(row["id"])])
                begin, end = row["vertex_range"]
                np.testing.assert_array_equal(
                    part["vertices"], full["vertices"][begin:end]
                )
                self.assertEqual(len(one), 1)
        with patch.object(
            LilyStudy, "recipe", side_effect=AssertionError("over-budget recipe")
        ):
            self.assertIsNone(plan.mesh(max_organs=1))

    def test_leaf_petiole_nodes_and_pedicel_stem_anchors(self):
        recipe = LilyStudy(51).recipe()
        for leaf in recipe["leaves"]:
            blade, frame = Blade(**leaf["blade"]), np.asarray(leaf["frame"])
            mesh = blade.mesh(12, 2)
            first = (mesh["parameters"][:, 0] == mesh["parameters"][:, 0].min()) & (
                mesh["parameters"][:, 1] == 0
            )
            node = mesh["vertices"][first].mean(0) @ frame.T + leaf["origin"]
            np.testing.assert_allclose(node, leaf["node"], atol=2e-17)
            np.testing.assert_allclose(frame.T @ frame, np.eye(3), atol=2e-14)
        from outshine_ref.vegetation.curve_frame import arc_frame

        expected = (
            arc_frame(
                recipe["stem_length_m"],
                recipe["stem_bend_rad"],
                0,
                np.linspace(0.38, 0.96, 9),
            )[0]
            @ UPRIGHT.T
        )
        np.testing.assert_array_equal(
            expected, [row["node"] for row in recipe["stalks"]]
        )

    def test_explicit_short_stand_frame_keeps_legacy_ecef_recipe_and_values(self):
        old = FourierMarks.prepare(71, count=16)
        self.assertEqual(old.recipe()["format"], "fourier-species-marks-1")
        stand = FourierMarks.prepare(
            71, length_m=2, count=16, coordinate_frame="stand-planar-metres"
        )
        points = np.array(((0, 0, 0), (1, 2, 0), (-3, 2, 0)), float)
        reloaded = FourierMarks.from_recipe(json.loads(json.dumps(stand.recipe())))
        np.testing.assert_array_equal(stand.gaussian(points), reloaded.gaussian(points))
        with self.assertRaises(ValueError):
            FourierMarks.from_recipe(
                {**old.recipe(), "coordinate_frame": "stand-planar-metres"}
            )
        with self.assertRaises(ValueError):
            CompetingMarks((("a", old, 1), ("b", stand, 2)))


if __name__ == "__main__":
    unittest.main()

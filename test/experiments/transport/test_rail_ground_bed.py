"""Actual rail crossover construction keeps source facts and logical topology."""

import unittest
from dataclasses import replace

import numpy as np
from coincident_ground_sources import prepare_sources
from constructive_earthwork_plane import fit_plane
from ground_contact import maximum_grade
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from rail_ground_bed import construct
from rail_ground_checks import audit
from rail_ground_inputs import load
from rail_ground_profile import bind_profile
from replay_rail_ground_bed import arrays


class RailGroundBedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (
            cls.index,
            cls.cases,
            terrain,
            cls.budget,
            cls.metadata,
            cls.raw,
            cls.manifest,
        ) = load()
        cls.terrain = staticmethod(terrain)
        cls.small = next(c for c in cls.cases if len(c["selected"]) == 7)
        cls.large = next(c for c in cls.cases if len(c["selected"]) == 13)

    def profile(self, case=None):
        return bind_profile(
            self.index,
            (self.small if case is None else case)["selected"],
            self.metadata["source_binding"],
            self.raw,
        )

    def test_bound_construction_retains_original_objects_and_missing_words(self):
        profile = self.profile()
        source = prepare_sources(
            self.index,
            self.small["selected"],
            self.budget,
            construction_grades=profile.grades(),
        )
        self.assertEqual(len(source.members), 7)
        self.assertEqual(len(source.bands), 3)
        for member in source.members:
            self.assertIs(member, self.index[1][member.identifier])
            self.assertIsNone(member.maximum_gradient)
            self.assertEqual(
                member.attributes["recipe"]["native_maximum_gradient_word"], 0
            )
        with self.assertRaisesRegex(ValueError, "cover_ground_profile_required"):
            prepare_sources(self.index, self.small["selected"], self.budget)

    def test_changed_original_source_and_replaced_members_do_not_rebind(self):
        with self.assertRaisesRegex(ValueError, "source_content_mismatch"):
            bind_profile(
                self.index,
                self.small["selected"],
                self.metadata["source_binding"],
                self.raw + b"x",
            )
        profile = self.profile()
        spans = dict(self.index[1])
        member = profile.members[0]
        spans[member.identifier] = replace(member)
        with self.assertRaisesRegex(ValueError, "original_object_binding"):
            profile.check_index((self.index[0], spans, self.index[2]))

    def test_known_profiles_and_unbounded_new_grades_cannot_be_overridden(self):
        spans = dict(self.index[1])
        edge = self.small["selected"][0]
        spans[edge] = replace(spans[edge], maximum_gradient=0.01)
        with self.assertRaises(ValueError):
            bind_profile(
                (self.index[0], spans, self.index[2]),
                self.small["selected"],
                self.metadata["source_binding"],
                self.raw,
            )
        with self.assertRaises(ValueError):
            bind_profile(
                self.index,
                self.small["selected"],
                self.metadata["source_binding"],
                self.raw,
                maximum_gradient=0.03,
            )

    def test_actual_constructed_bed_terrain_and_strict_prior_stay_distinct(self):
        result = construct(self.index, self.profile(), self.terrain, self.budget)
        self.assertEqual(result["state"], "constructed")
        self.assertIsNotNone(result["original_affine_prior_conflict"])
        observations = result["observations"]
        old, old_report = fit_plane(
            observations[:, :2],
            observations[:, 2],
            maximum_gradient=0.025,
            maximum_earthwork_m=0.5,
        )
        self.assertIsNone(old)
        self.assertEqual(old_report["reason"], "construction_earthwork_budget")
        bed, contact = result["bed"], result["terrain"]
        self.assertEqual((len(bed.vertices), len(bed.triangles)), (8, 6))
        self.assertLessEqual(
            maximum_grade(bed.vertices, bed.triangles), 0.025 * 1.0000001
        )
        self.assertLessEqual(
            maximum_grade(
                contact["mesh"].vertices,
                contact["mesh"].triangles[len(bed.triangles) :],
            ),
            (2 / 3) * 1.0000001,
        )
        np.testing.assert_array_equal(
            contact["mesh"].vertices[contact["road_vertex_map"]], bed.vertices
        )
        self.assertTrue(
            all(m.maximum_gradient is None for m in result["source"].members)
        )
        proof = audit(result, self.index, self.profile())
        self.assertEqual(proof["original_end_profiles"], 14)
        self.assertEqual(proof["bed_euler"], 1)
        exported = arrays(result)["constructed_end_sections_xyz"]
        for member, sections in zip(result["source"].members, exported, strict=True):
            axis = positions_in_frame(
                result["anchor"],
                [self.index[0][n] for n in member.nodes],
                np.zeros(2),
            )[:, :2]
            direction = axis[1] - axis[0]
            left = np.array([-direction[1], direction[0]]) / np.linalg.norm(direction)
            expected = axis[:, None, :] + np.array([-1, 1])[None, :, None] * (
                member.width_m / 2 * left
            )
            np.testing.assert_allclose(sections[:, :, :2], expected, atol=1e-12, rtol=0)
            np.testing.assert_array_equal(
                sections[:, :, 2],
                result["plane"].evaluate(expected.reshape(-1, 2)).reshape(2, 2),
            )

    def test_independent_audit_rejects_a_changed_original_direction(self):
        profile = self.profile()
        result = construct(self.index, profile, self.terrain, self.budget)
        bindings = list(result["source"].bindings)
        identity, band, direction = bindings[0]
        bindings[0] = (identity, band, -direction)
        result["source"] = replace(result["source"], bindings=tuple(bindings))
        with self.assertRaisesRegex(ValueError, "original_direction_lost"):
            audit(result, self.index, profile)

    def test_shared_and_per_member_assembly_keep_the_same_complete_product(self):
        profile = self.profile()
        shared = construct(self.index, profile, self.terrain, self.budget, shared=True)
        direct = construct(self.index, profile, self.terrain, self.budget, shared=False)
        for key in ("vertices", "triangles"):
            np.testing.assert_array_equal(
                getattr(shared["bed"], key), getattr(direct["bed"], key)
            )
            np.testing.assert_array_equal(
                getattr(shared["terrain"]["mesh"], key),
                getattr(direct["terrain"]["mesh"], key),
            )
        self.assertEqual(shared["source"].bindings, direct["source"].bindings)
        self.assertEqual(shared["work"]["constructed_band_polygons"], 3)
        self.assertEqual(direct["work"]["constructed_band_polygons"], 7)

    def test_large_original_case_keeps_extent_guard_before_dem_work(self):
        def missing(_points):
            raise AssertionError("DEM work before geometry admission")

        with self.assertRaisesRegex(ValueError, "cover_extent_budget"):
            construct(self.index, self.profile(self.large), missing, self.budget)


if __name__ == "__main__":
    unittest.main()

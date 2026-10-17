"""Actual missing fixed pins and complete old foot-plane protection countercases."""

import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_local_verify import whole_foot_pins
from stair_pier_ground import clipped_planes, fixed, foot_windows, indexed_reference
from stair_support_audit import Reference


class PierGround(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(__file__).with_name("fixtures") / "stair-pier-ground-vienna.npz"
        meta = json.loads(path.with_suffix(".json").read_text())
        if hashlib.sha256(path.read_bytes()).hexdigest() != meta["data_sha256"]:
            raise ValueError("actual pier terrain input changed")
        with np.load(path, allow_pickle=False) as data:
            cls.data = dict(data)
        cls.old = indexed_reference(cls.data["parent_triangles"])
        cls.foot = Mesh(cls.data["foot_vertices"], cls.data["foot_faces"])
        cls.shapes = shapely.polygons(cls.old.vertices[cls.old.triangles, :2])
        cls.region = shapely.union_all(cls.shapes)
        cls.plan = {
            "faces": cls.old.triangles,
            "ground": np.zeros(len(cls.old.triangles), bool),
        }
        cls.state = {"plan": cls.plan, "pier_contacts": [(cls.foot, cls.foot)]}
        cls.chosen = np.ones(len(cls.shapes), bool)

    def test_unindexed_real_reference_loses_pins_but_shared_vertices_retain_them(self):
        tri = self.data["protected_triangles"]
        wrong = Mesh(tri.reshape(-1, 3), np.arange(tri.size // 3).reshape(-1, 3))
        xy = self.data["pin_xy"]
        bad = SurfaceSampler(wrong, xy_tolerance_m=1e-7).sample(xy)
        self.assertEqual(bad.reasons.count("ambiguous_projected_contact"), 2)
        good = SurfaceSampler(indexed_reference(tri), xy_tolerance_m=1e-7).sample(xy)
        self.assertTrue(np.isfinite(good.points_enu_m).all())
        self.assertEqual(good.reasons, ("",) * len(xy))

    def test_clipped_actual_parent_planes_retain_whole_double_float32_contact(self):
        windows = foot_windows(self.state, self.region)
        triangles, note = clipped_planes(
            self.plan, self.old.vertices, self.chosen, self.shapes, windows
        )
        proof = Reference(indexed_reference(triangles)).check(self.foot)
        self.assertLessEqual(proof["double_contact_error_m"], 1e-7)
        self.assertLessEqual(
            proof["float32_contact_error_m"],
            proof["derived_float32_contact_bound_m"] + 1e-10,
        )
        self.assertLess(note["protected_area_m2"], self.region.area / 10)
        self.assertLessEqual(note["protected_pieces"], note["triangle_bound"])

    def test_reader_samples_original_parents_instead_of_retriangulated_references(self):
        values = np.full(len(self.data["pin_xy"]), np.nan)
        whole_foot_pins(
            self.state,
            self.data["pin_xy"],
            self.chosen,
            self.region,
            self.old.vertices,
            values,
        )
        expected = SurfaceSampler(self.old).sample(self.data["pin_xy"])
        np.testing.assert_allclose(
            values, expected.points_enu_m[:, 2], rtol=0, atol=1e-7
        )
        values[0] += 0.001
        with self.assertRaisesRegex(ValueError, "reference heights conflict"):
            whole_foot_pins(
                self.state,
                self.data["pin_xy"],
                self.chosen,
                self.region,
                self.old.vertices,
                values,
            )

    def test_inconsistent_shared_plane_and_unbounded_inputs_are_refused(self):
        triangles = self.data["protected_triangles"].copy()
        raw = triangles.reshape(-1, 3)
        _, counts = np.unique(raw[:, :2], axis=0, return_counts=True)
        self.assertGreater(counts.max(), 1)
        raw[:, 2] += np.arange(len(raw)) * 0.001
        with self.assertRaisesRegex(ValueError, "disagree"):
            indexed_reference(triangles)
        for invalid in (np.zeros((4097, 3, 3)), np.full((1, 3, 3), np.nan)):
            with self.subTest(shape=invalid.shape), self.assertRaises(ValueError):
                indexed_reference(invalid)
        with self.assertRaisesRegex(ValueError, "pier cap"):
            foot_windows(
                dict(self.state, pier_contacts=[(self.foot, self.foot)] * 129),
                self.region,
            )

    def test_default_keeps_full_original_parents_and_unknown_policy_is_refused(self):
        ids = np.arange(len(self.shapes))
        args = self.state, self.chosen, ids, self.shapes, self.region, self.old.vertices
        triangles, refs, note = fixed(*args, {})
        np.testing.assert_array_equal(triangles, self.old.vertices[self.old.triangles])
        np.testing.assert_array_equal(refs[0].triangles, self.old.triangles)
        self.assertEqual(note, {})
        with self.assertRaisesRegex(ValueError, "known pier ground"):
            fixed(*args, {"pier_ground_policy": "ignore_feet"})

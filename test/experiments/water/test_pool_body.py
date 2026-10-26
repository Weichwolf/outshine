"""Closed source-bound pool shell and the two distinct terrain contact roles."""

import json
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.water.frame import model_height_targets
from pool_body import build

FIXTURE = Path(__file__).with_name("fixtures") / "native-bank-contact"


class PoolBody(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = json.loads((FIXTURE / "input.json").read_text())
        cls.source = json.loads((FIXTURE / "source.json").read_text())
        cls.water = np.array(
            [f["vertices"] for f in data["water"]["sourceSurface344Faces"]]
        )
        cls.anchor = data["water"]["meshAnchor"]
        cls.body = build(
            cls.water,
            cls.reference,
            kind="swimming_pool",
            ground_role="declared_ground",
            footprint_role="declared_complete_local_component",
        )

    @classmethod
    def reference(cls, xy):
        return model_height_targets(
            cls.anchor, xy, np.full(len(xy), cls.source["source"]["levelM"])
        )

    def test_indexed_shell_has_two_opposing_incident_faces_at_every_edge(self):
        faces = self.body["faces"]
        directed = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
        edges, inverse, counts = np.unique(
            np.sort(directed, axis=1), axis=0, return_inverse=True, return_counts=True
        )
        np.testing.assert_array_equal(counts, 2)
        signs = np.where(directed[:, 0] < directed[:, 1], 1, -1)
        balance = np.zeros(len(edges), dtype=int)
        np.add.at(balance, inverse, signs)
        np.testing.assert_array_equal(balance, 0)
        p = self.body["vertices"][faces]
        volume = np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
        self.assertGreater(volume, 0)
        self.assertEqual(len(self.body["vertices"]) - len(edges) + len(faces), 2)

    def test_original_water_words_and_corner_order_are_unchanged(self):
        self.assertEqual(self.body["water_triangles"].tobytes(), self.water.tobytes())
        self.assertFalse(self.body["water_triangles"].flags.writeable)
        floor = self.body["contact_faces"][: len(self.water)]
        np.testing.assert_array_equal(self.body["plan_xy"][floor], self.water[:, :, :2])
        np.testing.assert_allclose(
            self.body["contact_vertices"][floor, 2],
            self.water[:, :, 2] - 2.2,
            rtol=0,
            atol=1e-14,
        )

    def test_inner_excavation_and_outer_grade_are_different_actual_body_roles(self):
        b = self.body
        inner, outer = b["inner_ring"], b["outer_ring"]
        np.testing.assert_array_equal(
            b["contact_vertices"][inner, 2], b["bottom_height_m"][inner]
        )
        np.testing.assert_array_equal(
            b["contact_vertices"][outer, 2], b["rim_height_m"][outer]
        )
        self.assertTrue(np.all(b["contact_vertices"][:, 2] >= b["bottom_height_m"]))
        self.assertTrue(np.all(b["contact_vertices"][:, 2] <= b["rim_height_m"]))

    def test_uncharted_roles_and_nonpool_semantics_are_never_ground_pools(self):
        arguments = {
            "kind": "swimming_pool",
            "ground_role": "declared_ground",
            "footprint_role": "declared_complete_local_component",
        }
        for key, value in (
            ("kind", "lake"),
            ("ground_role", "unknown"),
            ("ground_role", "roof"),
            ("ground_role", "bridge"),
            ("footprint_role", "clipped"),
            ("footprint_role", "continuation"),
        ):
            with self.assertRaises(ValueError):
                build(self.water, self.reference, **dict(arguments, **{key: value}))

    def test_invalid_dimensions_and_bad_geometry_fail_without_modifying_source(self):
        before = self.water.copy()
        args = {
            "kind": "swimming_pool",
            "ground_role": "declared_ground",
            "footprint_role": "declared_complete_local_component",
        }
        for dimension in (0.0, -1.0, np.nan, np.inf, 11.0):
            with self.assertRaises(ValueError):
                build(self.water, self.reference, depth_m=dimension, **args)
        with self.assertRaises(ValueError):
            build(self.water[:0], self.reference, **args)
        for field in ("wall_width_m", "floor_thickness_m", "freeboard_m"):
            with self.assertRaises(ValueError):
                build(self.water, self.reference, **dict(args, **{field: 1e-20}))
        np.testing.assert_array_equal(self.water, before)

    def test_internal_facet_subdivision_creates_no_pool_walls(self):
        triangle = self.water[0]
        centre = triangle.mean(axis=0)
        children = np.array(
            [[triangle[i], triangle[(i + 1) % 3], centre] for i in range(3)]
        )
        subdivided = np.concatenate((children, self.water[1:]))
        body = build(
            subdivided,
            self.reference,
            kind="swimming_pool",
            ground_role="declared_ground",
            footprint_role="declared_complete_local_component",
        )
        for part in (3, 4):
            self.assertEqual(
                int((body["parts"] == part).sum()),
                int((self.body["parts"] == part).sum()),
            )
        np.testing.assert_array_equal(
            body["vertices"][body["faces"][body["parts"] == 3]],
            self.body["vertices"][self.body["faces"][self.body["parts"] == 3]],
        )

    def test_island_hole_is_refused_instead_of_filled_as_a_ground_pool(self):
        xy = np.array(
            [[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]],
            dtype=float,
        )
        quads = np.array([[0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])
        faces = np.vstack((quads[:, [0, 1, 2]], quads[:, [0, 2, 3]]))
        points = np.column_stack((xy, np.ones(8)))
        with self.assertRaisesRegex(ValueError, "islands, holes"):
            build(
                points[faces],
                lambda p: np.ones(len(p)),
                kind="swimming_pool",
                ground_role="declared_ground",
                footprint_role="declared_complete_local_component",
            )


if __name__ == "__main__":
    unittest.main()

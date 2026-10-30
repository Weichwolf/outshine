"""Analytic original material inputs, bindings, unknowns and sparse equivalence."""

import copy
import unittest

import numpy as np
import source_fragment_inputs as fragments
from triangle_columns import prepare, query

CAPTURE = np.diag([1.0, 1.0, 0.25, 1.0])


def source(rear=False, triangle=False):
    p = (
        np.array([[-0.6, -0.6, 2], [0.6, -0.6, 2], [0, 0.6, 2]], np.float32)
        if triangle
        else np.array(
            [[-0.5, -0.5, 2], [0.5, -0.5, 2], [0.5, 0.5, 2], [-0.5, 0.5, 2]], np.float32
        )
    )
    t = (
        np.array([[0, 1, 2]], np.int64)
        if triangle
        else np.array([[0, 1, 2], [0, 2, 3]], np.int64)
    )
    positions = [p, (p - [0, 0, 1]).astype(np.float32)] if rear else [p]
    attributes = []
    for i, xyz in enumerate(positions):
        attributes.append(
            {
                "normals": np.tile([0, 0, 1], (len(p), 1)).astype(np.float32),
                "uv": np.c_[
                    2 * xyz[:, 0] + 3 * xyz[:, 1] + i * 256, -xyz[:, 0] + 2 * xyz[:, 1]
                ].astype(np.float32),
                "uv1": xyz[:, :2].copy(),
                "colours": np.c_[
                    0.2 + 0.1 * xyz[:, 0],
                    0.3 + 0.1 * xyz[:, 1],
                    np.full(len(p), 0.4),
                    np.full(len(p), 0.75),
                ].astype(np.float32),
            }
        )
    return {
        "positions": np.concatenate(positions),
        "indices": np.concatenate([t + i * len(p) for i in range(len(positions))]),
        "parts": tuple(
            {"vertices": len(p), "triangles": len(t), "material": 7 + i * 12}
            for i in range(len(positions))
        ),
        "attributes": tuple(attributes),
    }


def inputs(s, points, world=CAPTURE):
    field = prepare(s, CAPTURE, 16)
    material = fragments.prepare(field, s)
    result = query(field, world, points, [2, 2])
    return material, result


class FragmentTests(unittest.TestCase):
    def test_analytic_affine_fields_and_normalize_after_interpolation(self):
        s = source(triangle=True)
        s["attributes"][0]["normals"][:] = np.eye(3)
        points = np.array([[1.0, 1.0]])
        material, geometry = inputs(s, points)
        value, work = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        np.testing.assert_allclose(value["position"], [[0, 0, 2]], atol=1e-14)
        np.testing.assert_allclose(value["uv"], 0, atol=1e-7)
        np.testing.assert_allclose(value["uv1"], 0, atol=1e-7)
        np.testing.assert_allclose(value["colour"], [[0.2, 0.3, 0.4, 0.75]], atol=2e-8)
        n = np.array([0.25, 0.25, 0.5])
        n /= np.linalg.norm(n)
        np.testing.assert_allclose(value["normal"], [n], atol=1e-14)
        self.assertEqual(value["material_indices"].tolist(), [7])
        self.assertEqual(work["interpolated_corner_rows"], 3)
        self.assertFalse(work["runtime_admitted"])

    def test_shared_internal_edge_preserves_affine_vertex_inputs(self):
        points = np.array([[1, 1], [1 - 1e-5, 1], [1 + 1e-5, 1]], float)
        material, geometry = inputs(source(), points)
        value, _ = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        xy = points * [1, -1] + [-1, 1]
        np.testing.assert_allclose(
            value["uv"],
            np.c_[2 * xy[:, 0] + 3 * xy[:, 1], -xy[:, 0] + 2 * xy[:, 1]],
            atol=1e-12,
        )
        np.testing.assert_allclose(value["normal"], [[0, 0, 1]] * 3)

    def test_revealed_rear_has_its_original_part_and_material(self):
        world = CAPTURE.copy()
        world[0] = [1, 0, -1, 1]
        points = np.array([[0.9, 0.9], [1.1, 0.9], [0.9, 1.1], [1.1, 1.1]])
        material, geometry = inputs(source(rear=True), points, world)
        value, _ = fragments.read(material, world, points, [2, 2], geometry)
        np.testing.assert_array_equal(value["part_ordinals"], 1)
        np.testing.assert_array_equal(value["material_indices"], 19)
        self.assertTrue((value["original_triangle_ordinals"] >= 2).all())
        np.testing.assert_allclose(value["position"][:, 2], 1)
        self.assertTrue((value["uv"][:, 0] > 255).all())

    def test_sparse_and_dense_agree_but_only_hits_expand_attribute_outputs(self):
        y, x = np.mgrid[0:2:16j, 0:2:16j]
        points = np.c_[x.ravel(), y.ravel()]
        material, geometry = inputs(source(), points)
        a, wa = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        b, wb = fragments.read(
            material, CAPTURE, points, [2, 2], geometry, method="dense"
        )
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
        self.assertLess(wa["ray_rows"], wb["ray_rows"])
        self.assertLess(
            wa["temporary_interpolated_payload_bytes"],
            wb["temporary_interpolated_payload_bytes"],
        )

    def test_budget_unknown_geometry_and_cancelling_normals_retain_unknown(self):
        s = source(triangle=True)
        points = np.array([[1, 1]], float)
        material, geometry = inputs(s, points)
        a, w = fragments.read(material, CAPTURE, points, [2, 2], geometry, hits_most=0)
        self.assertFalse(a)
        self.assertEqual(w["reason"], "fragment_hit_budget")
        self.assertEqual(w["interpolated_corner_rows"], 0)
        changed = list(geometry)
        changed[2] = np.zeros(1, bool)
        a, w = fragments.read(material, CAPTURE, points, [2, 2], changed)
        self.assertEqual(w["unknown_geometry_samples"], 1)
        self.assertEqual(len(a["sample_indices"]), 0)
        s["attributes"][0]["normals"][:] = [[1, 0, 0], [1, 0, 0], [-1, 0, 0]]
        material, geometry = inputs(s, points)
        a, w = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        self.assertFalse(a)
        self.assertEqual(w["reason"], "undefined_interpolated_normal")

    def test_prepared_snapshot_keeps_attributes_and_refuses_wrong_geometry_parts(self):
        s = source()
        points = np.array([[1, 1]], float)
        material, geometry = inputs(s, points)
        s["attributes"][0]["normals"][:] = 0
        value, _ = fragments.read(material, CAPTURE, points, [2, 2], geometry)
        np.testing.assert_array_equal(value["normal"], [[0, 0, 1]])
        with self.assertRaises(ValueError):
            material.source["uv"].setflags(write=True)
        changed = copy.deepcopy(s)
        changed["positions"][0, 0] += 0.1
        with self.assertRaises(ValueError):
            fragments.prepare(material.geometry, changed)
        changed = copy.deepcopy(s)
        changed["parts"][0]["vertices"] += 1
        with self.assertRaises(ValueError):
            fragments.prepare(material.geometry, changed)


if __name__ == "__main__":
    unittest.main()

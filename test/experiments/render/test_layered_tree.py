"""Independent affine surfaces, retained hidden layers and metadata-first demand."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from layered_tree_bake import Source, block
from layered_tree_plan import Plan, plan, select
from layered_tree_render import load_cut
from layered_tree_render import query as layered_query
from packed_normals import pack


def sheets(axis=2, levels=(1.0,)):
    other = [i for i in range(3) if i != axis]
    xyz = []
    triangles = []
    uv = []
    metric = []
    colour = []
    normal = []
    binding = []
    for layer, height in enumerate(levels):
        p = np.zeros((4, 3))
        p[:, other] = [[-1, -1], [1, -1], [1, 1], [-1, 1]]
        p[:, axis] = height + 0.25 * p[:, other[0]] - 0.125 * p[:, other[1]]
        ids = np.array([[0, 1, 2], [0, 2, 3]])
        n = np.cross(p[1] - p[0], p[2] - p[0])
        n /= np.linalg.norm(n)
        if n[axis] < 0:
            ids = ids[:, [0, 2, 1]]
            n = -n
        start = len(xyz)
        xyz.extend(p)
        triangles.extend(ids + start)
        u, v = p[:, other].T
        uv.extend(np.column_stack((257 + u, 2 + v)))
        metric.extend(np.column_stack((3 + 2 * u, 4 + v)))
        colour.extend(
            np.column_stack(
                (0.5 + 0.2 * u, 0.6 + 0.1 * v, np.full(4, 0.7), 0.3 + 0.05 * u)
            )
        )
        normal.extend(pack(np.broadcast_to(n, (4, 3))))
        binding.extend([[0, layer, -1, -1, 0, 2 * layer + i] for i in range(2)])
    return {
        k: np.array(v, dtype)
        for k, v, dtype in (
            ("positions", xyz, np.float32),
            ("indices", triangles, np.uint32),
            ("uv", uv, np.float32),
            ("metres", metric, np.float32),
            ("colour", colour, np.float32),
            ("normal", normal, np.uint32),
            ("binding", binding, np.int32),
        )
    }


class LayeredFieldsTest(unittest.TestCase):
    def test_affine_fields_from_each_axis_and_full_rgba(self):
        box = np.array([[-2, -2, -2], [2, 2, 2]], float)
        for axis in range(3):
            original = sheets(axis)
            data = block(Source(original), box, 8)
            tree = Plan(box[None], np.full((1, 8), -1, np.int32), 8)
            with tempfile.TemporaryDirectory(prefix="outshine-layered-test-") as tmp:
                root = Path(tmp)
                (root / "nodes").mkdir()
                np.savez_compressed(root / "nodes/000000.npz", **data)
                cut = load_cut(root, tree, np.array([0]))
                other = [i for i in range(3) if i != axis]
                origin = np.zeros((1, 3))
                origin[0, other] = [0.3, -0.2]
                origin[0, axis] = 5
                direction = np.zeros((1, 3))
                direction[0, axis] = -1
                depth, fields, source, _ = layered_query(cut, origin, direction)
                self.assertTrue(np.isfinite(depth[0]))
                np.testing.assert_array_equal(source, [0])
                np.testing.assert_array_equal(fields["code"], [1])
                np.testing.assert_allclose(
                    fields["coordinates"], [[1.3, 1.8]], atol=2e-6
                )
                np.testing.assert_allclose(fields["metres"], [[3.6, 3.8]], atol=2e-6)
                np.testing.assert_allclose(
                    fields["colour"], [[0.56, 0.58, 0.7, 0.315]], atol=2e-6
                )
                self.assertLess(
                    abs(depth[0] - (5 - (1 + 0.25 * 0.3 - 0.125 * (-0.2)))), 1e-3
                )

    def test_default_white_and_coloured_nodes_can_share_one_cut(self):
        a, b = sheets(), sheets()
        b["positions"][:, 0] += 5
        b["indices"] += len(a["positions"])
        b["binding"][:, 1] = 42
        b["colour"][:] = 1
        combined = {k: np.concatenate((a[k], b[k])) for k in a}
        original = Source(combined)
        boxes = np.array([[[-2, -2, -2], [2, 2, 2]], [[3, -2, -2], [7, 2, 2]]], float)
        with tempfile.TemporaryDirectory(prefix="outshine-layered-test-") as tmp:
            root = Path(tmp)
            (root / "nodes").mkdir()
            for i, box in enumerate(boxes):
                np.savez_compressed(
                    root / "nodes" / f"{i:06d}.npz", **block(original, box, 8)
                )
            cut = load_cut(
                root, Plan(boxes, np.full((2, 8), -1, np.int32), 8), np.array([0, 1])
            )
            _, fields, _, _ = layered_query(
                cut,
                np.array([[0.3, -0.2, 5], [5.3, -0.2, 5]]),
                np.tile([0.0, 0.0, -1.0], (2, 1)),
            )
            np.testing.assert_allclose(
                fields["colour"], [[0.56, 0.58, 0.7, 0.315], [1, 1, 1, 1]], atol=2e-6
            )

    def test_more_than_four_hidden_layers_are_kept(self):
        source = Source(sheets(2, (1, 2, 3, 4, 5, 6)))
        box = np.array([[-2, -2, 0], [6, 6, 8]], float)
        pixel, _, points = source.hits(box, 8, 4)
        counts = np.unique(pixel, return_counts=True)[1]
        self.assertGreaterEqual(int(counts.max()), 6)
        heights = points[:, 2] - 0.25 * points[:, 0] + 0.125 * points[:, 1]
        np.testing.assert_allclose(np.unique(np.round(heights, 5)), np.arange(1, 7))
        self.assertTrue((source.group == 4).all())

    def test_metadata_demand_never_accepts_a_too_coarse_near_leaf(self):
        tree = Plan(
            np.array([[[-1, -1, -1], [1, 1, 1]]], float),
            np.full((1, 8), -1, np.int32),
            8,
        )
        profile = {"focal_px": 4.0, "width": 8, "height": 8}
        chosen, missing, _ = select(tree, np.array([0, 0, 0]), profile)
        self.assertEqual(len(chosen), 0)
        np.testing.assert_array_equal(missing, [0])
        chosen, missing, _ = select(tree, np.array([0, 0, 100]), profile)
        np.testing.assert_array_equal(chosen, [0])
        self.assertEqual(len(missing), 0)

    def test_source_bounds_crossing_cells_remain_covered(self):
        bounds = np.array([[[-2, -2, 0], [2, 2, 0]]], float)
        profile = {"focal_px": 4.0, "width": 8, "height": 8}
        tree, _ = plan(
            bounds,
            np.array([[0, 0, 10], [0, 0, 10]]),
            profile,
            resolution=4,
            target_px=0.5,
        )
        nodes, missing, _ = select(tree, np.array([0, 0, 10]), profile, target_px=0.5)
        self.assertEqual(len(missing), 0)
        for x in np.linspace(-1.9, 1.9, 7):
            for y in np.linspace(-1.9, 1.9, 7):
                p = np.array([x, y, 0.0])
                self.assertTrue(
                    ((p >= tree.bounds[nodes, 0]) & (p <= tree.bounds[nodes, 1]))
                    .all(1)
                    .any()
                )

    def test_node_budget_is_an_error_and_not_a_partial_product(self):
        with self.assertRaises(ValueError):
            plan(
                np.array([[[-1, -1, -1], [1, 1, 1]]], float),
                np.zeros((2, 3)),
                {"focal_px": 4.0, "width": 8, "height": 8},
                max_nodes=1,
            )


if __name__ == "__main__":
    unittest.main()

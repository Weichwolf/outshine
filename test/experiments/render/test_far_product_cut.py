"""Source completeness, safe budget fallback and projected geometric error."""

import unittest

import numpy as np
from far_product_cut import FarProductHierarchy, box_triangles, projection_diameter


class FarProductCut(unittest.TestCase):
    def setUp(self):
        self.boxes = np.array(
            [[[i * 2.0, 0, 10000], [i * 2 + 1, 1, 10001]] for i in range(16)]
        )
        self.boxes[-1] = np.nan
        self.known = np.isfinite(self.boxes).all(axis=(1, 2))
        self.h = FarProductHierarchy(
            self.boxes, known=self.known, proxy_eligible=self.known
        )
        self.profile = {
            "focal_px": 1000.0,
            "width": 1280,
            "height": 720,
            "error_px": 1.0,
            "node_budget": 10000,
        }

    def select(self, **changes):
        return self.h.select(np.zeros((2, 3)), **dict(self.profile, **changes))

    def assert_complete(self, cut):
        members = [*cut.unknown]
        for node, _ in cut.nodes:
            members.extend(self.h.members(node))
        np.testing.assert_array_equal(np.sort(members), np.arange(16))

    def test_proxy_cut_and_zero_budget_preserve_all_original_source_addresses(self):
        cut = self.select()
        self.assert_complete(cut)
        self.assertGreater(cut.work["proxy_sources"], 0)
        for budget in (0, 1, 3, 9):
            cut = self.select(node_budget=budget)
            self.assert_complete(cut)
            self.assertLessEqual(cut.work["node_tests"], budget)
        cut = self.select(node_budget=0)
        self.assertEqual(cut.work["proxy_sources"], 0)
        self.assertEqual(cut.work["original_sources"], 16)

    def test_unknown_and_protected_sources_never_enter_proxy_membership(self):
        flags = np.zeros(16, bool)
        flags[[2, 4, 8]] = True
        cut = self.select(protected=flags)
        self.assert_complete(cut)
        for node, proxy in cut.nodes:
            if proxy:
                self.assertFalse(flags[self.h.members(node)].any())
        np.testing.assert_array_equal(cut.unknown, [15])

    def test_ineligible_and_zero_error_delegate_to_original_plans(self):
        h = FarProductHierarchy(
            self.boxes, known=self.known, proxy_eligible=np.zeros(16, bool)
        )
        cut = h.select(np.zeros((2, 3)), **self.profile)
        self.assertEqual(cut.work["proxy_sources"], 0)
        self.assert_complete(self.select(error_px=0))

    def test_simple_euclidean_screen_error_underestimates_off_axis_change(self):
        p = np.array([640.0, 0, 1000])
        q = p + [0.99, 0, -0.6336]
        box = np.array([np.minimum(p, q), np.maximum(p, q)])
        diameter = np.linalg.norm(q - p)
        naive = 1000 * diameter / np.linalg.norm(box[0])
        actual = 1000 * abs(q[0] / q[2] - p[0] / p[2])
        bound = projection_diameter(
            box, np.zeros((2, 3)), focal_px=1000, width=1280, height=720
        )
        self.assertLess(naive, 1)
        self.assertGreater(actual, 1.3)
        self.assertGreaterEqual(bound, actual)

    def test_inside_and_near_plane_uncertainty_return_unbounded_error(self):
        for box in ([[-1, -1, -1], [1, 1, 1]], [[0, 0, 0.1], [1, 1, 2]]):
            self.assertEqual(
                projection_diameter(
                    box, np.zeros((2, 3)), focal_px=1000, width=1280, height=720
                ),
                np.inf,
            )

    def test_random_visible_point_pairs_observers_and_full_turn(self):
        rng = np.random.default_rng(12)
        box = np.array([[-2.0, -1, 50], [3, 2, 54]])
        observer = np.array([[-1.0, -1, -1], [1, 1, 1]])
        bound = projection_diameter(
            box, observer, focal_px=1000, width=1280, height=720
        )
        p = rng.uniform(box[0], box[1], (200, 3))
        q = rng.uniform(box[0], box[1], (200, 3))
        for yaw in np.linspace(0, 2 * np.pi, 73):
            c, s = np.cos(yaw), np.sin(yaw)
            rotation = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
            eye = rng.uniform(observer[0], observer[1])
            a = (p - eye) @ rotation.T
            b = (q - eye) @ rotation.T
            visible = (
                (a[:, 2] > 0.1)
                & (abs(a[:, 0]) <= 0.64 * a[:, 2])
                & (abs(a[:, 1]) <= 0.36 * a[:, 2])
            )
            if visible.any():
                self.assertTrue((b[visible, 2] > 0.1).all())
                delta = 1000 * np.linalg.norm(
                    a[visible, :2] / a[visible, 2, None]
                    - b[visible, :2] / b[visible, 2, None],
                    axis=1,
                )
                self.assertTrue((delta <= bound).all())

    def test_generated_proxy_is_inside_its_original_node_bound(self):
        cut = self.select()
        for index, proxy in cut.nodes:
            if not proxy:
                continue
            box = self.h.tree.nodes[index]["bounds"]
            triangles = box_triangles(box)
            self.assertEqual(triangles.shape, (12, 3, 3))
            self.assertTrue(((triangles >= box[0]) & (triangles <= box[1])).all())

    def test_observer_and_profile_validity_cannot_be_reused_after_changes(self):
        observer = np.array([[-1.0, -1, -1], [1, 1, 1]])
        cut = self.h.select(observer, **self.profile)
        view = {k: v for k, v in self.profile.items() if k != "node_budget"}
        self.assertTrue(cut.contains_view([1, 1, 1], **view))
        observer[:] = 100
        self.assertTrue(cut.contains_view([0, 0, 0], **view))
        self.assertFalse(cut.contains_view([0, 0, 2], **view))
        self.assertFalse(cut.contains_view([np.nan, 0, 0], **view))
        self.assertFalse(cut.contains_view([0, 0, 0], **dict(view, width=1920)))
        with self.assertRaises(ValueError):
            cut.observer_box.setflags(write=True)


if __name__ == "__main__":
    unittest.main()

"""Geometric product lifetimes and real-arithmetic projection counterexamples."""

import math
import random
import struct
import unittest

from source_geometry_cut import Camera, GeometrySources, dot, unit
from source_product_cut import SourceProducts
from test_source_product_cut import index


def metadata(tree, errors=(0.25,) * 4, faces=(6,) * 4, revision=None):
    revision = (tree.revision if revision is None else revision).encode()
    return (
        struct.pack("<QII", 0x3152544D4F454753, len(errors), len(revision))
        + revision
        + b"".join(struct.pack("<dI", e, f) for e, f in zip(errors, faces))
    )


def project(camera, point, translation=(0, 0, 0)):
    relative = tuple(p - e - t for p, e, t in zip(point, camera.eye, translation))
    x, y, z = (dot(relative, a) for a in (camera.right, camera.up, camera.forward))
    if not camera.orthographic:
        x, y = x / z, y / z
    return camera.focal * x, camera.focal * y


class GeometryCutTests(unittest.TestCase):
    def setUp(self):
        self.tree = SourceProducts(*index(), revision="geometry-fixture")
        self.geometry = GeometrySources(self.tree, metadata(self.tree))

    def test_static_addresses_survive_motion_and_rotation(self):
        cuts = []
        for eye in [(15, 1, 100000), (115, 1, 100000), (15, 101, 100000)]:
            cut, _ = self.geometry.cut(
                Camera.toward(eye, (15, 1, 1), 720, radius=100), 1
            )
            self.tree.validate_cut(cut)
            cuts.append(tuple(p.address for p in cut))
        self.assertEqual(cuts, [cuts[0]] * 3)
        self.assertEqual([p.kind for p in cut], ["hull"])

    def test_near_sources_keep_originals_and_middle_sources_use_declared_shells(self):
        near, _ = self.geometry.cut(Camera.toward((15, 1, 1), (15, 1, 2), 720), 1)
        middle, _ = self.geometry.cut(Camera.toward((15, 1, 1000), (15, 1, 1), 720), 1)
        self.assertEqual([p.kind for p in near], ["original"] * 4)
        self.assertEqual([p.kind for p in middle], ["shell"])
        self.tree.validate_cut(middle)
        self.assertTrue(all(p.selection_error_px <= 1 for p in middle))

    def test_group_error_uses_maximum_and_never_an_average(self):
        geometry = GeometrySources(
            self.tree, metadata(self.tree, errors=(0.25, 100, 0.25, 0.25))
        )
        cut, _ = geometry.cut(Camera.toward((15, 1, 1000), (15, 1, 1), 720), 1)
        self.tree.validate_cut(cut)
        self.assertTrue(
            any(
                p.kind == "original" and self.tree.nodes[p.node][6:8] == (1, 1)
                for p in cut
            )
        )
        self.assertNotIn(0, [p.node for p in cut])

    def test_no_shell_bound_does_not_invent_one(self):
        geometry = GeometrySources(self.tree, metadata(self.tree, errors=(-1,) * 4))
        cut, _ = geometry.cut(Camera.toward((15, 1, 1000), (15, 1, 1), 720), 1)
        self.assertEqual([p.kind for p in cut], ["original"] * 4)

    def test_geometry_support_is_independent_of_field_support(self):
        data, cells, _ = index()
        tree = SourceProducts(data, cells, bytes(4), revision="geometry-fixture")
        geometry = GeometrySources(tree, metadata(tree))
        cut, _ = geometry.cut(Camera.toward((15, 1, 1000), (15, 1, 1), 720), 1)
        self.assertEqual([p.kind for p in cut], ["shell"])

    def test_unresolved_sources_remain_explicit(self):
        geometry = GeometrySources(self.tree, metadata(self.tree, faces=(6, 0, 6, 6)))
        cut, _ = geometry.cut(Camera.toward((15, 1, 100000), (15, 1, 1), 720), 1)
        self.tree.validate_cut(cut)
        self.assertEqual(sum(p.kind == "unresolved" for p in cut), 1)

    def test_invalid_and_foreign_metadata_and_cameras_are_rejected(self):
        valid = metadata(self.tree)
        for data in [
            valid[:-1],
            metadata(self.tree, revision="foreign"),
            metadata(self.tree, errors=(math.nan,) * 4),
            metadata(self.tree, errors=(-2,) * 4),
            metadata(self.tree, errors=(0,) * 3, faces=(6,) * 3),
        ]:
            with self.assertRaises(ValueError):
                GeometrySources(self.tree, data)
        with self.assertRaises(ValueError):
            Camera((0, 0, 0), (1, 0, 0), (1, 0, 0), (0, 0, 1), 720)
        for bad in [-1, math.inf, math.nan]:
            with self.assertRaises(ValueError):
                Camera.toward((0, 0, 0), (0, 0, 1), 720, radius=bad)

    def test_orthographic_hull_spans_ignore_shared_camera_translation(self):
        box = (-3, -2, 40, 3, 2, 50)
        a = Camera.toward((0, 0, 0), (0, 0, 1), 1, orthographic=True)
        b = Camera.toward((0, 0, 0), (0, 0, 1), 1, radius=1000, orthographic=True)
        self.assertEqual(a.hull_error(a.bounds(box)), b.hull_error(b.bounds(box)))

    def test_translation_domain_near_plane_and_orthographic_scale(self):
        camera = Camera.toward((0, 0, 0), (0, 0, 1), 720, radius=2)
        bounds = camera.bounds((-1, -1, 1, 1, 1, 3))
        self.assertEqual(camera.hull_error(bounds), math.inf)
        self.assertEqual(camera.shell_error(bounds, 0.1), math.inf)
        orthographic = Camera.toward((0, 0, 0), (0, 0, 1), 0.25, orthographic=True)
        self.assertAlmostEqual(orthographic.shell_error(bounds, 2), math.sqrt(2) / 2)

    def test_dense_source_proxy_pairs_stay_inside_both_projection_bounds(self):
        randoms = random.Random(1955)
        box = (-3, -2, 40, 3, 2, 50)
        for orthographic in [False, True]:
            camera = Camera.toward(
                (0, 0, 0), (7, 2, 45), 720, radius=2, orthographic=orthographic
            )
            bounds = camera.bounds(box)
            hull = camera.hull_error(bounds)
            shell = camera.shell_error(bounds, 0.25)
            for _ in range(3000):
                translation = tuple(
                    x * randoms.uniform(0, 2)
                    for x in unit(tuple(randoms.uniform(-1, 1) for _ in range(3)))
                )
                a = tuple(randoms.uniform(lo, hi) for lo, hi in zip(box[:3], box[3:]))
                b = tuple(randoms.uniform(lo, hi) for lo, hi in zip(box[:3], box[3:]))
                p, q = project(camera, a, translation), project(camera, b, translation)
                self.assertLessEqual(
                    sum(abs(x - y) for x, y in zip(p, q)), hull + 1e-10
                )
                delta = unit(tuple(randoms.uniform(-1, 1) for _ in range(3)))
                b = tuple(x + 0.25 * d for x, d in zip(a, delta))
                q = project(camera, b, translation)
                self.assertLessEqual(
                    sum(abs(x - y) for x, y in zip(p, q)), shell + 1e-10
                )


if __name__ == "__main__":
    unittest.main()

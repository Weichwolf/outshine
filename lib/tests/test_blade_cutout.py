"""Independent parameter coverage, physical area, seams, anchors and replay controls."""

import json
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.vegetation import Blade, BladeCutout
from shapely.affinity import scale


def shape(blade):
    bottom = -blade.petiole / blade.length
    return shapely.Polygon(
        [
            (-1, bottom),
            (1, bottom),
            (1, 0),
            (0.68, 0.12),
            (0.95, 0.3),
            (0.43, 0.47),
            (0.84, 0.66),
            (0.31, 0.8),
            (0, 1),
            (-0.52, 0.84),
            (-0.82, 0.61),
            (-0.38, 0.44),
            (-0.94, 0.25),
            (-1, 0),
        ]
    )


class BladeCutoutTests(unittest.TestCase):
    def test_whole_parameter_domain_is_covered_without_overlap_or_seam_crossings(self):
        blade = Blade(fold_power=1, fold=0.5)
        domain = shape(blade).difference(shapely.box(0.2, 0.31, 0.3, 0.35))
        cutout = BladeCutout(blade, domain)
        with self.assertRaisesRegex(ValueError, "does not resolve"):
            cutout.mesh(32, 3)
        mesh = cutout.mesh(64, 3)
        xy = mesh["parameters"][:, ::-1]
        triangles = shapely.polygons(xy[mesh["faces"]])
        self.assertTrue(shapely.coverage_is_valid(triangles))
        union = shapely.coverage_union_all(triangles)
        self.assertLess(union.symmetric_difference(domain).area, 1e-12)
        self.assertAlmostEqual(shapely.area(triangles).sum(), domain.area, places=12)
        corners = mesh["parameters"][mesh["faces"]]
        for component in (0, 1):
            self.assertFalse(
                np.any(
                    (corners[:, :, component].min(axis=1) < -1e-14)
                    & (corners[:, :, component].max(axis=1) > 1e-14)
                )
            )
        # Independent edge-incidence: only the intended polygon boundary is open.
        f = mesh["faces"]
        edges = np.sort(
            np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]]), axis=1
        )
        edges, count = np.unique(edges, axis=0, return_counts=True)
        self.assertTrue(np.all(count <= 2))
        boundary = shapely.linestrings(xy[edges[count == 1]])
        self.assertLess(
            shapely.union_all(boundary).hausdorff_distance(domain.boundary), 1e-12
        )

    def test_analytic_flat_area_and_anchors_do_not_depend_on_mesh_detail(self):
        blade = Blade(
            length=0.12,
            width=0.06,
            petiole=0,
            base_fill=1,
            lobes=0,
            fold=0,
            bend=0,
            twist=0,
        )
        cutout = BladeCutout(blade, shapely.Polygon([(-1, 0), (1, 0), (0, 1)]))
        for along in (16, 32, 64):
            mesh = cutout.mesh(along, 2)
            p = mesh["vertices"][mesh["faces"]]
            area = (
                np.linalg.norm(
                    np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1
                ).sum()
                / 2
            )
            expected = blade.width * blade.length / 3
            self.assertLess(abs(area / expected - 1), 2 / along**2)
            for point in ((0, 0), (1, 0)):
                found = np.where(np.all(mesh["parameters"] == point, axis=1))[0]
                self.assertEqual(len(found), 1)
                np.testing.assert_array_equal(
                    mesh["vertices"][found[0]], [0, point[0] * blade.length, 0]
                )

    def test_normals_match_independent_finite_differences_and_one_sided_fold(self):
        for power in (1, 2):
            blade = Blade(fold_power=power, fold=0.4, bend=0.7, twist=0.5)
            cutout = BladeCutout(blade, shape(blade))
            mesh = cutout.mesh(48, 4)
            s, t = 0.32, 0.4
            h = 1e-6
            ds = (blade.evaluate(t, s + h, 1)[0] - blade.evaluate(t, s - h, 1)[0]) / (
                2 * h
            )
            dt = (blade.evaluate(t + h, s, 1)[0] - blade.evaluate(t - h, s, 1)[0]) / (
                2 * h
            )
            n = np.cross(ds, dt)
            n /= np.linalg.norm(n)
            np.testing.assert_allclose(blade.evaluate(t, s, 1)[1], n, atol=1e-9)
            corners = mesh["parameters"][mesh["faces"]]
            normal = mesh["normals"].reshape(-1, 3, 3)
            on_fold = abs(corners[:, :, 1]) < 1e-15
            indices = np.where(
                on_fold & (corners[:, :, 0] > 0.1) & (corners[:, :, 0] < 0.9)
            )
            if power == 1:
                # Distinct normals exist on the two sides of identical shared vertices.
                self.assertTrue(np.any(normal[indices][:, 0] > 0.2))
                self.assertTrue(np.any(normal[indices][:, 0] < -0.2))
            p = mesh["vertices"][mesh["faces"]]
            cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
            self.assertTrue(np.all(np.sum(cross[:, None] * normal, axis=2) > 0))

    def test_capacity_precedes_surface_mapping_and_replay_is_exact(self):
        cutout = BladeCutout(Blade(), shape(Blade()))
        with patch.object(
            Blade, "evaluate", side_effect=AssertionError("unbudgeted map")
        ):
            self.assertIsNone(cutout.mesh(max_faces=0))
            self.assertIsNone(cutout.mesh(max_faces=1))
            self.assertIsNone(cutout.mesh(max_vertices=1))
        restored = BladeCutout.from_recipe(json.loads(json.dumps(cutout.recipe())))
        self.assertEqual(restored.identity, cutout.identity)
        for name, values in cutout.mesh().items():
            np.testing.assert_array_equal(values, restored.mesh()[name])
        other = BladeCutout(replace(cutout.blade, twist=0.3), cutout.domain)
        self.assertNotEqual(cutout.identity, other.identity)
        self.assertEqual(cutout.blade, Blade())
        numpy_blade = replace(cutout.blade, lobes=np.int64(4), width=np.float32(0.06))
        numpy_cutout = BladeCutout(numpy_blade, shape(numpy_blade))
        restored_numpy = BladeCutout.from_recipe(
            json.loads(json.dumps(numpy_cutout.recipe()))
        )
        self.assertEqual(numpy_cutout.identity, restored_numpy.identity)

    def test_invalid_domains_and_unresolved_orientation_fail_explicitly(self):
        blade = Blade()
        for domain in (
            shapely.box(-1, 0, 1, 1),  # Missing petiole / collapsed tip.
            scale(shape(blade), xfact=1.1, origin=(0, 0)),
            shape(blade).difference(shapely.box(-0.1, 0.2, 0.1, 0.3)),
            shapely.Polygon([(0, 0), (1, 1), (1, 0), (0, 1)]),
        ):
            with self.assertRaises(ValueError):
                BladeCutout(blade, domain)
        cutout = BladeCutout(blade, shape(blade))
        for args in ((True, 2), (1, 2), (129, 2), (32, 0)):
            with self.assertRaises(ValueError):
                cutout.mesh(*args)


if __name__ == "__main__":
    unittest.main()

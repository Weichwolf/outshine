"""Source ownership, bounded work and independent positive-normal controls."""

import unittest
from unittest.mock import patch

import numpy as np
from group_cloud import make_chart
from leaf_patch_catalog import LeafPatchCatalog
from leaf_patch_prototype import build_prototype, chart_moment, normal_field
from outshine_ref.vegetation import Blade
from outshine_ref.vegetation.normal_distribution import (
    evaluate_diffuse,
    jensen_error_bound,
)
from raster_batch import rasterize
from spatial_source_checks import fixture


class LeafPatches(unittest.TestCase):
    def test_retained_map_preserves_raw_vector_length_and_coverage_mass(self):
        blade = Blade(length=0.15, width=0.06, bend=0.4, twist=0.5, fold=0.2)
        mesh = blade.mesh(8, 1)
        p = build_prototype(blade, 0, 4)
        variance = False
        for part, c in enumerate(p.charts):
            mask = p.face_partition == part
            source = mesh | {
                "faces": mesh["faces"][mask],
                "normals": mesh["normals"].reshape(-1, 3, 3)[mask].reshape(-1, 3),
            }
            exact = normal_field(source, c, 16)
            decoded = chart_moment(c)
            np.testing.assert_allclose(decoded, exact, rtol=0, atol=1e-15)
            length = np.linalg.norm(decoded[:, 0, 1:], axis=1)
            mass = decoded[:, 0, 0]
            self.assertTrue(np.all(length <= mass + 1e-12))
            variance |= bool(np.any(length < mass - 1e-6))
        self.assertTrue(variance)

    def test_refusal_and_summary_do_not_build_poses_or_meshes(self):
        source = fixture()
        with (
            patch(
                "outshine_ref.vegetation.Shoot.detail",
                side_effect=AssertionError("poses"),
            ),
            patch(
                "outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError("mesh")
            ),
        ):
            catalog = LeafPatchCatalog(source, 4)
            catalog.summary()
            self.assertIsNone(catalog.group(int(source.crown.hosts[0]), 0))
        self.assertFalse(catalog.prototypes)
        self.assertFalse(source.crown._groups)

    def test_partitions_cover_each_source_face_once_and_bound_projection(self):
        blade = fixture().blades[0]
        mesh = blade.mesh(8, 1)
        for count in (1, 2, 4):
            p = build_prototype(blade, 0, count)
            self.assertEqual(len(p.face_partition), len(mesh["faces"]))
            self.assertEqual(set(p.face_partition), set(range(count)))
            for part, chart in enumerate(p.charts):
                faces = mesh["faces"][p.face_partition == part]
                displacement = abs(
                    mesh["vertices"][faces] @ chart.frame[:, 2] - chart.offset
                )
                self.assertLessEqual(float(displacement.max()), chart.displacement_m)
                self.assertFalse(p.normal_bounds.flags.writeable)
                self.assertFalse(chart.normal.flags.writeable)
                self.assertTrue(np.isfinite(chart.mesh["vertices"]).all())
                triangle = chart.mesh["vertices"][chart.mesh["faces"]]
                self.assertTrue(
                    (
                        np.linalg.norm(
                            np.cross(
                                triangle[:, 1] - triangle[:, 0],
                                triangle[:, 2] - triangle[:, 0],
                            ),
                            axis=1,
                        )
                        > 0
                    ).all()
                )

    def test_exact_original_poses_ids_and_physical_area_counted_once(self):
        source = fixture()
        catalog = LeafPatchCatalog(source, 4)
        node = int(source.crown.hosts[-1])
        g = catalog.group(node)
        original = source.crown.group(node, 128)
        for k in original:
            np.testing.assert_array_equal(g["poses"][k], original[k])
        self.assertEqual(len(g["physical_leaf_area_m2"]), len(original["ids"]))
        np.testing.assert_array_equal(
            g["physical_leaf_area_m2"],
            [source.blades[int(v)].area() for v in original["variants"]],
        )
        self.assertEqual(len(catalog.charts(g)), 4 * len(original["ids"]))
        self.assertFalse(source.prototypes)

    def test_reordered_hosts_share_images_without_changing_anchors(self):
        source = fixture()
        catalog = LeafPatchCatalog(source, 2)
        nodes = list(map(int, source.crown.hosts))
        groups = {n: catalog.group(n) for n in nodes}
        for node in nodes[::-1]:
            g = catalog.group(node)
            for k, a in g["poses"].items():
                np.testing.assert_array_equal(a, groups[node]["poses"][k])
            self.assertEqual(len(source.crown._groups), 1)
        g = groups[nodes[0]]
        charts = catalog.charts(g)
        origin, frame = source.crown.stem(nodes[0])
        for i, c in enumerate(charts):
            leaf, part = divmod(i, 2)
            v = int(g["poses"]["variants"][leaf])
            t = catalog.prototypes[v].charts[part]
            self.assertTrue(np.shares_memory(c.colour, t.colour))
            expected = (
                t.mesh["vertices"] @ g["poses"]["frames"][leaf].T
                + g["poses"]["origins"][leaf]
            )
            np.testing.assert_allclose(
                c.mesh["vertices"] @ frame.T + origin, expected, rtol=0, atol=2e-15
            )

    def test_group_uses_no_fused_reference_geometry_or_whole_tables(self):
        source = fixture()
        catalog = LeafPatchCatalog(source, 2)
        with (
            patch.object(
                source, "geometry", side_effect=AssertionError("fused source")
            ),
            patch(
                "crown_shoots.CrownShoots.tables",
                side_effect=AssertionError("whole poses"),
            ),
        ):
            catalog.group(int(source.crown.hosts[0]))
        self.assertEqual(catalog.summary()["source_triangles_built"], 272)
        self.assertEqual(catalog.summary()["prepared_variants"], 8)

    def test_positive_normal_field_matches_scalar_signed_contacts(self):
        source = {
            "vertices": np.array(((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), float),
            "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32),
            "parameters": np.array(((0, 0), (1, 0), (1, 1), (0, 1)), float),
            "normals": np.repeat(np.array(((0.8, 0, 0.6), (-0.8, 0, 0.6))), 3, axis=0),
            "organ_faces": np.zeros(2, np.int32),
            "variants": np.zeros(4, np.uint8),
        }
        chart = make_chart(source, np.array((0,)), np.array((0.0, 0.0, 1.0)), 16)
        field = normal_field(source, chart, 16, partitions=4)
        corners = chart.mesh["vertices"]
        q = source["vertices"] @ chart.frame
        lower, upper = (
            (corners @ chart.frame)[:, :2].min(axis=0),
            (corners @ chart.frame)[:, :2].max(axis=0),
        )
        covered, normals = rasterize(
            q, source["faces"], source["normals"].reshape(-1, 3, 3), lower, upper, 32
        )
        lights = np.array(((1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1)), float)
        exact = np.zeros((4, 256))
        for y, x in zip(*np.nonzero(covered), strict=True):
            n = normals[y, x].astype(float)
            n /= np.linalg.norm(n)
            for k, l in enumerate(lights):
                exact[k, y // 2 * 16 + x // 2] += 0.25 * max(0, float(n @ l))
        predicted = evaluate_diffuse(field[..., 1:], lights)
        np.testing.assert_allclose(predicted, exact, rtol=0, atol=1e-14)
        self.assertTrue(
            (abs(predicted - exact) <= jensen_error_bound(field)[None] + 1e-14).all()
        )


if __name__ == "__main__":
    unittest.main()

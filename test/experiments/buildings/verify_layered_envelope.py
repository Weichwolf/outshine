import unittest

import numpy as np
import shapely
from layered_envelope import layered_envelope
from outshine_ref.buildings.modelling import PrismVolume, union_prisms


def measure(product):
    mesh = product.mesh
    xyz = mesh["vertices"][mesh["faces"]]
    shift = xyz.astype(np.longdouble) - xyz.mean(axis=(0, 1)).astype(np.longdouble)
    return float((shift[:, 0] * np.cross(shift[:, 1], shift[:, 2])).sum() / 6), float(
        np.linalg.norm(
            np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]), axis=1
        ).sum()
        / 2
    )


class Layers(unittest.TestCase):
    def test_metric_uv_and_normals_follow_each_actual_surface(self):
        source = PrismVolume(
            "metric", shapely.box(100.0, -30.0, 110.0, -22.0), 2.0, 5.0, 17
        )
        product = layered_envelope([source], roof_material=9, base_material=4)
        mesh = product.mesh
        xyz = mesh["vertices"][mesh["faces"]]
        uv = mesh["uv"][mesh["faces"]]
        vectors = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        normals = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
        np.testing.assert_allclose(
            mesh["normals"][mesh["faces"]],
            np.repeat(normals[:, None], 3, axis=1),
            atol=1e-12,
            rtol=0,
        )
        caps = product.surface_roles != 2
        np.testing.assert_allclose(
            uv[caps], xyz[caps, :, :2] - [105.0, -26.0], atol=1e-12, rtol=0
        )
        walls = product.surface_roles == 2
        np.testing.assert_allclose(
            uv[walls, :, 1], xyz[walls, :, 2], atol=1e-12, rtol=0
        )
        for a, b in ((0, 1), (1, 2), (2, 0)):
            np.testing.assert_allclose(
                np.linalg.norm(uv[walls, a] - uv[walls, b], axis=1),
                np.linalg.norm(xyz[walls, a] - xyz[walls, b], axis=1),
                atol=1e-12,
                rtol=0,
            )

    def test_analytic_stepped_box_removes_only_interface_caps(self):
        polygons = [
            shapely.box(0, 0, 18, 14),
            shapely.box(0.8, 0.8, 17.2, 13.2),
            shapely.box(1.6, 1.6, 16.4, 12.4),
        ]
        sources = [
            PrismVolume(str(i), polygon, 3 * i, 3 * (i + 1), 17 + i)
            for i, polygon in enumerate(polygons)
        ]
        product = layered_envelope(sources, roof_material=9, base_material=4)
        np.testing.assert_allclose(
            measure(product), [1845.6, 1022.4], atol=1e-9, rtol=0
        )
        self.assertEqual(product.source_ids, ("0", "1", "2"))
        for role, material in ((0, 9), (1, 4)):
            np.testing.assert_array_equal(
                product.mesh["materials"][product.surface_roles == role], material
            )
        walls = product.surface_roles == 2
        np.testing.assert_array_equal(
            product.mesh["materials"][walls],
            np.asarray([17, 18, 19])[product.source_indices[walls]],
        )

    def test_equal_layers_and_partially_shared_boundary_match_complete_solid(self):
        for polygons in (
            [shapely.box(0, 0, 10, 8)] * 3,
            [
                shapely.box(0, 0, 10, 8),
                shapely.box(0, 0, 8, 6),
                shapely.box(0, 0, 6, 4),
            ],
        ):
            sources = [
                PrismVolume(str(i), p, i * 3, (i + 1) * 3)
                for i, p in enumerate(polygons)
            ]
            product = layered_envelope(sources)
            other = union_prisms(sources)
            np.testing.assert_allclose(
                measure(product), [other.volume_m3, other.area_m2], atol=1e-8, rtol=0
            )

    def test_courtyard_is_preserved_and_large_translation_changes_no_measure(self):
        outer = shapely.Polygon(
            [(0, 0), (20, 0), (20, 18), (0, 18)],
            holes=[[(6, 5), (6, 13), (14, 13), (14, 5)]],
        )
        upper = outer.buffer(-1.0, join_style="mitre")
        sources = [
            PrismVolume("base", outer, 0.0, 3.0),
            PrismVolume("upper", upper, 3.0, 6.0),
        ]
        original = layered_envelope(sources)
        for shift in ((0.0, 0.0), (1e6, -2e6)):
            moved = [
                PrismVolume(
                    s.identifier,
                    shapely.affinity.translate(s.polygon, *shift),
                    s.low_m,
                    s.high_m,
                )
                for s in sources
            ]
            product = layered_envelope(moved)
            np.testing.assert_allclose(
                measure(product), measure(original), atol=1e-7, rtol=0
            )
            self.assertTrue(
                shapely.from_wkb(product.base_footprint_wkb).equals(moved[0].polygon)
            )

    def test_replay_immutable_buffers_and_complete_source_roles(self):
        sources = [
            PrismVolume("base", shapely.box(0, 0, 10, 8), 0.0, 3.0),
            PrismVolume("top", shapely.box(1, 1, 9, 7), 3.0, 6.0),
        ]
        a = layered_envelope(sources)
        b = layered_envelope(sources)
        for key in a.mesh:
            np.testing.assert_array_equal(a.mesh[key], b.mesh[key])
        for array in [
            *a.mesh.values(),
            a.vertex_roots,
            a.source_indices,
            a.surface_roles,
        ]:
            with self.assertRaises(ValueError):
                array.setflags(write=True)
        self.assertEqual(len(a.source_indices), len(a.mesh["faces"]))

    def test_invalid_non_nested_gapped_duplicate_and_over_capacity_reject(self):
        base = PrismVolume("base", shapely.box(0, 0, 10, 8), 0.0, 3.0)
        for sources in (
            [],
            [base, PrismVolume("up", shapely.box(0, 0, 12, 8), 3.0, 6.0)],
            [base, PrismVolume("up", shapely.box(1, 1, 9, 7), 4.0, 6.0)],
            [base, PrismVolume("base", shapely.box(1, 1, 9, 7), 3.0, 6.0)],
        ):
            with self.assertRaises(ValueError):
                layered_envelope(sources)
        for capacity in (True, 0, 1):
            with self.assertRaises(ValueError):
                layered_envelope([base], max_triangles=capacity)


if __name__ == "__main__":
    unittest.main()

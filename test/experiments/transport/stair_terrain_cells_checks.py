"""Whole domain, fine contact neighborhoods and conforming mixed terrain cells."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_terrain_cells import transport_cells
from stair_terrain_layout import partition
from stair_terrain_solid import retaining_body
from surface_solid_checks import audit_closed


class MixedCells(unittest.TestCase):
    def test_odd_domain_has_no_missing_or_overlapping_cells(self):
        xs, ys = np.arange(24) * 12, np.arange(18) * 12
        shape = shapely.box(80, 70, 85, 75)
        cells, note = transport_cells(
            xs, ys, shape, near_m=48, sample=lambda xy: np.zeros(len(xy))
        )
        rectangle = shapely.box(xs[0], ys[0], xs[-1], ys[-1])
        union = shapely.union_all(cells)
        self.assertLess(union.symmetric_difference(rectangle).area, 1e-9)
        self.assertAlmostEqual(float(shapely.area(cells).sum()), rectangle.area)
        self.assertGreater(note["coarse_blocks"], 0)
        self.assertLess(len(cells), (len(xs) - 1) * (len(ys) - 1))
        near = cells[shapely.intersects(cells, shape.buffer(48))]
        bounds = shapely.bounds(near)
        self.assertLessEqual(float((bounds[:, 2:] - bounds[:, :2]).max()), 12)
        self.assertLessEqual(note["maximum_far_cell_width_m"], 24)

    def test_zero_width_reversed_nonfinite_or_coarser_fine_axes_fail(self):
        shape = shapely.box(0, 0, 1, 1)
        for xs in ([0, 0], [12, 0], [0, float("nan")], [0, 13]):
            with self.assertRaises(ValueError):
                transport_cells(xs, [0, 12], shape, near_m=48)
        with self.assertRaises(ValueError):
            transport_cells([0, 12], [0, 12], shape, near_m=47)
        with self.assertRaises(ValueError):
            transport_cells([0, 12], [0, 12], shapely.Polygon(), near_m=48)

    def test_domain_budget_precedes_axis_array_allocation(self):
        class HugeAxis:
            def __len__(self):
                return 200000

            def __array__(self, *args, **kwargs):
                raise AssertionError("oversized axes converted before rejection")

        with self.assertRaisesRegex(ValueError, "bounded original"):
            transport_cells(HugeAxis(), [0, 12], shapely.box(0, 0, 1, 1), near_m=48)

    def test_unsampled_peak_keeps_fine_nodes_and_missing_dem_cannot_coarsen(self):
        axes = np.array([0, 12, 24, 36, 48])
        distant = shapely.box(1000, 1000, 1001, 1001)

        def peak(xy):
            return np.where(np.all(xy == [12, 12], axis=1), 5.0, 0.0)

        cells, note = transport_cells(axes, axes, distant, near_m=48, sample=peak)
        self.assertEqual(note["dem_refined_blocks"], 1)
        self.assertEqual(note["maximum_candidate_control_error_m"], 5)
        self.assertLessEqual(note["maximum_retained_control_error_m"], 0.25)
        self.assertEqual(len(cells), 7)
        unknown, note = transport_cells(axes, axes, distant, near_m=48)
        self.assertEqual(len(unknown), 16)
        self.assertEqual(note["coarse_blocks"], 0)
        with self.assertRaisesRegex(ValueError, "finite DEM controls"):
            transport_cells(
                axes,
                axes,
                distant,
                near_m=48,
                sample=lambda xy: np.full(len(xy), np.nan),
            )

    def test_mixed_partition_preserves_source_facets_and_closed_whole_topology(self):
        vertices = np.array(
            [
                [0, 0, 0],
                [4, 0, 0],
                [4, 4, 0],
                [0, 4, 0],
                [240, 180, 0],
                [244, 180, 0],
                [244, 184, 0],
                [240, 184, 0],
            ],
            float,
        )
        faces = np.array([[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]])
        ground = Mesh(vertices, faces)
        transport = shapely.union_all(shapely.polygons(vertices[faces, :2]))
        layout, note = partition(
            ground,
            None,
            None,
            sample=lambda xy: np.zeros(len(xy)),
            interface="boundary_band",
            transport=transport,
        )
        self.assertGreater(note["coarse_blocks"], 0)
        actual = np.c_[layout["xy"], layout["bed"]][layout["faces"][layout["ground"]]]
        np.testing.assert_array_equal(actual, vertices[faces])
        projected = shapely.polygons(layout["xy"][layout["faces"]])
        self.assertTrue(shapely.coverage_is_valid(projected))
        self.assertLess(
            shapely.union_all(projected)
            .symmetric_difference(shapely.box(-15, -15, 259, 199))
            .area,
            1e-7,
        )
        body, _parts = retaining_body(layout)
        self.assertGreater(audit_closed(body), 0)
        self.assertGreater(
            audit_closed(
                Mesh(body.vertices.astype(np.float32).astype(float), body.triangles)
            ),
            0,
        )


if __name__ == "__main__":
    unittest.main()

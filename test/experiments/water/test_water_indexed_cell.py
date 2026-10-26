"""Lossless indexed geometry, masks, source ownership and refusal controls."""

import dataclasses
import unittest

import numpy as np
import shapely
from shapely.affinity import translate
from test_water_parameter_cut import CELL, cook
from water_indexed_cell import index_cell, topology
from water_nodal_height import parameter_triangle
from water_parameter_cut import cut_cell


class IndexedWater(unittest.TestCase):
    def setUp(self):
        self.local = shapely.Polygon(
            [(0.0625, 0.0625), (0.9375, 0.0625), (0.9375, 0.9375), (0.0625, 0.9375)],
            [[(0.25, 0.25), (0.25, 0.5), (0.5, 0.5), (0.5, 0.25)]],
        )
        self.sources = [translate(self.local, xoff=CELL[1], yoff=CELL[2])]
        self.cut = cook(self.sources)

    def test_indexing_preserves_every_position_source_face_and_parent_normal(self):
        indexed = index_cell(self.cut, self.sources)
        np.testing.assert_array_equal(
            indexed.parameter[indexed.faces], self.cut.parameter
        )
        np.testing.assert_array_equal(indexed.world[indexed.faces], self.cut.world)
        np.testing.assert_array_equal(
            indexed.parent_normals[indexed.parents], self.cut.normals
        )
        np.testing.assert_array_equal(indexed.parents, self.cut.parents)
        self.assertEqual(indexed.source_keys, self.cut.source_keys)
        self.assertEqual(indexed.context, self.cut.context)
        for name in (
            "offsets",
            "members",
            "representative",
            "member_area_m2",
            "uncovered_area_m2",
            "status",
        ):
            a, b = getattr(indexed.membership, name), getattr(self.cut.membership, name)
            np.testing.assert_array_equal(a, b)
            self.assertFalse(np.shares_memory(a, b))
            self.assertFalse(a.flags.writeable)
        for name in (
            "parameter",
            "world",
            "faces",
            "parents",
            "parent_normals",
            "boundary",
        ):
            self.assertFalse(getattr(indexed, name).flags.writeable)
        audit = indexed.report["topology"]
        self.assertEqual(
            (
                audit["components"],
                audit["holes"],
                audit["boundary_loops"],
                audit["euler_characteristic"],
            ),
            (1, 1, 2, 0),
        )

    def test_disconnected_islands_keep_separate_components(self):
        pieces = [
            shapely.box(0.0625, 0.0625, 0.1875, 0.1875),
            shapely.box(0.625, 0.625, 0.875, 0.875),
        ]
        sources = [translate(p, xoff=CELL[1], yoff=CELL[2]) for p in pieces]
        indexed = index_cell(cook(sources), sources)
        audit = indexed.report["topology"]
        self.assertEqual(
            (audit["components"], audit["holes"], audit["boundary_loops"]), (2, 0, 2)
        )

    def test_shared_crease_vertices_keep_two_distinct_parent_normals(self):
        def crease(keys):
            xy = parameter_triangle(keys) - CELL[1:]
            return "admitted", np.c_[100 * xy, 500 + 5 * abs(xy[:, 0] - 0.5)]

        cut, _ = cut_cell(CELL, self.sources, ("crease",), crease, context="crease-v1")
        indexed = index_cell(cut, self.sources)
        np.testing.assert_array_equal(indexed.world[indexed.faces], cut.world)
        np.testing.assert_array_equal(
            indexed.parent_normals[indexed.parents], cut.normals
        )
        self.assertGreater(
            len(np.unique(indexed.parent_normals[indexed.parents], axis=0)), 1
        )

    def test_incomplete_source_membership_is_not_published(self):
        status = self.cut.membership.status.copy()
        status[0] = 1
        csr = dataclasses.replace(self.cut.membership, status=status)
        with self.assertRaisesRegex(ValueError, "source CSR"):
            index_cell(dataclasses.replace(self.cut, membership=csr), self.sources)

    def test_four_independent_cells_keep_the_same_corner_and_border_positions(self):
        def field(keys):
            xy = parameter_triangle(keys) - [5, 5]
            return "admitted", np.c_[100 * xy, 400 + xy[:, 0] - xy[:, 1]]

        products = {}
        for x, y in ((6, 6), (5, 5), (6, 5), (5, 6)):
            cell = (4, x, y)
            sources = [shapely.box(x, y, x + 1, y + 1)]
            cut, _ = cut_cell(cell, sources, (cell,), field, context="shared-v1")
            products[cell] = index_cell(cut, sources)
        for cell, product in products.items():
            corner = np.all(product.parameter + cell[1:] == [6, 6], axis=1)
            np.testing.assert_array_equal(product.world[corner], [[100, 100, 400]])
            for axis, neighbour in (
                (0, (4, cell[1] + 1, cell[2])),
                (1, (4, cell[1], cell[2] + 1)),
            ):
                if neighbour not in products:
                    continue
                other = products[neighbour]
                left = product.world[product.parameter[:, axis] == 1]
                right = other.world[other.parameter[:, axis] == 0]
                np.testing.assert_array_equal(left, right)

    def test_refinement_keeps_source_topology_without_rebuilding_normals(self):
        dense = cook(self.sources, refine_interior=True)
        indexed = index_cell(dense, self.sources)
        self.assertEqual(len(indexed.faces), 3 * len(self.cut.parameter))
        np.testing.assert_array_equal(indexed.world[indexed.faces], dense.world)
        np.testing.assert_array_equal(
            indexed.parent_normals[indexed.parents], dense.normals
        )
        self.assertEqual(indexed.report["topology"]["holes"], 1)

    def test_any_world_disagreement_at_equal_xy_refuses_instead_of_snapping(self):
        world = self.cut.world.copy()
        _, inverse, counts = np.unique(
            self.cut.parameter.reshape(-1, 2),
            axis=0,
            return_inverse=True,
            return_counts=True,
        )
        repeated = np.flatnonzero(inverse == np.flatnonzero(counts > 1)[0])
        row = world.reshape(-1, 3)[repeated[0]]
        row[2] = np.nextafter(row[2], np.inf)
        with self.assertRaisesRegex(ValueError, "different world values"):
            index_cell(dataclasses.replace(self.cut, world=world), self.sources)

    def test_mask_change_invalid_root_and_normal_disagreement_refuse(self):
        with self.assertRaisesRegex(ValueError, "source mask differs"):
            index_cell(self.cut, [translate(self.sources[0], xoff=0.01)])
        parents = self.cut.parents.copy()
        parents[0] = 8
        with self.assertRaisesRegex(ValueError, "bounded finite"):
            index_cell(dataclasses.replace(self.cut, parents=parents), self.sources)
        normal = self.cut.normals.copy()
        repeated = np.flatnonzero(self.cut.parents == self.cut.parents[0])
        normal[repeated[0], 0] = np.nextafter(normal[repeated[0], 0], np.inf)
        with self.assertRaisesRegex(ValueError, "inconsistent normals"):
            index_cell(dataclasses.replace(self.cut, normals=normal), self.sources)

    def test_hanging_vertex_and_pinched_islands_are_not_silently_welded(self):
        points = np.array([[0, 0], [1, 0], [1, 1], [0, 1], [0.5, 0.5]])
        faces = np.array([[0, 2, 1], [0, 3, 4], [4, 3, 2]])
        with self.assertRaisesRegex(ValueError, "boundary vertex"):
            topology(points, faces, shapely.box(0, 0, 1, 1))
        pieces = [shapely.box(0, 0, 0.5, 0.5), shapely.box(0.5, 0.5, 1, 1)]
        sources = [translate(p, xoff=CELL[1], yoff=CELL[2]) for p in pieces]
        with self.assertRaisesRegex(ValueError, "boundary vertex"):
            index_cell(cook(sources), sources)


if __name__ == "__main__":
    unittest.main()

import struct
import unittest

from source_geometry_costs import coalesce, cut, geometry_inputs
from panoramic_geometry_cut import PanoramicView
from test_source_product_cut import index


def inventory(rows=((1, 6, 0.25),) * 4):
    return struct.pack("<QI", 0x3250524752435342, len(rows)) + b"".join(
        struct.pack("<IBId", 1, supported, faces, error)
        for supported, faces, error in rows
    )


class SourceGeometryCostsTests(unittest.TestCase):
    def test_ready_metadata_uses_existing_geometry_reference(self):
        data, _, _ = index()
        members, sources = geometry_inputs(inventory(), data, "fixture")
        self.assertEqual(len(members), 4)
        near = cut(sources, (15, 1, 3), (1280, 720), 720, 1, False)
        far = cut(sources, (15, 1, 1000), (1280, 720), 720, 1, False)
        self.assertEqual(near["representations"]["fine"]["sources"], 4)
        self.assertEqual(far["representations"]["shell"]["sources"], 4)
        self.assertEqual(far["representations"]["shell"]["source_faces"], 24)

    def test_hypothetical_hull_is_distinct_from_available_products(self):
        data, _, _ = index()
        _, sources = geometry_inputs(inventory(), data, "fixture")
        available = cut(sources, (15, 1, 1000000), (1280, 720), 720, 1, False)
        hypothetical = cut(sources, (15, 1, 1000000), (1280, 720), 720, 1, True)
        self.assertEqual(available["representations"]["hull"]["products"], 0)
        self.assertEqual(hypothetical["representations"]["hull"]["products"], 1)
        self.assertEqual(hypothetical["representations"]["hull"]["sources"], 4)

    def test_unknown_source_remains_explicit_in_complete_cut(self):
        data, _, _ = index()
        _, sources = geometry_inputs(inventory(((1, 6, 0.25), (0, 0, -1),
                                                (1, 6, -1), (1, 6, 0.25))), data, "fixture")
        report = cut(sources, (15, 1, 1000), (1280, 720), 720, 1, True)
        rows = report["representations"]
        self.assertEqual(rows["unknown"]["sources"], 1)
        self.assertEqual(rows["fine"]["sources"], 1)
        self.assertEqual(sum(row["sources"] for row in rows.values()), 4)
        admission = report["cell_parent_admission"]
        self.assertEqual(admission["generate_groups"], 0)
        self.assertEqual(admission["keep_parent_cells"], [1])
        self.assertEqual(admission["deferred_known_sources"], 3)

    def test_unresolved_members_never_poison_known_recipe_groups(self):
        data, _, _ = index()
        _, sources = geometry_inputs(inventory(((1, 6, -1), (0, 0, -1),
                                                (1, 6, -1), (1, 6, -1))), data, "fixture")
        products, _ = sources.cut(PanoramicView((15, 1, 1000), 720, 1280, 720), 1,
                                  cell_cohorts=True)
        groups = coalesce(sources, products)
        decoded = {row["kind"]: {at for first, count in row["source_ranges"]
                                for at in range(first, first + count)} for row in groups}
        self.assertEqual(decoded["unknown"], {1})
        self.assertEqual(decoded["fine"], {0, 2, 3})
        self.assertTrue(decoded["fine"].isdisjoint(decoded["unknown"]))
        with self.assertRaises(ValueError):
            coalesce(sources, products + products[:1])
        with self.assertRaises(ValueError):
            coalesce(sources, products[:-1])

    def test_truncated_and_foreign_dimensions_fail(self):
        data, _, _ = index()
        raw = inventory()
        for size in range(len(raw)):
            with self.assertRaises(ValueError):
                geometry_inputs(raw[:size], data, "fixture")
        with self.assertRaises(ValueError):
            geometry_inputs(inventory(((1, 6, 0.25),)), data, "fixture")


if __name__ == "__main__":
    unittest.main()

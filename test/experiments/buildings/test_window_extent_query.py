"""Independent scalar radius oracles, actual recipe containment and no mesh work."""

import unittest
from dataclasses import replace
from fractions import Fraction
from unittest.mock import patch

import numpy as np
from test_window_detail_demand import cases
from window_detail_demand import WindowDetails, window_closed
from window_extent_plan import compile_bounds
from window_extent_query import WindowBounds, WindowRange


def scalar_ids(metadata, center, radius):
    center = [Fraction(float(v)) for v in center]
    radius = Fraction(float(radius))
    result = []
    for identifier, box in zip(metadata.identifiers, metadata.boxes, strict=True):
        distance = sum(
            max(Fraction(float(low)) - c, c - Fraction(float(high)), 0) ** 2
            for low, high, c in zip(box[0], box[1], center, strict=True)
        )
        if distance <= radius * radius:
            result.append(identifier)
    return tuple(result)


class WindowExtents(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = cases()

    def test_every_actual_closed_window_vertex_is_inside_its_planned_bounds(self):
        count = 0
        for prepared in self.plans:
            metadata = compile_bounds(prepared)
            self.assertEqual(
                metadata.identifiers,
                tuple(w.identifier for w in prepared.facade.windows),
            )
            for window, box in zip(
                prepared.facade.windows, metadata.boxes, strict=True
            ):
                actual = window_closed(prepared, window)["vertices"]
                self.assertTrue(np.all(actual >= box[0]) and np.all(actual <= box[1]))
                count += 1
        self.assertEqual(count, 2936)

    def test_metadata_compilation_never_calls_mesh_or_template_builders(self):
        with (
            patch("opening_modules.template", side_effect=AssertionError("template")),
            patch(
                "terraced_facade_details.details", side_effect=AssertionError("detail")
            ),
            patch("building_mesh.solid", side_effect=AssertionError("mesh")),
        ):
            for prepared in self.plans:
                self.assertEqual(
                    len(compile_bounds(prepared).boxes), len(prepared.facade.windows)
                )

    def test_queries_preserve_plan_order_against_independent_fraction_scan(self):
        for prepared in self.plans:
            metadata = compile_bounds(prepared)
            scan, tree = WindowRange(metadata), WindowRange(metadata, indexed=True)
            for point, radius in (
                (metadata.boxes[0].mean(0), 5),
                (metadata.boxes[-1, 0], 0),
                (metadata.boxes[0, 1] + [3, 4, 0], 5),
                (metadata.boxes[0].mean(0) + [0, 0, 1000], 2),
            ):
                expected = scalar_ids(metadata, point, radius)
                self.assertEqual(scan.query(point, radius).identifiers, expected)
                self.assertEqual(tree.query(point, radius).identifiers, expected)

    def test_extent_contact_is_retained_when_a_center_only_query_misses(self):
        metadata = WindowBounds(
            ("wide", "distant"),
            np.array([[[-2, -0.2, 0], [2, 0.2, 2]], [[20, 20, 20], [21, 21, 21]]]),
        )
        center = np.array([2.0, 0.2, 2.0])
        self.assertGreater(np.linalg.norm(metadata.boxes[0].mean(0) - center), 0)
        for indexed in (False, True):
            self.assertEqual(
                WindowRange(metadata, indexed=indexed).query(center, 0).identifiers,
                ("wide",),
            )

    def test_rounding_tangencies_subnormals_and_degenerate_xy_bounds(self):
        metadata = WindowBounds(
            ("point", "flat"),
            np.array([[[0, 0, 0], [0, 0, 0]], [[2, -1, -1], [2, 1, 1]]], float),
        )
        for indexed in (False, True):
            query = WindowRange(metadata, indexed=indexed)
            for center, radius in (
                ([3, 4, 0], 5.0),
                ([3, 4, 0], np.nextafter(5.0, 0)),
                ([1e-300, 0, 0], 1e-300),
                ([np.nextafter(0.0, 1), 0, 0], 0),
                ([2, 0, 0], 0),
            ):
                self.assertEqual(
                    query.query(center, radius).identifiers,
                    scalar_ids(metadata, center, radius),
                )

    def test_forward_rotated_and_sheared_core_bounds_contain_actual_products(self):
        prepared = self.plans[0]
        core = replace(
            prepared.mass.core,
            origin_xy=(1e6, -2e6),
            across_xy=(1.0, 0.3),
            along_xy=(0.4, 1.0),
        )
        changed = replace(prepared, mass=replace(prepared.mass, core=core))
        metadata = compile_bounds(changed)
        for window, box in zip(
            changed.facade.windows[::23], metadata.boxes[::23], strict=True
        ):
            vertices = window_closed(changed, window)["vertices"]
            self.assertTrue(np.all(vertices >= box[0]) and np.all(vertices <= box[1]))

    def test_empty_immutable_metadata_and_invalid_queries_reject_before_tree_work(self):
        empty = WindowBounds((), np.empty((0, 2, 3)))
        for indexed in (False, True):
            self.assertEqual(
                WindowRange(empty, indexed=indexed).query([0, 0, 0], 1).identifiers, ()
            )
        metadata = compile_bounds(self.plans[0])
        with self.assertRaises(ValueError):
            metadata.boxes.flags.writeable = True
        query = WindowRange(metadata, indexed=True)
        for point, radius in (
            ([0, 0], 1),
            ([0, 0, float("nan")], 1),
            ([0, 0, 0], -1),
            ([0, 0, 0], True),
            ([0, 0, 0], 10**1000),
            (["0", "0", "0"], 1),
        ):
            with self.assertRaises(ValueError):
                query.query(point, radius)
        for ids, boxes in (
            (("a", "a"), np.zeros((2, 2, 3))),
            (("a",), np.ones((1, 2, 2))),
            (("a",), np.array([[[1, 1, 1], [0, 0, 0]]])),
        ):
            with self.assertRaises(ValueError):
                WindowBounds(ids, boxes)

    def test_query_changes_build_only_entering_detail_ids_and_preserve_references(self):
        prepared = self.plans[0]
        metadata = compile_bounds(prepared)
        query, product = WindowRange(metadata), WindowDetails(prepared)
        center = metadata.boxes[0].mean(0)
        previous = set()
        for offset in (0, 0, 2, 4, 4, 1000):
            selected = query.query(center + [offset, 0, 0], 5).identifiers
            old = dict(product.resident)
            result = product.update(selected)
            self.assertEqual(set(result.generated_ids), set(selected) - previous)
            self.assertEqual(set(result.released_ids), previous - set(selected))
            for key in previous & set(selected):
                self.assertIs(old[key], product.resident[key])
            previous = set(selected)


if __name__ == "__main__":
    unittest.main()

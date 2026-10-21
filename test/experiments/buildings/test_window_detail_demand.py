"""Actual-source ownership, full-recipe geometry and bounded residency checks."""

import gc
import unittest
import weakref
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.rooflight_panels import triangle_surface
from terraced_admission_probe import controls
from terraced_facade_details import details
from terraced_facade_prepare import positions, prepare
from terraced_facade_surface import compact
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from window_detail_demand import (
    MAX_WINDOWS,
    WindowDetails,
    buffer_bytes,
    materialize,
    window_closed,
    window_patch,
)


def cases():
    rows = sources()
    compact = next(row for row in controls() if row["shape"] == "compact")
    return [
        prepare(plan(row, kind))
        for row in (*rows, compact)
        for kind in ("cascade", "paired-wings")
    ]


def triangles(mesh):
    values = np.c_[mesh["vertices"][mesh["faces"]].reshape(-1, 9), mesh["materials"]]
    order = np.lexsort(values[:, ::-1].T)
    return values[order]


def draw_triangles(mesh):
    values = np.c_[
        tuple(
            mesh[key][mesh["faces"]].reshape(len(mesh["faces"]), -1)
            for key in ("vertices", "normals", "uv", "facade_extent")
        )
        + (mesh["materials"],)
    ]
    return values[np.lexsort(values[:, ::-1].T)]


class WindowDemandTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = cases()
        cls.prepared = cls.plans[0]

    def test_all_actual_windows_match_the_existing_full_batch_recipe(self):
        for prepared in self.plans:
            with self.subTest(source=prepared.mass.identifier, kind=prepared.mass.kind):
                original = details(prepared.local, prepared.facade, legacy_trim=False)
                original["vertices"] = positions(
                    original["vertices"], prepared.mass.core
                )
                expected = triangles(original)
                actual = np.concatenate(
                    [
                        triangles(window_closed(prepared, window))
                        for window in prepared.facade.windows
                    ]
                )
                actual = actual[np.lexsort(actual[:, ::-1].T)]
                np.testing.assert_array_equal(expected, actual)
                full_draw = triangle_surface(original, 10)
                full_draw["materials"] = original["materials"]
                actual_draw = np.concatenate(
                    [
                        draw_triangles(window_patch(prepared, w).mesh)
                        for w in prepared.facade.windows
                    ]
                )
                actual_draw = actual_draw[np.lexsort(actual_draw[:, ::-1].T)]
                np.testing.assert_array_equal(draw_triangles(full_draw), actual_draw)

    def test_closed_components_keep_positive_volume_and_two_sided_edges(self):
        for prepared in self.plans:
            for window in prepared.facade.windows[:2]:
                raw = window_closed(prepared, window)
                for part in np.unique(raw["parts"]):
                    points = raw["vertices"][raw["faces"][raw["parts"] == part]]
                    points = points - points[0, 0]
                    self.assertGreater(
                        np.einsum(
                            "ij,ij->",
                            points[:, 0],
                            np.cross(points[:, 1], points[:, 2]),
                        )
                        / 6,
                        0,
                    )
                    vertices, ids = np.unique(
                        points.reshape(-1, 3), axis=0, return_inverse=True
                    )
                    self.assertTrue(np.isfinite(vertices).all())
                    ids = ids.reshape(-1, 3)
                    edges = ids[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
                    forward = edges[:, 0] < edges[:, 1]
                    _, groups, count = np.unique(
                        np.sort(edges, axis=1),
                        axis=0,
                        return_inverse=True,
                        return_counts=True,
                    )
                    np.testing.assert_array_equal(count, 2)
                    np.testing.assert_array_equal(
                        np.bincount(groups, weights=forward), 1
                    )

    def test_partial_draw_retains_every_other_coarse_owner_and_exact_near_products(
        self,
    ):
        product = WindowDetails(self.prepared)
        identifiers = tuple(product.windows)[::17][:8]
        before = product.coarse.mesh["vertices"].copy()
        draw = product.update(identifiers)
        self.assertEqual(draw.detailed_ids, identifiers)
        self.assertEqual(draw.generated_ids, identifiers)
        self.assertEqual(draw.source_id, self.prepared.mass.identifier)
        self.assertEqual(
            tuple(s for s in draw.spans if s.mesh is product.coarse.mesh),
            tuple(s for s in product.base_spans if s.identifier not in identifiers),
        )
        for identifier in identifiers:
            near = next(s for s in draw.spans if s.identifier == identifier)
            expected = window_patch(self.prepared, product.windows[identifier]).mesh
            for key in expected:
                np.testing.assert_array_equal(near.mesh[key], expected[key])
        np.testing.assert_array_equal(before, product.coarse.mesh["vertices"])
        self.assertEqual(
            len(materialize(draw)["faces"]), sum(s.face_count for s in draw.spans)
        )
        self.assertTrue(
            all(
                not value.flags.writeable
                for p in product.resident.values()
                for value in p.mesh.values()
            )
        )
        with self.assertRaises(ValueError):
            next(iter(product.resident.values())).mesh["vertices"].setflags(write=True)

    def test_every_source_keeps_the_original_complete_coarse_product(self):
        for prepared in self.plans:
            baseline = compact(prepared)
            product = WindowDetails(prepared)
            for key in baseline.mesh:
                np.testing.assert_array_equal(
                    baseline.mesh[key], product.coarse.mesh[key]
                )
            self.assertEqual(baseline.owner_ids, product.coarse.owner_ids)
            np.testing.assert_array_equal(
                baseline.triangle_owners, product.coarse.triangle_owners
            )
            draw = product.update(tuple(product.windows)[::31][:5])
            for span in product.base_spans:
                if span.identifier not in draw.detailed_ids:
                    self.assertIn(span, draw.spans)

    def test_motion_generates_only_entering_ids_and_releases_departing_ownership(self):
        product = WindowDetails(self.prepared)
        identifiers = tuple(product.windows)
        first = product.update(identifiers[:3])
        kept = product.resident[identifiers[1]]
        departed = weakref.ref(product.resident[identifiers[0]].mesh["vertices"])
        with patch(
            "window_detail_demand.window_patch", side_effect=AssertionError("rebuilt")
        ):
            again = product.update(tuple(reversed(identifiers[:3])))
        self.assertFalse(again.generated_ids)
        self.assertEqual(again.generated_buffer_bytes, 0)
        moved = product.update(identifiers[1:4])
        self.assertEqual(moved.generated_ids, (identifiers[3],))
        self.assertEqual(moved.released_ids, (identifiers[0],))
        self.assertIs(kept, product.resident[identifiers[1]])
        self.assertEqual(
            moved.resident_detail_bytes,
            sum(buffer_bytes(p.mesh) for p in product.resident.values()),
        )
        del first, again
        gc.collect()
        self.assertIsNone(departed())
        cleared = product.update(())
        self.assertFalse(product.resident)
        self.assertFalse(cleared.detailed_ids)
        self.assertEqual(cleared.spans, product.base_spans)

    def test_foreign_duplicates_types_and_failure_are_transactional(self):
        product = WindowDetails(self.prepared)
        identifiers = tuple(product.windows)
        product.update(identifiers[:1])
        previous = product.resident
        with patch(
            "window_detail_demand.window_patch", side_effect=AssertionError("expanded")
        ):
            for invalid in (
                ("foreign",),
                (identifiers[0], identifiers[0]),
                (None,),
                "bad",
                iter(()),
            ):
                with self.assertRaises((ValueError, TypeError)):
                    product.update(invalid)
        with (
            patch(
                "window_detail_demand.window_patch",
                side_effect=[
                    window_patch(self.prepared, product.windows[identifiers[1]]),
                    RuntimeError("failed"),
                ],
            ),
            self.assertRaises(RuntimeError),
        ):
            product.update(identifiers[1:3])
        self.assertIs(product.resident, previous)

    def test_capacity_falls_back_without_missing_windows_or_source_order_instability(
        self,
    ):
        identifiers = tuple(w.identifier for w in self.prepared.facade.windows)
        product = WindowDetails(self.prepared, max_windows=2)
        draw = product.update(tuple(reversed(identifiers[:5])))
        self.assertEqual(draw.detailed_ids, identifiers[:2])
        self.assertEqual(draw.fallback_ids, identifiers[2:5])
        self.assertTrue(set(identifiers) <= {s.identifier for s in draw.spans})
        empty = WindowDetails(self.prepared, max_windows=0)
        with patch(
            "window_detail_demand.window_patch", side_effect=AssertionError("expanded")
        ):
            self.assertEqual(empty.update(identifiers).fallback_ids, identifiers)
        with patch(
            "window_detail_demand.compact", side_effect=AssertionError("constructed")
        ):
            for count in (-1, True, MAX_WINDOWS + 1):
                with self.assertRaises(ValueError):
                    WindowDetails(self.prepared, max_windows=count)
            invalid = replace(
                self.prepared,
                facade=replace(
                    self.prepared.facade, windows=(self.prepared.facade.windows[0],) * 2
                ),
            )
            with self.assertRaises(ValueError):
                WindowDetails(invalid)

    def test_no_floor_stair_or_boolean_builder_is_needed(self):
        with (
            patch(
                "terraced_facade_geometry.build", side_effect=AssertionError("floors")
            ),
            patch(
                "terraced_facade_geometry.subtract_prisms",
                side_effect=AssertionError("CSG"),
            ),
        ):
            product = WindowDetails(self.prepared)
            draw = product.update(tuple(product.windows)[:2])
        self.assertEqual(len(draw.detailed_ids), 2)


if __name__ == "__main__":
    unittest.main()

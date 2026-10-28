"""Actual source composition, complete coarse fallback and atomic bounded details."""

import unittest
from copy import copy
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from facade_programme_products import programme_products
from programme_window_demand import ProgrammeWindowDetails, retained_spans
from programme_window_reference import source_product
from terraced_crown_geometry import capless
from terraced_facade_surface import joined
from test_window_detail_demand import draw_triangles
from window_detail_demand import MAX_WINDOWS, MeshSpan, WindowDetails, materialize


def actual_product():
    return source_product()


def equal_arrays(test, a, b):
    test.assertEqual(a.keys(), b.keys())
    for key in a:
        test.assertEqual((a[key].dtype, a[key].shape), (b[key].dtype, b[key].shape))
        test.assertEqual(a[key].tobytes(), b[key].tobytes(), key)


def equal_surface(test, a, b):
    test.assertEqual(a.keys(), b.keys())
    for key in a:
        test.assertEqual(a[key].dtype, b[key].dtype)
    np.testing.assert_array_equal(draw_triangles(a), draw_triangles(b))


def reference_body(full, request, drawn):
    admitted, _fallback = request.request(
        drawn.windows.detailed_ids + drawn.windows.fallback_ids
    )
    spans = tuple(s for s in full.base_spans if s.identifier not in admitted)
    spans += tuple(
        MeshSpan(full.resident[i].mesh, 0, len(full.resident[i].mesh["faces"]), i)
        for i in admitted
    )
    return materialize(replace(drawn.windows, spans=spans))


class ProgrammeWindows(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.product = actual_product()
        cls.facade = cls.product.body.facade.source
        cls.identifiers = tuple(
            w.identifier for w in cls.facade.prepared.facade.windows
        )
        cls.cells = tuple(c.identifier for c in cls.product.plan.cells[:3])
        cls.full = WindowDetails(
            cls.facade.prepared,
            portals=cls.facade.portals,
            max_windows=MAX_WINDOWS,
            facade_product=cls.facade,
        )
        cls.full.update(cls.identifiers)

    def test_whole_composition_matches_all_prebuilt_windows_for_identical_requests(
        self,
    ):
        subject = ProgrammeWindowDetails(self.product, max_windows=8)
        for ids in (
            (),
            self.identifiers[:8],
            self.identifiers[:8],
            self.identifiers[4:12],
            self.identifiers[:12],
            (),
        ):
            with self.subTest(request=ids):
                result = subject.update(ids, selected_ids=self.cells)
                reference = self.product.compose(
                    reference_body(self.full, subject.windows, result),
                    portal_frames=True,
                    layer_mode="outer",
                    roof_near=False,
                    selected_ids=frozenset(self.cells),
                )
                equal_surface(self, materialize(result), reference)
                self.assertLessEqual(len(subject.windows.resident), 8)
                self.assertEqual(
                    len(result.windows.detailed_ids) + len(result.windows.fallback_ids),
                    len(ids),
                )
                self.assertIs(subject.windows.coarse, self.facade.coarse)
        self.assertFalse(subject.windows.resident)

    def test_no_complete_facade_floors_stairs_or_coarse_rebuild_for_window_requests(
        self,
    ):
        with (
            patch(
                "window_detail_demand.compact",
                side_effect=AssertionError("rebuilt coarse"),
            ),
            patch.object(
                self.facade, "physical", side_effect=AssertionError("full physical")
            ),
            patch.object(
                self.product.source,
                "physical_roof",
                side_effect=AssertionError("full roof"),
            ),
            patch(
                "window_detail_demand.materialize",
                side_effect=AssertionError("draw copies"),
            ),
        ):
            subject = ProgrammeWindowDetails(self.product, max_windows=8)
            self.assertIs(subject.windows.coarse, self.facade.coarse)
            for ids in ((), self.identifiers[:8], self.identifiers[4:12], ()):
                output = subject.update(ids, selected_ids=())
                self.assertGreater(sum(span.face_count for span in output.spans), 0)
        self.assertIsNone(self.product.body.facade.near)
        self.assertIsNone(self.facade.near)

    def test_only_requested_missing_windows_are_generated_and_old_patches_reused(self):
        import window_detail_demand

        subject = ProgrammeWindowDetails(self.product, max_windows=8)
        with patch(
            "window_detail_demand.window_patch", wraps=window_detail_demand.window_patch
        ) as generate:
            a = subject.update(self.identifiers[:8], selected_ids=())
            resident = dict(subject.windows.resident)
            b = subject.update(tuple(reversed(self.identifiers[:8])), selected_ids=())
            self.assertEqual(generate.call_count, 8)
            self.assertEqual(b.windows.generated_ids, ())
            self.assertEqual(b.windows.generated_buffer_bytes, 0)
            equal_arrays(self, materialize(a), materialize(b))
            subject.update(self.identifiers[4:12], selected_ids=())
            self.assertEqual(generate.call_count, 12)
            for identifier in self.identifiers[4:8]:
                self.assertIs(
                    subject.windows.resident[identifier], resident[identifier]
                )

    def test_bad_requests_and_failed_composition_preserve_previous_residency(self):
        subject = ProgrammeWindowDetails(self.product, max_windows=8)
        subject.update(self.identifiers[:8], selected_ids=())
        before = subject.windows
        for ids, options in (
            (self.identifiers[:1] * 2, {}),
            (("foreign-window",), {}),
            (self.identifiers[4:12], {"selected_ids": ("foreign-cell",)}),
            (self.identifiers[4:12], {"roof_near": "yes"}),
        ):
            with self.assertRaises(ValueError):
                subject.update(ids, **options)
            self.assertIs(subject.windows, before)
        with (
            patch.object(
                subject, "_layers", side_effect=ValueError("failed composition")
            ),
            self.assertRaisesRegex(ValueError, "failed composition"),
        ):
            subject.update(self.identifiers[4:12], selected_ids=())
        self.assertIs(subject.windows, before)
        result = subject.update(self.identifiers[:8], selected_ids=())
        self.assertEqual(result.windows.generated_ids, ())

    def test_zero_capacity_keeps_whole_coarse_body_layers_roof_and_portals(self):
        subject = ProgrammeWindowDetails(self.product, max_windows=0)
        result = subject.update(self.identifiers, selected_ids=self.cells)
        self.assertEqual(result.windows.detailed_ids, ())
        self.assertEqual(result.windows.fallback_ids, self.identifiers)
        self.assertFalse(subject.windows.resident)
        reference = self.product.compose(
            self.facade.coarse.mesh,
            portal_frames=True,
            layer_mode="outer",
            roof_near=False,
            selected_ids=frozenset(self.cells),
        )
        equal_surface(self, materialize(result), reference)
        self.assertEqual(len(self.facade.portals), 2)
        self.assertIsNotNone(self.product.source.plan.crown)

    def test_empty_programme_preserves_legacy_trim(self):
        planned = replace(self.product.plan, cells=())
        product = programme_products(self.product.source.plan, planned)
        self.assertTrue(product.body.facade.source.legacy_trim)
        result = ProgrammeWindowDetails(product, max_windows=0).update(
            (), selected_ids=()
        )
        equal_surface(
            self,
            materialize(result),
            product.draw(body_near=False, roof_near=False, selected_ids=()),
        )

    def test_borrowed_coarse_rejects_foreign_preparation_and_portals(self):
        with self.assertRaises(ValueError):
            WindowDetails(
                copy(self.facade.prepared),
                portals=self.facade.portals,
                facade_product=self.facade,
            )
        with self.assertRaises(ValueError):
            WindowDetails(self.facade.prepared, portals=(), facade_product=self.facade)
        with self.assertRaises(TypeError):
            ProgrammeWindowDetails(None)

    def test_unchanged_layers_are_not_extruded_again_and_source_changes_reject(self):
        subject = ProgrammeWindowDetails(self.product, max_windows=8)
        first = subject.update(self.identifiers[:8], selected_ids=self.cells)
        self.assertGreater(first.generated_layer_bytes, 0)
        with patch(
            "programme_window_demand.expand", side_effect=AssertionError("re-expanded")
        ):
            second = subject.update(self.identifiers[4:12], selected_ids=self.cells)
        self.assertEqual(second.generated_layer_bytes, 0)
        before = subject.windows
        with (
            patch.object(self.product, "plan", replace(self.product.plan)),
            self.assertRaisesRegex(ValueError, "source changed"),
        ):
            subject.update(self.identifiers[:8], selected_ids=self.cells)
        self.assertIs(subject.windows, before)

    def test_alternate_real_source_vertical_programme_and_near_roof_match_dense_reference(
        self,
    ):
        product = source_product(3, "paired-wings", "vertical-slots")
        subject = ProgrammeWindowDetails(product, max_windows=8)
        identifiers = tuple(subject.windows.windows)[:8]
        cells = tuple(cell.identifier for cell in product.plan.cells[:3])
        for near in (False, True):
            output = subject.update(identifiers, selected_ids=cells, roof_near=near)
            reference = product.compose(
                materialize(output.windows),
                portal_frames=True,
                layer_mode="outer",
                roof_near=near,
                selected_ids=frozenset(cells),
            )
            equal_surface(self, materialize(output), reference)
        self.assertIsNone(product.body.facade.near)
        self.assertIsNone(product.body.facade.source.near)

    def test_old_draw_keeps_released_layer_buffers_alive_until_last_consumer_releases(
        self,
    ):
        import gc
        import weakref

        subject = ProgrammeWindowDetails(self.product, max_windows=8)
        first = subject.update(self.identifiers[:8], selected_ids=self.cells)
        reference = weakref.ref(subject.layers["vertices"])
        original = reference().tobytes()
        subject.update((), selected_ids=self.cells[1:])
        self.assertEqual(reference().tobytes(), original)
        del first
        gc.collect()
        self.assertIsNone(reference())

    def test_mixed_cap_and_wall_ranges_match_dense_whole_source_cap_removal(self):
        body = joined(
            (
                self.facade.coarse.mesh,
                *self.product.body.additions,
                self.product.body.portal_frames,
            )
        )
        height = self.product.source.plan.body.top_m
        span = MeshSpan(body, 0, len(body["faces"]), self.product.plan.source_id)
        spans, count = retained_spans(span, height)
        self.assertGreater(count, 0)
        self.assertGreater(sum(s.face_count for s in spans), 0)
        draw = replace(copy(self.full).update(()), spans=spans)
        equal_surface(self, materialize(draw), capless(body, height))
        self.assertEqual(len(self.full.resident), len(self.identifiers))


if __name__ == "__main__":
    unittest.main()

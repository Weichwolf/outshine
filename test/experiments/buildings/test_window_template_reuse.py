"""Existing components remain identical while update-local template work is bounded."""

import gc
import unittest
import weakref
from dataclasses import replace
from functools import lru_cache
from unittest.mock import patch

import opening_details
import window_detail_demand as demand
from test_window_detail_demand import cases


def assert_mesh_equal(test, first, second):
    test.assertEqual(first.keys(), second.keys())
    for key in first:
        test.assertEqual(first[key].dtype, second[key].dtype, key)
        test.assertEqual(first[key].shape, second[key].shape, key)
        test.assertEqual(first[key].tobytes(), second[key].tobytes(), key)


class TemplateReuseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = cases()

    def test_all_original_window_draw_fields_and_raw_parts_are_unchanged(self):
        for prepared in self.plans:
            product = demand.WindowDetails(prepared, max_windows=demand.MAX_WINDOWS)
            original = opening_details.template
            with patch.object(opening_details, "template", wraps=original) as build:
                product.update(tuple(product.windows))
            self.assertLessEqual(build.call_count, 16)
            templates = lru_cache(maxsize=16)(original)
            for window in prepared.facade.windows:
                expected = demand.window_patch(prepared, window)
                assert_mesh_equal(
                    self, expected.mesh, product.resident[window.identifier].mesh
                )
                assert_mesh_equal(
                    self,
                    demand.window_closed(prepared, window),
                    demand.window_closed(prepared, window, template_builder=templates),
                )

    def test_context_is_lazy_local_cleared_and_outputs_own_their_buffers(self):
        product = demand.WindowDetails(self.plans[0])
        ids = tuple(product.windows)
        raw_references, contexts = [], []
        original_template, original_patch = (
            opening_details.template,
            demand.window_patch,
        )

        def template(*key):
            value = original_template(*key)
            raw_references.extend(weakref.ref(array) for array in value.values())
            return value

        def window(*args, template_builder=None):
            contexts.append(template_builder)
            return original_patch(*args, template_builder=template_builder)

        with (
            patch.object(opening_details, "template", template),
            patch.object(demand, "window_patch", window),
        ):
            first = product.update(ids[:8])
        self.assertTrue(all(context is contexts[0] for context in contexts))
        self.assertEqual(contexts[0].cache_info().maxsize, 16)
        self.assertEqual(contexts[0].cache_info().currsize, 0)
        gc.collect()
        self.assertTrue(all(ref() is None for ref in raw_references))
        previous = dict(product.resident)
        with patch.object(
            demand, "lru_cache", side_effect=AssertionError("idle context")
        ):
            again = product.update(ids[:8])
            product.update(())
        self.assertFalse(again.generated_ids)
        self.assertTrue(first.spans)
        self.assertTrue(all(value.mesh["vertices"].size for value in previous.values()))

    def test_failure_clears_context_and_preserves_previous_residency(self):
        product = demand.WindowDetails(self.plans[0])
        ids = tuple(product.windows)
        product.update(ids[:1])
        previous = product.resident
        contexts = []
        original = demand.window_patch

        def failing(*args, template_builder=None):
            contexts.append(template_builder)
            if len(contexts) == 2:
                raise RuntimeError("second patch failed")
            return original(*args, template_builder=template_builder)

        with (
            patch.object(demand, "window_patch", failing),
            self.assertRaisesRegex(RuntimeError, "second patch"),
        ):
            product.update(ids[1:3])
        self.assertIs(product.resident, previous)
        self.assertEqual(contexts[0].cache_info().currsize, 0)
        self.assertIs(contexts[0], contexts[1])

    def test_distinct_dimensions_eviction_does_not_change_geometry(self):
        prepared = self.plans[0]
        windows = tuple(
            replace(w, width_m=w.width_m - 0.005 * (i if i < 20 else 0))
            for i, w in enumerate(prepared.facade.windows[:21])
        )
        varied = replace(prepared, facade=replace(prepared.facade, windows=windows))
        product = demand.WindowDetails(varied)
        original, build = demand.window_patch, opening_details.template
        sizes = []

        def window(*args, template_builder=None):
            result = original(*args, template_builder=template_builder)
            sizes.append(template_builder.cache_info().currsize)
            return result

        with (
            patch.object(demand, "window_patch", window),
            patch.object(opening_details, "template", wraps=build) as count,
        ):
            product.update(tuple(product.windows))
        self.assertEqual(count.call_count, 21)
        self.assertEqual(max(sizes), 16)
        for w in windows:
            assert_mesh_equal(
                self, original(varied, w).mesh, product.resident[w.identifier].mesh
            )

    def test_all_distinct_or_single_new_dimensions_need_no_shared_context(self):
        prepared = self.plans[0]
        windows = tuple(
            replace(w, width_m=w.width_m - 0.005 * i)
            for i, w in enumerate(prepared.facade.windows[:21])
        )
        varied = replace(prepared, facade=replace(prepared.facade, windows=windows))
        product = demand.WindowDetails(varied)
        with patch.object(demand, "lru_cache", side_effect=AssertionError("no reuse")):
            product.update(tuple(product.windows))
            product.update(())
            product.update((windows[0].identifier,))
        assert_mesh_equal(
            self,
            demand.window_patch(varied, windows[0]).mesh,
            product.resident[windows[0].identifier].mesh,
        )

    def test_legacy_expand_variants_defaults_and_empty_inputs(self):
        openings = [
            {
                "kind": kind,
                "width_m": 1.4,
                "height_m": 1.8,
                "sill": sill,
                "centre": (4.0, -2.0),
                "tangent": (0.0, 1.0),
                "bottom_z_m": 3.0,
                "storey": 2,
            }
            for kind in ("window", "entrance", "terrace")
            for sill in (False, True)
        ]
        for method in ("ring", "bars"):
            for reuse in (False, True):
                expected = opening_details.expand(openings * 2, method, reuse)
                actual = opening_details.expand(
                    openings * 2,
                    method,
                    reuse,
                    template_builder=lru_cache(maxsize=16)(opening_details.template),
                )
                assert_mesh_equal(self, expected, actual)
        assert_mesh_equal(
            self,
            opening_details.expand([]),
            opening_details.expand(
                [], template_builder=lambda *args: self.fail("empty expansion")
            ),
        )


if __name__ == "__main__":
    unittest.main()

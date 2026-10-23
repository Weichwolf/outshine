"""Canonical caster proxies, independent boxes and unchanged resource-role controls."""

import functools
import unittest

import numpy as np
from compare_urban_layers import mixed
from test_urban_parent_conditional import brute_image
from test_urban_ready_pixels import fixture
from test_urban_receiver_ownership import one_box
from triangle_image_reference import image_rays
from urban_caster_proxy import select_caster_proxy
from urban_ready_pixels import composed_scene, integrate_control
from urban_sampled_layer import render_sampled_layer


def proxy_fixture(empty=False):
    parent, field, o, d, profile = one_box()
    parent.frame[3] = 32
    parent.centre_fractions = np.ones((2, 2, 3))
    field = field | {
        "shape": np.array([2, 2]),
        "lookup": np.array([[0, -1], [-1, -1]], np.int32),
        "spans": np.array([[0.0, 12.0]]),
        "height_bounds": np.array([0.0, 12.0]),
        "column_geometry": "single_grounded_prism",
    }
    if empty:
        field |= {
            "columns": np.empty((0, 2), np.int32),
            "spans": np.empty((0, 2)),
            "offsets": np.array([0], np.uint32),
            "lookup": np.full((2, 2), -1, np.int32),
        }
    return parent, field, o, d, profile


class CasterProxyTest(unittest.TestCase):
    def test_selection_borrows_the_existing_field_and_preserves_camera_products(self):
        parent, field, _, _, _ = proxy_fixture()
        region = np.array([[[0, 0], [1, 2]]])
        camera, domain, guard = select_caster_proxy(
            parent, None, field, None, region, True
        )
        self.assertIs(camera["spans"], field["spans"])
        self.assertIs(camera["columns"], field["columns"])
        self.assertTrue(np.shares_memory(camera["lookup"], field["lookup"]))
        np.testing.assert_array_equal(camera["shape"], [1, 2])
        np.testing.assert_array_equal(camera["lookup"], field["lookup"][:, :1])
        self.assertIs(domain, region)
        self.assertIsNotNone(guard)
        self.assertEqual(len(guard["primary_covered"]), 0)
        for enabled in [False, True]:
            camera, domain, guard = select_caster_proxy(
                parent, field, field.copy(), region, region, enabled
            )
            self.assertIs(camera, field)
            self.assertIs(domain, region)
            self.assertIsNone(guard)

        camera, domain, guard = select_caster_proxy(
            parent, None, field, region, region, True
        )
        self.assertIsNone(camera)
        self.assertIs(domain, region)
        self.assertIsNone(guard)
        for bad in [region.astype(float), np.array([[[1, 0], [0, 2]]])]:
            self.assertIsNone(
                select_caster_proxy(parent, None, field, None, bad, True)[2]
            )

    def test_tall_and_empty_cells_match_independent_physical_boxes(self):
        for empty in [False, True]:
            parent, field, o, d, profile = proxy_fixture(empty)
            old = composed_scene(parent, field, profile, "half_caster", 0.75)
            new = composed_scene(
                parent, field, profile, "half_caster", 0.75, caster_primary_proxy=True
            )
            actual, _, work = new(o, d)
            expected = brute_image(
                [0, 0, 0, 0] if empty else [12, 0, 0, 0], o, d, profile
            )[0]
            with self.subTest(empty=empty):
                np.testing.assert_allclose(actual, expected, rtol=0, atol=2e-14)
                self.assertGreater(float(np.max(abs(old(o, d)[0] - expected))), 0)
                self.assertEqual(work["caster_proxy_rays"], 1)
                self.assertEqual(work["child_primary_work"], {})

    def test_all_other_ready_states_retain_their_complete_composition(self):
        parent, resolved, view, profile, _ = fixture()
        o, d = image_rays(view, np.array([[637, 357], [641, 361]]), 4, 4, 2)
        d /= np.linalg.norm(d, axis=1)[:, None]
        for state in ["pending", "half_view", "half_ready", "all_ready", "foreground"]:
            a = composed_scene(
                parent,
                resolved,
                profile,
                state,
                0.75,
                receiver_ownership=True,
                camera_miss_witness=True,
            )
            b = composed_scene(
                parent,
                resolved,
                profile,
                state,
                0.75,
                receiver_ownership=True,
                camera_miss_witness=True,
                caster_primary_proxy=True,
            )
            np.testing.assert_array_equal(a(o, d)[0], b(o, d)[0])
        for component in ["complete", "moments", "background"]:
            a = composed_scene(
                parent,
                resolved,
                profile,
                "half_caster",
                0.75,
                component=component,
                receiver_ownership=True,
                camera_miss_witness=True,
                caster_primary_proxy=True,
            )
            b = composed_scene(
                parent,
                resolved,
                profile,
                "half_ready",
                0.75,
                component=component,
                receiver_ownership=True,
                camera_miss_witness=True,
            )
            np.testing.assert_array_equal(a(o, d)[0], b(o, d)[0])

    def test_split_integration_recovers_the_same_physical_ready_image(self):
        parent, resolved, view, profile, args = fixture()
        args.samples, args.background_samples, args.weighted_transport = 2, 4, True
        box = np.array([[637, 357], [641, 361]])
        outputs = []
        for state, proxy in [("half_caster", True), ("half_ready", False)]:
            args.state = state
            model = composed_scene(
                parent,
                resolved,
                profile,
                state,
                0.75,
                component="moments",
                receiver_ownership=True,
                camera_miss_witness=True,
                caster_primary_proxy=proxy,
            )
            background = composed_scene(
                parent,
                resolved,
                profile,
                state,
                0.75,
                component="background",
                caster_primary_proxy=proxy,
            )
            outputs.append(
                integrate_control(resolved, view, box, profile, args, model, background)
            )
        np.testing.assert_array_equal(outputs[0][0], outputs[1][0])
        for key in outputs[0][1]:
            np.testing.assert_array_equal(outputs[0][1][key], outputs[1][1][key])
        self.assertGreater(
            outputs[0][2]["moment_phase"]["scene_work"]["caster_proxy_rays"], 0
        )

    def test_inside_possible_mass_keeps_the_original_fail_closed_guard(self):
        parent, field, o, d, profile = proxy_fixture()
        o[:, 2] = 3
        for proxy in [False, True]:
            q = composed_scene(
                parent, field, profile, "half_caster", 0.75, caster_primary_proxy=proxy
            )
            with self.subTest(proxy=proxy), self.assertRaises(ValueError):
                q(o, d)

    def test_unproved_layout_or_field_does_not_create_a_primary_proxy(self):
        parent, field, o, d, profile = proxy_fixture()
        for source in [
            field | {"column_geometry": "unproved"},
            field | {"reference_origin_m": np.array([0.0, 0, 4])},
            field | {"header": field["header"] | {"footprint_fraction": 0.5}},
        ]:
            a = composed_scene(parent, source, profile, "half_caster", 0.75)
            b = composed_scene(
                parent, source, profile, "half_caster", 0.75, caster_primary_proxy=True
            )
            np.testing.assert_array_equal(a(o, d)[0], b(o, d)[0])
        region = np.array([[[0, 0], [1, 2]]])
        self.assertIsNone(
            select_caster_proxy(
                parent, None, field | {"fallback": {}}, None, region, True
            )[2]
        )
        self.assertIsNone(
            select_caster_proxy(parent, None, None, None, region, True)[2]
        )

    def test_existing_sampled_consumer_renders_the_selected_proxy(self):
        parent, field, o, d, profile = proxy_fixture()
        region = np.array([[[0, 0], [1, 2]]])
        selected, domain, guard = select_caster_proxy(
            parent, None, field, None, region, True
        )
        query = functools.partial(render_sampled_layer, samples=4)
        a, _ = mixed(
            parent,
            o,
            d,
            profile,
            selected,
            field,
            domain,
            region,
            False,
            layer_query=query,
        )
        b, _ = mixed(
            parent,
            o,
            d,
            profile,
            field,
            field,
            region,
            region,
            False,
            layer_query=query,
        )
        np.testing.assert_array_equal(a, b)
        self.assertIsNotNone(guard)

    def test_packets_preserve_results_and_source_arrays(self):
        parent, resolved, view, profile, _ = fixture()
        o, d = image_rays(view, np.array([[637, 357], [641, 361]]), 4, 4, 2)
        d /= np.linalg.norm(d, axis=1)[:, None]
        before = {
            k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)
        }
        q = composed_scene(
            parent, resolved, profile, "half_caster", 0.75, caster_primary_proxy=True
        )
        whole = q(o, d)[0]
        chunks = np.concatenate(
            [q(o[i : i + 7], d[i : i + 7])[0] for i in range(0, len(o), 7)]
        )
        np.testing.assert_allclose(whole, chunks, rtol=0, atol=2e-14)
        self.assertEqual(
            before,
            {k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)},
        )


if __name__ == "__main__":
    unittest.main()

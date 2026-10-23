"""Independent box ensembles condition shadows on actual free camera prefixes."""

import itertools
import unittest

import numpy as np
from compare_urban_layers import mixed
from test_urban_parent_conditional import brute_hit
from test_urban_receiver_ownership import one_box
from urban_opaque_receivers import has_camera_miss_witness
from urban_parent_conditional import grid, primary_cap
from urban_parent_layer import render_layer
from urban_parent_light import light_at
from urban_sampled_layer import render_sampled_layer
from urban_shadow_runs import RowRunShadows


def empty_field():
    parent, resolved, origins, directions, profile = one_box()
    resolved = resolved | {
        "columns": np.empty((0, 2), np.int32),
        "spans": np.empty((0, 2)),
        "offsets": np.array([0], np.uint32),
        "lookup": np.array([[-1]], np.int32),
        "column_geometry": "single_grounded_prism",
    }
    return parent, resolved, origins, directions, profile


def ensemble_visibility(parent, origins, directions, stops, profile):
    cells = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
    points = origins + stops[:, None] * directions
    points[:, 2] += 0.001
    light, _, limit = light_at(points, profile)
    numerator, denominator = np.zeros(len(origins)), np.zeros(len(origins))
    probabilities = np.array([0.15, 0.85 * 0.7, 0.85 * 0.3])
    for state in itertools.product(range(3), repeat=4):
        weight = np.prod(probabilities[list(state)])
        heights = np.array([0, 6, 12])[list(state)]
        boxes = np.zeros((4, 2, 3))
        boxes[:, 0, :2], boxes[:, 1, :2] = (cells + 0.125) * 16, (cells + 0.875) * 16
        boxes[:, 1, 2] = heights
        boxes = boxes[heights > 0]
        miss, visible = np.ones(len(origins), bool), np.ones(len(origins), bool)
        if len(boxes):
            depth = brute_hit(boxes, origins, directions, 0.1)[0]
            miss = depth >= stops
            shadow = brute_hit(boxes, points, light, 0)[0]
            visible = ~np.isfinite(shadow) | (shadow >= limit)
        denominator += weight * miss
        numerator += weight * miss * visible
    return numerator / denominator


class CameraMissTest(unittest.TestCase):
    def test_known_empty_cell_matches_the_same_fully_ready_scene(self):
        parent, empty, o, d, profile = empty_field()
        whole = np.array([[[0, 0], [1, 1]]])
        old, _ = mixed(
            parent,
            o,
            d,
            profile,
            empty,
            None,
            whole,
            None,
            False,
            receiver_ownership=True,
        )
        actual, work = mixed(
            parent,
            o,
            d,
            profile,
            empty,
            None,
            whole,
            None,
            False,
            receiver_ownership=True,
            camera_miss_witness=True,
        )
        ready, _ = mixed(parent, o, d, profile, empty, empty, whole, whole, False)
        np.testing.assert_array_equal(actual, ready)
        self.assertGreater(float(np.max(abs(old - actual))), 0)
        self.assertEqual(work["parent_work"]["camera_miss_witness_rays"], 1)
        self.assertGreater(
            work["parent_work"]["shadow_camera_miss_conditioned_cells"], 0
        )
        untyped = empty.copy()
        del untyped["column_geometry"]
        unproved, _ = mixed(
            parent,
            o,
            d,
            profile,
            untyped,
            None,
            whole,
            None,
            False,
            camera_miss_witness=True,
        )
        np.testing.assert_array_equal(unproved, old)

    def test_81_world_conditional_shadow_reference(self):
        parent, _, _, _, profile = empty_field()
        parent.frame[3] = 32
        parent.centre_fractions = np.ones((2, 2, 3))
        o = np.array([[8, 8, 50], [1, 8, 50], [24, 8, 50], [8, -8, 50]], float)
        d = np.array([[0, 0, -1], [0, 0, -1], [0, 0, -1], [0, 0.5, -1]], float)
        stops = -o[:, 2] / d[:, 2]
        profile["direction"] = [1, 0.2, 0.3]
        whole = np.array([[[0, 0], [2, 2]]])
        c, t, s, _ = render_layer(
            parent,
            o,
            d,
            profile,
            stops,
            primary_covered=whole,
            camera_miss_witness=True,
        )
        expected = ensemble_visibility(parent, o, d, stops, profile)
        np.testing.assert_allclose(s, expected, rtol=0, atol=3e-14)
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)

    def test_ground_in_lot_gap_does_not_make_the_cell_empty(self):
        parent, _, o, d, profile = empty_field()
        o[:, 0] = 1
        profile["direction"] = [1, 0, 0.2]
        whole = np.array([[[0, 0], [1, 1]]])
        old = render_layer(
            parent, o, d, profile, np.array([50.0]), primary_covered=whole
        )[2]
        new = render_layer(
            parent,
            o,
            d,
            profile,
            np.array([50.0]),
            primary_covered=whole,
            camera_miss_witness=True,
        )[2]
        np.testing.assert_array_equal(old, new)
        self.assertLess(float(new[0]), 1)

    def test_open_receiver_endpoint_retains_its_equal_height(self):
        parent, _, o, d, _ = empty_field()
        product = grid(parent) | {
            "primary_covered": np.array([[[0, 0], [1, 1]]]),
            "camera_miss_witness": True,
        }
        work = {"primary_condition_tests": 0}
        cap = primary_cap(
            product, (o, d, np.array([44.0])), np.array([0]), np.array([[0, 0]]), work
        )
        self.assertGreater(cap[0], 6)
        self.assertLess(cap[0], 12)

    def test_sampled_and_forced_run_shadow_consumers_keep_the_prefix(self):
        parent, _, o, d, profile = empty_field()
        whole = np.array([[[0, 0], [1, 1]]])
        for query in [None, RowRunShadows(parent, force=True)]:
            c, t, s, _ = render_sampled_layer(
                parent,
                o,
                d,
                profile,
                np.array([50.0]),
                primary_covered=whole,
                camera_miss_witness=True,
                shadow_query=query,
            )
            np.testing.assert_array_equal(c, 0)
            np.testing.assert_array_equal(t, 1)
            np.testing.assert_array_equal(s, 1)

    def test_complete_matching_geometry_is_required_not_readiness_alone(self):
        parent, resolved, _, _, _ = empty_field()
        product, whole = grid(parent), np.array([[[0, 0], [1, 1]]])
        self.assertTrue(has_camera_miss_witness(product, resolved, whole))
        self.assertFalse(
            has_camera_miss_witness(product, resolved, np.array([[[0, 0], [2, 1]]]))
        )
        self.assertFalse(
            has_camera_miss_witness(
                product, resolved | {"column_geometry": "arbitrary"}, whole
            )
        )
        self.assertFalse(
            has_camera_miss_witness(
                product, resolved | {"reference_origin_m": np.array([0.0, 0, 4])}, whole
            )
        )
        self.assertFalse(
            has_camera_miss_witness(product, resolved | {"fallback": {}}, whole)
        )
        self.assertFalse(
            has_camera_miss_witness(
                product,
                resolved | {"header": resolved["header"] | {"footprint_fraction": 0.5}},
                whole,
            )
        )
        self.assertFalse(has_camera_miss_witness(product, resolved, None))

    def test_explicit_witness_rejects_unqueried_or_undeclared_prefixes(self):
        parent, _, o, d, profile = empty_field()
        whole = np.array([[[0, 0], [1, 1]]])
        with self.assertRaises(ValueError):
            render_layer(
                parent,
                o,
                d,
                profile,
                None,
                primary_covered=whole,
                camera_miss_witness=True,
            )
        with self.assertRaises(ValueError):
            render_layer(
                parent, o, d, profile, np.array([50.0]), camera_miss_witness=True
            )
        with self.assertRaises(ValueError):
            render_layer(
                parent,
                o,
                d,
                profile,
                np.array([50.0]),
                primary_covered=whole,
                camera_miss_witness=1,
            )

    def test_packets_and_inputs_remain_independent_of_witness_transport(self):
        parent, _, o, d, profile = empty_field()
        o = np.repeat(o, 7, axis=0)
        d = np.repeat(d, 7, axis=0)
        whole = np.array([[[0, 0], [1, 1]]])
        before = {k: a.tobytes() for k, a in parent.arrays().items()}
        old_o, old_d = o.copy(), d.copy()
        a = render_layer(
            parent,
            o,
            d,
            profile,
            np.full(7, 50.0),
            primary_covered=whole,
            camera_miss_witness=True,
            batch=2,
        )
        b = render_layer(
            parent,
            o,
            d,
            profile,
            np.full(7, 50.0),
            primary_covered=whole,
            camera_miss_witness=True,
            batch=7,
        )
        for x, y in zip(a[:3], b[:3], strict=True):
            np.testing.assert_array_equal(x, y)
        self.assertEqual(before, {k: a.tobytes() for k, a in parent.arrays().items()})
        np.testing.assert_array_equal(o, old_o)
        np.testing.assert_array_equal(d, old_d)


if __name__ == "__main__":
    unittest.main()

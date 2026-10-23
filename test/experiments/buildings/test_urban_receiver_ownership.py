"""Independent convex-box shadows and conservative actual-hit owner proofs."""

import unittest

import numpy as np
from compare_urban_layers import mixed
from test_urban_parent_conditional import brute_hit
from test_urban_ready_pixels import fixture
from urban_opaque_receivers import known_receiver_cells, opaque_receivers
from urban_parent_conditional import grid
from urban_parent_layer import render_layer
from urban_parent_light import light_at
from urban_parent_stats import UrbanParent
from urban_sampled_layer import render_sampled_layer


def one_box():
    parent = UrbanParent(
        np.ones((1, 1, 3)),
        np.array([0, 0, 1, 16, 16, 0.75, 0.15]),
        np.array([6.0, 12.0]),
        np.array([0.7, 0.3]),
    )
    resolved = {
        "columns": np.array([[0, 0]], np.int32),
        "spans": np.array([[0.0, 6.0]]),
        "offsets": np.array([0, 1], np.uint32),
        "lookup": np.array([[0]], np.int32),
        "low": np.array([0, 0]),
        "basis": np.eye(3),
        "shape": np.array([1, 1]),
        "height_bounds": np.array([0.0, 6.0]),
        "reference_origin_m": np.zeros(3),
        "header": {"pitch_m": 16.0, "footprint_fraction": 0.75},
    }
    origins = np.array([[8.0, 8.0, 50.0]])
    directions = np.array([[0.0, 0.0, -1.0]])
    profile = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3, "ambient": 0.1}
    return parent, resolved, origins, directions, profile


class ReceiverOwnershipTest(unittest.TestCase):
    def test_known_short_roof_cannot_be_shadowed_by_its_taller_prior(self):
        parent, resolved, o, d, profile = one_box()
        whole = np.array([[[0, 0], [1, 1]]])
        args = {
            "receiver_normals": np.array([[0.0, 0.0, 1.0]]),
            "primary_covered": whole,
        }
        old = render_layer(parent, o, d, profile, np.array([44.0]), **args)
        new = render_layer(
            parent,
            o,
            d,
            profile,
            np.array([44.0]),
            receiver_cells=np.array([[0, 0]]),
            **args,
        )
        point = np.array([[8.0, 8.0, 6.001]])
        light = light_at(point, profile)[0]
        depth = brute_hit(np.array([[[2, 2, 0], [14, 14, 6]]]), point, light, 0)[0]
        np.testing.assert_array_equal(np.isfinite(depth), False)
        self.assertEqual(old[2][0], 0.745)
        np.testing.assert_array_equal(new[2], 1)
        np.testing.assert_array_equal(new[0], old[0])
        np.testing.assert_array_equal(new[1], old[1])
        self.assertEqual(new[3]["receiver_owner_rays"], 1)
        self.assertEqual(new[3]["receiver_owner_input_peak_bytes"], 16)
        generic = render_layer(parent, o, d, profile, np.array([44.0]), **args)
        np.testing.assert_array_equal(generic[2], old[2])
        old_rgb, _ = mixed(parent, o, d, profile, resolved, None, whole, None, False)
        new_rgb, _ = mixed(
            parent,
            o,
            d,
            profile,
            resolved,
            None,
            whole,
            None,
            False,
            receiver_ownership=True,
        )
        ready_rgb, _ = mixed(
            parent, o, d, profile, resolved, resolved, whole, whole, False
        )
        np.testing.assert_array_equal(new_rgb, ready_rgb)
        self.assertGreater(float(np.max(abs(old_rgb - new_rgb))), 0)

    def test_neighbor_shadow_remains(self):
        parent, _resolved, o, d, profile = one_box()
        parent.frame[3], parent.frame[6] = 32, 0
        parent.centre_fractions = np.ones((2, 2, 3))
        parent.heights, parent.probabilities = np.array([12.0]), np.array([1.0])
        profile["direction"] = [1, 0, 0.2]
        whole = np.array([[[0, 0], [2, 2]]])
        _c, _t, s, _ = render_layer(
            parent,
            o,
            d,
            profile,
            np.array([44.0]),
            receiver_normals=np.array([[0.0, 0.0, 1.0]]),
            primary_covered=whole,
            receiver_cells=np.array([[0, 0]]),
        )
        point = np.array([[8.0, 8.0, 6.001]])
        light = light_at(point, profile)[0]
        depth = brute_hit(np.array([[[18, 2, 0], [30, 14, 12]]]), point, light, 0)[0]
        self.assertTrue(np.isfinite(depth[0]))
        np.testing.assert_array_equal(s, 0)

    def test_actual_trace_proves_owners_and_foreground_clears_them(self):
        parent, resolved, o, d, _profile = one_box()
        stop, _normal, _, owners, _ = opaque_receivers(
            parent, o, d, resolved, False, 1, ownership=True
        )
        np.testing.assert_array_equal(owners, [[0, 0]])
        np.testing.assert_array_equal(stop, [44])
        o[:, 2] = 200
        stop, _normal, _, owners, _ = opaque_receivers(
            parent, o, d, resolved, True, 1, ownership=True
        )
        np.testing.assert_array_equal(stop, [80])
        self.assertIsNone(owners)
        self.assertIsNone(
            opaque_receivers(parent, o, d, None, False, 1, ownership=True)[3]
        )

    def test_unproven_layouts_and_fallbacks_never_supply_owners(self):
        parent, resolved, _, _, _ = one_box()
        pg = grid(parent)

        def proof(product, owners=None):
            owners = np.array([0]) if owners is None else owners
            return known_receiver_cells(pg, product, owners, np.array([True]))

        self.assertIsNone(proof(resolved, np.array([-2])))
        self.assertIsNone(
            proof(resolved | {"header": resolved["header"] | {"pitch_m": 15}})
        )
        self.assertIsNone(
            proof(
                resolved | {"header": resolved["header"] | {"footprint_fraction": 0.5}}
            )
        )
        self.assertIsNone(
            proof(resolved | {"reference_origin_m": np.array([1.0, 0, 0])})
        )
        self.assertIsNone(proof(resolved | {"spans": np.array([[1.0, 6.0]])}))
        self.assertIsNone(
            proof(
                resolved
                | {
                    "spans": np.array([[0.0, 6.0], [9, 12]]),
                    "offsets": np.array([0, 2]),
                }
            )
        )

    def test_vertical_origin_and_basis_are_part_of_the_owner_proof(self):
        parent, resolved, _, _, _ = one_box()
        product = grid(parent)
        owners, hits = np.array([0]), np.array([True])
        raised = resolved | {"reference_origin_m": np.array([0.0, 0, 4])}
        self.assertIsNone(known_receiver_cells(product, raised, owners, hits))
        self.assertIsNone(
            known_receiver_cells(
                product, resolved | {"basis": np.diag([-1.0, -1, 1])}, owners, hits
            )
        )
        matched = product | {"reference_origin_m": np.array([0.0, 0, 4])}
        np.testing.assert_array_equal(
            known_receiver_cells(matched, raised, owners, hits), [[0, 0]]
        )

    def test_explicit_invalid_owner_contracts_fail(self):
        parent, _, o, d, profile = one_box()
        whole = np.array([[[0, 0], [1, 1]]])
        args = {
            "receiver_normals": np.array([[0.0, 0.0, 1.0]]),
            "primary_covered": whole,
        }
        for owners in [
            np.array([[0.0, 0.0]]),
            np.array([[0, -1]]),
            np.array([[1, 0]]),
            np.array([0, 0]),
        ]:
            with self.subTest(owners=owners), self.assertRaises(ValueError):
                render_layer(
                    parent,
                    o,
                    d,
                    profile,
                    np.array([44.0]),
                    receiver_cells=owners,
                    **args,
                )
        with self.assertRaises(ValueError):
            render_layer(
                parent,
                o,
                d,
                profile,
                np.array([np.inf]),
                receiver_cells=np.array([[0, 0]]),
                **args,
            )
        with self.assertRaises(ValueError):
            render_layer(
                parent,
                o,
                d,
                profile,
                np.array([44.0]),
                receiver_normals=args["receiver_normals"],
                receiver_cells=np.array([[0, 0]]),
            )

    def test_sampled_consumer_preserves_the_same_receiver_fact(self):
        parent, _, o, d, profile = one_box()
        whole = np.array([[[0, 0], [1, 1]]])
        args = {
            "receiver_normals": np.array([[0.0, 0.0, 1.0]]),
            "primary_covered": whole,
            "receiver_cells": np.array([[0, 0]]),
        }
        actual = render_sampled_layer(
            parent, o, d, profile, np.array([44.0]), samples=4, **args
        )
        np.testing.assert_array_equal(actual[0], 0)
        np.testing.assert_array_equal(actual[1], 1)
        np.testing.assert_array_equal(actual[2], 1)

    def test_batch_invariance_and_child_arrays_are_immutable(self):
        parent, resolved, view, profile, _args = fixture()
        from triangle_image_reference import image_rays

        o, d = image_rays(view, np.array([[637, 357], [641, 361]]), 4, 4, 2)
        d /= np.linalg.norm(d, axis=1)[:, None]
        before = {
            k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)
        }
        whole = np.array([[[0, 0], grid(parent)["shape"]]])
        a = mixed(
            parent,
            o,
            d,
            profile,
            resolved,
            None,
            whole,
            None,
            False,
            receiver_ownership=True,
        )[0]
        b = np.concatenate(
            [
                mixed(
                    parent,
                    o[i : i + 7],
                    d[i : i + 7],
                    profile,
                    resolved,
                    None,
                    whole,
                    None,
                    False,
                    receiver_ownership=True,
                )[0]
                for i in range(0, len(o), 7)
            ]
        )
        np.testing.assert_allclose(a, b, rtol=0, atol=2e-14)
        self.assertEqual(
            before,
            {k: a.tobytes() for k, a in resolved.items() if isinstance(a, np.ndarray)},
        )


if __name__ == "__main__":
    unittest.main()

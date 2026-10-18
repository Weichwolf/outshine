"""Exact source/material lineage and no full near expansion before connected demand."""

import unittest
from unittest.mock import patch

import numpy as np
from connected_catalog import ConnectedCatalog
from spatial_source_checks import fixture


class ConnectedDemand(unittest.TestCase):
    def test_refusal_and_summary_do_not_generate_poses_or_templates(self):
        s = fixture()
        c = ConnectedCatalog(s)
        with (
            patch(
                "connected_catalog.build_connected",
                side_effect=AssertionError("prototype"),
            ),
            patch(
                "outshine_ref.vegetation.Shoot.detail",
                side_effect=AssertionError("poses"),
            ),
        ):
            self.assertIsNone(c.group(int(s.crown.hosts[0]), 0))
            self.assertEqual(c.summary()["prepared_variants"], 0)
        self.assertFalse(s.crown._groups)

    def test_original_ids_variants_anchors_and_area_survive_reordered_demand(self):
        s = fixture()
        c = ConnectedCatalog(s)
        nodes = list(map(int, s.crown.hosts))
        first = {n: c.group(n) for n in nodes}
        for n in nodes[::-1]:
            g = c.group(n)
            for k, a in g["poses"].items():
                np.testing.assert_array_equal(a, first[n]["poses"][k])
            np.testing.assert_array_equal(
                g["physical_leaf_area_m2"],
                [s.blades[int(v)].area() for v in g["poses"]["variants"]],
            )
            self.assertEqual(len(s.crown._groups), 1)
            self.assertEqual(len(g["physical_leaf_area_m2"]), len(g["poses"]["ids"]))
            self.assertEqual(g["material_inputs_json"], s.material_json)
        self.assertEqual(c.summary()["source_triangles_built"], 272)

    def test_draw_surfaces_share_maps_and_keep_exact_original_root(self):
        s = fixture()
        c = ConnectedCatalog(s)
        n = int(s.crown.hosts[-1])
        g = c.group(n)
        origin, frame = s.crown.stem(n)
        for i, d in enumerate(c.surfaces(g)):
            v = int(g["poses"]["variants"][i])
            p = c.prototypes[v]
            self.assertTrue(np.shares_memory(d["colour"], p["colour"]))
            self.assertEqual(int(d["owner"]), int(g["poses"]["ids"][i]))
            expected = (
                p["mesh"]["vertices"][1] @ g["poses"]["frames"][i].T
                + g["poses"]["origins"][i]
            )
            np.testing.assert_allclose(
                d["mesh"]["vertices"][1] @ frame.T + origin,
                expected,
                rtol=0,
                atol=2e-15,
            )
            np.testing.assert_allclose(
                d["basis"], frame.T @ g["poses"]["frames"][i], rtol=0, atol=1e-15
            )

    def test_group_avoids_fused_source_geometry_and_whole_pose_tables(self):
        s = fixture()
        c = ConnectedCatalog(s)
        with (
            patch.object(s, "geometry", side_effect=AssertionError("fused source")),
            patch(
                "crown_shoots.CrownShoots.tables",
                side_effect=AssertionError("whole table"),
            ),
        ):
            g = c.group(int(s.crown.hosts[0]))
        self.assertEqual(len(c.surfaces(g)), len(g["poses"]["ids"]))
        self.assertFalse(s.prototypes)

    def test_representation_identity_binds_shape_size_and_original_material(self):
        s = fixture()
        a = ConnectedCatalog(s)
        b = ConnectedCatalog(s, size=16)
        self.assertNotEqual(a.identity, b.identity)
        from spatial_group_source import SpatialGroupSource

        altered = ConnectedCatalog(
            SpatialGroupSource(s.crown, s.blades, {"material": "changed"})
        )
        self.assertNotEqual(a.identity, altered.identity)
        self.assertEqual(a.identity, ConnectedCatalog(fixture()).identity)


if __name__ == "__main__":
    unittest.main()

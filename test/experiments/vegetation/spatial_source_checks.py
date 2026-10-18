"""Budget/identity controls on the actual host-to-original-leaf source contract."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from botanical_profiles import plant_recipe
from crown_phase_catalog import CrownPhaseCatalog
from crown_shoots import CrownShoots
from spatial_group_source import SpatialGroupSource


def fixture():
    points = np.array(
        [[0, 0, 0], [0, 0, 1], [0.2, 0, 2], [-0.2, 0, 2], [0.1, 0.1, 3]], float
    )
    parents = np.array([-1, 0, 1, 1, 2], np.int32)
    variants = np.arange(84, dtype=np.uint8) % 8
    crown = CrownShoots(
        points, parents, variants, 682, length_m=0.35, bend_rad=0.4, foliage_depth_m=1.1
    )
    return SpatialGroupSource(crown, plant_recipe("quercus", 682).blades)


class SpatialSource(unittest.TestCase):
    def test_refusal_and_summary_create_no_poses_or_meshes(self):
        s = fixture()
        with (
            patch(
                "outshine_ref.vegetation.Shoot.detail",
                side_effect=AssertionError("poses before demand"),
            ),
            patch(
                "outshine_ref.vegetation.Blade.mesh",
                side_effect=AssertionError("mesh before demand"),
            ),
        ):
            s.summary()
            self.assertIsNone(s.geometry(int(s.crown.hosts[0]), 0))
            catalog = CrownPhaseCatalog(s, 8)
            self.assertIsNone(catalog.geometry(int(s.crown.hosts[0]), 0))
        self.assertFalse(s.prototypes)
        self.assertFalse(s.crown._groups)

    def test_source_order_and_global_ids_are_exact(self):
        s = fixture()
        nodes = list(map(int, s.crown.hosts))
        first = {n: s.geometry(n) for n in nodes}
        for n in nodes[::-1]:
            result = s.geometry(n)
            for key in (
                "vertices",
                "faces",
                "normals",
                "ids",
                "leaf_variants",
                "physical_leaf_area_m2",
            ):
                np.testing.assert_array_equal(result[key], first[n][key])
            self.assertEqual(len(s.crown._groups), 1)
        np.testing.assert_array_equal(
            np.concatenate([first[n]["ids"] for n in nodes]), np.arange(84)
        )

    def test_exact_anchor_and_saved_group_contract(self):
        s = fixture()
        n = int(s.crown.hosts[-1])
        data = s.geometry(n)
        group = s.crown.group(n, 128)
        np.testing.assert_array_equal(data["ids"], group["ids"])
        np.testing.assert_array_equal(data["leaf_variants"], group["variants"])
        np.testing.assert_allclose(
            data["organ_anchors"] @ data["anchor_frame"].T + data["anchor_origin"],
            group["nodes"],
            rtol=0,
            atol=1e-14,
        )

    def test_shape_changes_bind_source_and_phase_catalog_identities(self):
        s = fixture()
        a = CrownPhaseCatalog(s)
        changed = SpatialGroupSource(
            s.crown,
            (replace(s.blades[0], width=s.blades[0].width * 0.9), *s.blades[1:]),
        )
        b = CrownPhaseCatalog(changed)
        self.assertNotEqual(s.identity, changed.identity)
        self.assertNotEqual(a.identity, b.identity)

    def test_material_inputs_are_preserved_without_claiming_group_response(self):
        s = fixture()
        material = {"recipe": {"translucent_mix": 0.12}, "scope": "qualitative"}
        original = SpatialGroupSource(s.crown, s.blades, material)
        before = original.identity
        material["recipe"]["translucent_mix"] = 0.4
        changed = SpatialGroupSource(s.crown, s.blades, material)
        self.assertNotEqual(before, changed.identity)
        self.assertEqual(
            original.summary()["material_inputs"]["recipe"]["translucent_mix"], 0.12
        )
        self.assertEqual(original.identity, before)
        self.assertIn(
            "qualitative",
            original.geometry(int(s.crown.hosts[0]))["material_inputs_json"],
        )

    def test_catalog_capacity_refuses_before_any_source_geometry(self):
        s = fixture()
        with (
            patch.object(s, "geometry", side_effect=AssertionError("source expansion")),
            self.assertRaises(ValueError),
        ):
            CrownPhaseCatalog(s, max_templates=0)
        self.assertFalse(s.prototypes)
        self.assertFalse(s.crown._groups)

    def test_catalog_is_deterministic_and_rebinds_original_ids(self):
        s = fixture()
        a = CrownPhaseCatalog(s, 8)
        b = CrownPhaseCatalog(fixture(), 8)
        self.assertEqual(a.identity, b.identity)
        leaves = []
        for n in map(int, s.crown.hosts[::-1]):
            result = a.geometry(n)
            i = s.crown._host_index(n)
            first, last = map(int, s.crown.offsets[i : i + 2])
            np.testing.assert_array_equal(result["ids"], np.arange(first, last))
            np.testing.assert_array_equal(
                result["leaf_variants"], s.crown.variants[first:last]
            )
            self.assertFalse(result["vertices"].flags.writeable)
            leaves.extend(result["ids"].tolist())
        self.assertEqual(sorted(leaves), list(range(84)))

    def test_shared_template_has_distinct_original_source_ids(self):
        positions = np.c_[np.zeros(64), np.zeros(64), np.arange(64) * 0.1]
        crown = CrownShoots(
            positions,
            np.arange(-1, 63, dtype=np.int32),
            np.arange(1071, dtype=np.uint32) % 8,
            682,
            length_m=0.35,
            bend_rad=0.4,
            foliage_depth_m=5.0,
        )
        s = SpatialGroupSource(crown, plant_recipe("quercus", 682).blades)
        c = CrownPhaseCatalog(s, 8)
        _, counts = np.unique(c.mapping, return_counts=True)
        self.assertGreater(counts.max(), 1)
        slot = int(np.flatnonzero(np.bincount(c.mapping) > 1)[0])
        members = np.flatnonzero(c.mapping == slot)[:2]
        a, b = [c.geometry(int(crown.hosts[i])) for i in members]
        self.assertTrue(np.shares_memory(a["vertices"], b["vertices"]))
        self.assertFalse(np.intersect1d(a["ids"], b["ids"]).size)
        np.testing.assert_array_equal(a["leaf_variants"], b["leaf_variants"])
        self.assertEqual(len(c.templates), 1)


if __name__ == "__main__":
    unittest.main()

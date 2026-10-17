"""Independent per-light membership union, immutable packets and actual Blender links."""

import unittest

import bpy
import numpy as np
from organ_preview import link_shadow_products
from window_portrait_roles import role_packets


class WindowRoleTests(unittest.TestCase):
    def test_complete_membership_against_independent_scalar_dictionary(self):
        rng = np.random.default_rng(630)
        selected = {name: rng.integers(0, 4, 203) for name in ("camera", "sun", "fill")}
        indices = np.arange(203)[::2]
        expected = set()
        for index in indices:
            expected.add((int(index), int(selected["camera"][index]), "camera"))
            for light in ("sun", "fill"):
                expected.add((int(index), int(selected[light][index]), light))
        actual, surfaces = set(), []
        for role, material, lights, members in role_packets(selected, indices):
            for index in members:
                view = int(selected[role][index])
                surfaces.append((int(index), view))
                if material in ("camera", "all"):
                    actual.add((int(index), view, "camera"))
                for light in lights:
                    actual.add((int(index), view, light))
            self.assertFalse(members.flags.writeable)
            with self.assertRaises(ValueError):
                members.setflags(write=True)
        self.assertEqual(actual, expected)
        self.assertEqual(len(surfaces), len(set(surfaces)))
        self.assertEqual(
            len(surfaces),
            sum(len({int(v[i]) for v in selected.values()}) for i in indices),
        )

    def test_single_shared_view_and_original_distinct_packets(self):
        same = {name: np.array([7]) for name in ("camera", "sun", "fill")}
        packets = role_packets(same, np.array([0]))
        self.assertEqual(len(packets), 1)
        self.assertEqual(packets[0][:3], ("camera", "all", ("sun", "fill")))
        distinct = {
            "camera": np.array([0]),
            "sun": np.array([1]),
            "fill": np.array([2]),
        }
        packets = role_packets(distinct, np.array([0]))
        self.assertEqual(
            [p[:3] for p in packets],
            [
                ("camera", "camera", ()),
                ("sun", "shadow", ("sun",)),
                ("fill", "shadow", ("fill",)),
            ],
        )

    def test_explicit_shared_camera_owners_do_not_leak_into_other_lights(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

        def obj(name, role, owners=None):
            mesh = bpy.data.meshes.new(name)
            item = bpy.data.objects.new(name, mesh)
            bpy.context.collection.objects.link(item)
            item["vegetation_pass_role"] = role
            if owners is not None:
                item["shadow_lights"] = {n: True for n in owners}
            return item

        obj("wood", "all")
        obj("camera-sun", "all", ("sun",))
        obj("shared-shadow", "shadow", ("sun", "fill"))
        legacy = obj("legacy-fill", "shadow")
        legacy["shadow_light"] = "fill"
        expected = {
            "sun": {"wood", "camera-sun", "shared-shadow"},
            "fill": {"wood", "shared-shadow", "legacy-fill"},
        }
        for name in ("sun", "fill"):
            bpy.ops.object.light_add(type="SUN")
            lamp = bpy.context.object
            link_shadow_products(lamp, name)
            self.assertEqual(
                {o.name for o in lamp.light_linking.blocker_collection.objects},
                expected[name],
            )

    def test_corrupt_or_oversized_demands_fail_without_geometry(self):
        selected = {"camera": np.array([0, 1])}
        for indices in (
            np.array([1, 0]),
            np.array([0, 0]),
            np.array([-1]),
            np.array([2]),
            np.zeros(80001, np.int64),
        ):
            with self.assertRaises(ValueError):
                role_packets(selected, indices)
        with self.assertRaises(ValueError):
            role_packets({"camera": np.array([64])}, np.array([0]))
        with self.assertRaisesRegex(ValueError, "window budget"):
            role_packets({"camera": np.zeros(80001, np.int64)}, np.array([], np.int64))
        self.assertEqual(role_packets(selected, np.array([], np.int64)), ())


if __name__ == "__main__":
    unittest.main()

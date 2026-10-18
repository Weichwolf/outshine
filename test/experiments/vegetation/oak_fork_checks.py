"""Regression controls for shared-axis ownership and untouched cached geometry."""

import unittest

import numpy as np
from limb_surface import limb_mesh
from oak_fork_probe import saved_limbs
from oak_fork_product import UNASSIGNED, replace_groups, requested_groups


def fixture():
    positions = np.array(
        [[0, 0, 0], [0, 0, 1], [0, 0, 2], [0, 0, 3], [1, 0, 2], [-1, 0, 3], [0, 1, 2]],
        float,
    )
    radii = np.array([0.1, 0.08, 0.06, 0.02, 0.03, 0.02, 0.015])
    paths = [(0, 1, 2, 3), (1, 4), (2, 5), (1, 6)]
    meshes = [limb_mesh(positions, radii, p) for p in paths]
    offset, faces = 0, []
    for mesh in meshes:
        faces.append(mesh["faces"] + offset)
        offset += len(mesh["vertices"])
    wood = {
        "vertices": np.concatenate([m["vertices"] for m in meshes]),
        "faces": np.concatenate(faces),
        "normals": np.concatenate([m["normals"] for m in meshes]),
        "limb_ids": np.concatenate(
            [
                np.full(len(m["faces"]), p[1], np.uint32)
                for m, p in zip(meshes, paths, strict=True)
            ]
        ),
    }
    wood["surface"] = np.zeros(len(wood["faces"]), np.uint8)
    wood["corner_uv"] = np.zeros((3 * len(wood["faces"]), 2))
    return wood, paths, saved_limbs(wood, paths)


class ForkOwnership(unittest.TestCase):
    def test_shared_parent_is_solved_once_in_either_order(self):
        _, paths, limbs = fixture()
        groups = requested_groups(limbs, paths, (1, 2))
        self.assertEqual(len(groups), 1)
        nodes, sources, owners = groups[0]
        self.assertEqual(nodes, (1, 2))
        self.assertEqual(owners, [1, 4, 5, 6])
        self.assertEqual([s["source_node"] for s in sources], owners)
        for a, b in zip(
            sources, requested_groups(limbs, paths, (2, 1))[0][1], strict=True
        ):
            for field in ("vertices", "faces", "normals"):
                np.testing.assert_array_equal(a[field], b[field])
            self.assertEqual(a["forbidden_caps"], b["forbidden_caps"])
        self.assertEqual(sources[0]["forbidden_caps"], ())
        self.assertTrue(all(len(s["forbidden_caps"]) == 8 for s in sources[1:]))

    def test_bad_or_excess_demand_is_refused(self):
        _, paths, limbs = fixture()
        for nodes in ((0,), (1, 1), (3,), (-1,), (True,), tuple(range(1, 10))):
            with self.subTest(nodes=nodes), self.assertRaises(ValueError):
                requested_groups(limbs, paths, nodes)

    def test_saved_ownership_mismatch_fails(self):
        wood, paths, _ = fixture()
        wood["limb_ids"][0] = 5
        with self.assertRaises(AssertionError):
            saved_limbs(wood, paths)

    def test_no_demand_returns_original_without_repacking(self):
        wood, _, _ = fixture()
        actual, work = replace_groups(wood, [])
        self.assertIs(actual, wood)
        self.assertEqual(work["copied_array_bytes"], 0)

    def test_unrequested_triangle_positions_attributes_and_shading_are_exact(self):
        wood, _, limbs = fixture()
        patch = limbs[5] | {
            "source_kind": np.zeros(len(limbs[5]["faces"]), np.uint8),
            "source_node": np.full(len(limbs[5]["faces"]), 5, np.uint32),
        }
        actual, _ = replace_groups(wood, [(2, patch, [5])])
        old = wood["limb_ids"] != 5
        new = actual["patch_nodes"] == UNASSIGNED
        np.testing.assert_array_equal(
            wood["vertices"][wood["faces"][old]],
            actual["vertices"][actual["faces"][new]],
        )
        for field, corners in (("normals", 3), ("corner_uv", 2)):
            np.testing.assert_array_equal(
                wood[field].reshape(-1, 3, corners)[old],
                actual[field].reshape(-1, 3, corners)[new],
            )
        for field in ("surface", "limb_ids"):
            np.testing.assert_array_equal(wood[field][old], actual[field][new])
        self.assertEqual(set(actual["patch_nodes"]), {2, UNASSIGNED})

    def test_duplicate_owners_and_unsupported_material_are_refused(self):
        wood, _, limbs = fixture()
        with self.assertRaises(ValueError):
            replace_groups(wood, [(1, limbs[4], [4]), (2, limbs[5], [4])])
        wood["corner_uv"][0, 0] = 1
        with self.assertRaises(ValueError):
            replace_groups(wood, [(1, limbs[4], [4])])


if __name__ == "__main__":
    unittest.main()
